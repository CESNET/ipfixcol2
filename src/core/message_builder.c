/**
 * \file message_builder.c
 * \author Adam Zvara <xzvara01@stud.fit.vutbr.cz>
 * \brief Creating new IPFIX messages based on existing message (source file)
 * \date 2023
 */

#include <ipfixcol2.h>
#include <libfds.h>
#include <stdlib.h>

#include "message_ipfix.h"

/** Size of offset array                                            */
#define IPX_BUILDER_OFFSET_CNT UINT8_MAX

/** Structure holding information about offset (and other things
    such as record reference for record references ...)             */
struct offset_item {
    // Actual offset
    uint32_t off;
    // Data record (is NOT used for set offsets)
    struct fds_drec *rec;
};

/**
 * \brief Structure storing offsets of referenced sets and records in IPFIX message
 *
 * Set and record references can become invalid when IPFIX wrapper is reallocated.
 * This structure keeps offsets of sets or records so they can be fixed to new
 * address after reallocation.
 */
struct offsets {
    /** Number of valid items in array      */
    size_t cnt_valid;
    /** Size allocated for array of offsets */
    size_t cnt_alloc;
    /** Array of offsets                    */
    struct offset_item offsets[];
};

/** \brief Message builder for creating new IPFIX messages              */
struct ipx_msg_builder {
    /** Pointer to raw ipfix message                                    */
    uint8_t *raw;
    /** Points after last added part (next free space) in raw message   */
    uint8_t *curr;

    struct {
        /** Maximum length of raw message                               */
        size_t max;
        /** Allocated length of raw message                             */
        size_t alloc;
        /** Current length of raw message                               */
        size_t message;
    } lengths;

    struct {
        /** Pointer to last created set                                 */
        struct fds_ipfix_set_hdr *prev_set;
        /** Lenght of last created set                                  */
        uint16_t set_length;
        /** Sets offsets                                                */
        struct offsets *sets_offsets;
        /** Records offsets                                             */
        struct offsets *records_offsets;
    } _private; // Internal structure
};

/**
 * \brief Create new offset structure
 *
 * \param[in] cnt Number of offset_items in offset structure
 * \return Allocated structure on success, otherwise NULL
 */
static struct offsets *
offsets_create(size_t cnt)
{
    struct offsets *arr = malloc(sizeof(*arr) + cnt * sizeof(struct offset_item));
    if (arr == NULL) {
        return NULL;
    }

    arr->cnt_valid = 0;
    arr->cnt_alloc = cnt;
    return arr;
}

/**
 * \brief Reallocate memory for offsets
 *
 * \param[in,out] off Pointer to offset_array
 * \return #IPX_OK on success, otherwise #IPX_ERR_NOMEM (memory allocation error)
 */
static int
offsets_realloc(struct offsets **off)
{
    const size_t new_alloc_size = (*off)->cnt_alloc * 2u;
    struct offsets *new_offset = realloc(*off, sizeof(**off) + new_alloc_size * sizeof(struct offset_item));
    if (new_offset == NULL) {
        return IPX_ERR_NOMEM;
    }

    *off = new_offset;
    (*off)->cnt_alloc = new_alloc_size;
    return IPX_OK;
}

/**
 * \brief Add new record offset item
 *
 * \param[in] offsets Pointer to offset array
 * \param[in] offset  Offset of data record (from beginning of the message)
 * \param[in] rec     Data record
 * \return #IPX_OK on success
 * \return #IPX_ERR_ARG on NULL argument
 * \return #IPX_ERR_NOMEM on memory allocation error
 */
static int
offsets_add_rec(struct offsets **offsets, const size_t offset, const struct fds_drec *rec)
{
    struct offsets *off = *offsets;

    if (off == NULL || rec == NULL) {
        return IPX_ERR_ARG;
    }

    if (off->cnt_valid >= off->cnt_alloc) {
        if (offsets_realloc(offsets)) {
            return IPX_ERR_NOMEM;
        }
    }

    // Reset pointer in case it got reallocated
    off = *offsets;

    struct offset_item *new_item = &(off->offsets[off->cnt_valid]);
    new_item->off = offset;

    // Store copy of original record
    struct fds_drec *rec_cpy = malloc(sizeof(*rec_cpy));
    if (!rec_cpy) {
        return IPX_ERR_NOMEM;
    }
    rec_cpy->data = NULL; // not used
    rec_cpy->size = rec->size;
    rec_cpy->snap = rec->snap;
    rec_cpy->tmplt = rec->tmplt;
    new_item->rec = rec_cpy;
    off->cnt_valid++;

    return IPX_OK;
}

/**
 * \brief Append record offset item into offset array
 *
 * \param[in] offsets Pointer to offset array
 * \param[in] offset  Offset of set
 * \return #IPX_OK on success
 * \return #IPX_ERR_ARG on NULL argument
 * \return #IPX_ERR_NOMEM on memory allocation error
 */
static int
offsets_add_set(struct offsets **offsets, size_t offset)
{
    struct offsets *off = *offsets;

    if (off == NULL) {
        return IPX_ERR_ARG;
    }

    if (off->cnt_valid >= off->cnt_alloc) {
        int rc = offsets_realloc(offsets);
        if (rc != IPX_OK) {
            return rc;
        }
    }

    // Reset pointer in case of reallocation
    off = *offsets;

    struct offset_item *new_item = &(off->offsets[off->cnt_valid]);
    new_item->off = offset;
    // new_item->rec is NOT set and CAN NOT be used!
    off->cnt_valid++;

    return IPX_OK;
}

/**
 * \brief Clear content of offset array
 *
 * \param[in] off Pointer to offset array
 */
static inline void
offsets_clear(struct offsets *off)
{
    off->cnt_valid = 0;
}

/**
 * \brief Destroy offset array
 *
 * \param[in] off Pointer to offset array
 */
static inline void
offsets_destroy(struct offsets *off)
{
    free(off);
}

ipx_msg_builder_t *
ipx_msg_builder_create()
{
    ipx_msg_builder_t *builder = calloc(1, sizeof(*builder));
    if (builder == NULL) {
        return NULL;
    }

    // Allocate memory for set offsets
   builder->_private.sets_offsets = offsets_create(IPX_BUILDER_OFFSET_CNT);
    if (!builder->_private.sets_offsets) {
        free(builder);
        return NULL;
    }

    // Allocate memory for record offsets
    builder->_private.records_offsets = offsets_create(IPX_BUILDER_OFFSET_CNT);
    if (!builder->_private.records_offsets) {
        offsets_destroy(builder->_private.sets_offsets);
        free(builder);
        return NULL;
    }

    return builder;
}

void
ipx_msg_builder_destroy(ipx_msg_builder_t *builder)
{
    if (!builder) {
        return;
    }

    offsets_destroy(builder->_private.records_offsets);
    offsets_destroy(builder->_private.sets_offsets);
    // Raw packet is destroyed when calling ipx_msg_ipfix_destroy() on wrapper
    free(builder);
}

void
ipx_msg_builder_free_raw(ipx_msg_builder_t *builder)
{
    if (!builder) {
        return;
    }

    free(builder->raw);
}

size_t
ipx_msg_builder_get_maxlength(const ipx_msg_builder_t *builder)
{
    if (builder) {
        return builder->lengths.max;
    }
    return 0;
}

void
ipx_msg_builder_set_maxlength(ipx_msg_builder_t *builder, const size_t new_length)
{
    assert(new_length > FDS_IPFIX_MSG_HDR_LEN);

    if (builder) {
        builder->lengths.max = new_length;
    }
}

/**
 * \brief Reallocate memory used for raw message
 *
 * \param[in] builder Pointer to builder
 * \return #IPX_OK on success, otherwise #IPX_ERR_NOMEM
 */
static int
raw_msg_realloc(ipx_msg_builder_t *builder)
{
    size_t new_length = builder->lengths.alloc * 2u;
    uint8_t *new_msg = realloc(builder->raw, new_length);
    if (new_msg == NULL) {
        return IPX_ERR_NOMEM;
    }
    builder->lengths.alloc = new_length;

    // Update positions of pointers
    builder->_private.prev_set = (struct fds_ipfix_set_hdr *)
        (new_msg + ((uint8_t *) builder->_private.prev_set - builder->raw));
    builder->curr = builder->lengths.message + new_msg;
    builder->raw = new_msg;

    return IPX_OK;
}

int
ipx_msg_builder_start(ipx_msg_builder_t *builder, const struct fds_ipfix_msg_hdr *hdr,
    const uint32_t maxbytes, const uint32_t hints)
{
    if (!builder || !hdr || maxbytes < FDS_IPFIX_MSG_HDR_LEN) {
        return IPX_ERR_ARG;
    }

    // Allocate memory for new raw message
    builder->lengths.alloc = (hints == 0) ? maxbytes : hints;
    uint8_t *msg_data = malloc(builder->lengths.alloc);
    if (msg_data == NULL) {
        return IPX_ERR_NOMEM;
    }

    builder->raw = msg_data;
    builder->curr = msg_data + FDS_IPFIX_MSG_HDR_LEN;
    builder->lengths.max = maxbytes;
    builder->lengths.message = FDS_IPFIX_MSG_HDR_LEN;
    builder->_private.prev_set = NULL;
    builder->_private.set_length = 0;

    offsets_clear(builder->_private.records_offsets);
    offsets_clear(builder->_private.sets_offsets);

    // Copy original message header
    memcpy(msg_data, hdr, FDS_IPFIX_MSG_HDR_LEN);

    return IPX_OK;
}

int
ipx_msg_builder_add_set(ipx_msg_builder_t *builder, const uint16_t id)
{
    int rc;

    // Check if maximum length will be exceeded
    if (builder->lengths.message + FDS_IPFIX_MSG_HDR_LEN > builder->lengths.max) {
        return IPX_ERR_DENIED;
    }

    // Reallocate raw message if necessary
    if (builder->lengths.message + FDS_IPFIX_SET_HDR_LEN > builder->lengths.alloc) {
        if (raw_msg_realloc(builder)) {
            return IPX_ERR_NOMEM;
        }
    }

    // Store set offset
    assert(builder->_private.sets_offsets != NULL);
    rc = offsets_add_set(&(builder->_private.sets_offsets), builder->lengths.message);
    if (rc) {
        // Memory allocation error
        return rc;
    }

    // Create new set header
    builder->_private.prev_set = (struct fds_ipfix_set_hdr *) builder->curr;
    builder->_private.prev_set->flowset_id = htons(id);
    builder->_private.set_length = FDS_IPFIX_SET_HDR_LEN;
    builder->lengths.message += FDS_IPFIX_SET_HDR_LEN;

    builder->curr += FDS_IPFIX_SET_HDR_LEN;

    return IPX_OK;
}

int
ipx_msg_builder_add_drec(ipx_msg_builder_t *builder, const struct fds_drec *record)
{
    int rc;
    uint16_t set_number;

    // Get set number and add new set if necessary
    if (builder->_private.prev_set == NULL) {
        set_number = 0;
    } else {
        set_number = ntohs(*(uint16_t *)(builder->_private.prev_set));
    }

    if (set_number != record->tmplt->id) {
        rc = ipx_msg_builder_add_set(builder, record->tmplt->id);
        if (rc) {
            return rc;
        }
    }

    // Check if maximum length will be exceeded
    if (builder->lengths.message + record->size > builder->lengths.max) {
        return IPX_ERR_DENIED;
    }

    // Reallocate raw message if necessary
    if (builder->lengths.message + record->size > builder->lengths.alloc) {
        if (raw_msg_realloc(builder)) {
            return IPX_ERR_NOMEM;
        }
    }

    // Copy record data and size to raw message
    memcpy(builder->curr, record->data, record->size);

    // Store record offset
    rc = offsets_add_rec(&(builder->_private.records_offsets), builder->lengths.message, record);
    if (rc) {
        return rc;
    }

    builder->_private.set_length += record->size;
    builder->curr += record->size;
    builder->lengths.message += record->size;

    // Update set length
    if (builder->_private.prev_set != NULL) {
        builder->_private.prev_set->length = htons(builder->_private.set_length);
    }

    return IPX_OK;
}

ipx_msg_ipfix_t *
ipx_msg_builder_end(const ipx_msg_builder_t *builder, const ipx_ctx_t *plugin_ctx,
    const struct ipx_msg_ctx *msg_ctx)
{
    if (!builder || !plugin_ctx || !msg_ctx) {
        return NULL;
    }

    // Update message length
    struct fds_ipfix_msg_hdr *hdr = (struct fds_ipfix_msg_hdr *)builder->raw;
    hdr->length = htons(builder->lengths.message);

    // Create wrapper for IPFIX message
    ipx_msg_ipfix_t *new_msg = ipx_msg_ipfix_create(plugin_ctx, msg_ctx, builder->raw,
        builder->lengths.message);

    if (new_msg == NULL) {
        return NULL;
    }

    // Add set references
    struct ipx_ipfix_set *set_ref;
    struct offset_item *arr = builder->_private.sets_offsets->offsets;

    for (size_t i = 0; i < builder->_private.sets_offsets->cnt_valid; i++) {
        set_ref = ipx_msg_ipfix_add_set_ref(new_msg);
        if (!set_ref) {
            ipx_msg_ipfix_destroy(new_msg);
            return NULL;
        }
        set_ref->ptr = (struct fds_ipfix_set_hdr *) (builder->raw + arr[i].off);
    }

    // Add drec references
    struct ipx_ipfix_record *drec_ref;
    arr = builder->_private.records_offsets->offsets;

    for (size_t i = 0; i < builder->_private.records_offsets->cnt_valid; i++) {
        drec_ref = ipx_msg_ipfix_add_drec_ref(&new_msg);
        if (!drec_ref) {
            ipx_msg_ipfix_destroy(new_msg);
            return NULL;
        }
        drec_ref->rec.data = builder->raw + arr[i].off;
        drec_ref->rec.size = arr[i].rec->size;
        drec_ref->rec.tmplt = arr[i].rec->tmplt;
        drec_ref->rec.snap = arr[i].rec->snap;
        free(arr[i].rec);
    }

    return new_msg;
}
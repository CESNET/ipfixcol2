/**
 * @file src/core/netflow2ipfix/netflow9.c
 * @author Lukas Hutak <lukas.hutak@cesnet.cz>
 * @brief Converter from NetFlow v9 to IPFIX Message (source code)
 * @date 2018
 *
 * Copyright(c) 2019 CESNET z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdint.h>
#include <endian.h>
#include <assert.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <inttypes.h>

#include <ipfixcol2.h>
#include <libfds.h>
#include <libfds/ipfix_structs.h>
#include "netflow2ipfix.h"
#include "netflow_structs.h"
#include "netflow9_parsers.h"
#include "netflow9_templates.h"
#include "../message_ipfix.h"
#include "../verbose.h"

/// ID of the first incompatible NetFlow Information Element
#define NF_INCOMP_ID_MIN  128
/// IPFIX Enterprise Number for incompatible NetFlow IEs (128 <= ID <= 32767)
#define NF_INCOMP_EN_LOW  4294967294UL
/// IPFIX Enterprise Number for incompatible NetFlow IEs (32768 <= ID <= 65535)
#define NF_INCOMP_EN_HIGH 4294967295UL
/**
 * Maximum length of any IPFIX Message Set
 * @note
 *   The length represents maximum IPFIX Message size without IPFIX Message header and
 *   IPFIX Set header size
 */
#define MAX_SET_CONTENT_LEN \
    (UINT16_MAX - FDS_IPFIX_MSG_HDR_LEN - FDS_IPFIX_SET_HDR_LEN)

// Simple static asserts to prevent unexpected structure modifications!
static_assert(IPX_NF9_MSG_HDR_LEN == 20U, "NetFlow v9 Message header size is not valid!");
static_assert(IPX_NF9_SET_HDR_LEN == 4U,  "NetFlow v9 Set header size is not valid!");

/// Auxiliary conversion structure from NetFlow Options Field to IPFIX Inf. Element ID
struct nf2ipx_opts {
    /// (Source) NetFlow scope field type
    uint16_t nf_id;
    /// (Target) IPFIX Information Element ID
    uint16_t ipx_id;
    /// Maximal size of IPFIX IE
    uint16_t ipx_max_size;
};

/**
 * @brief Options conversion table
 *
 * Each records represents mapping from NetFlow Scope Field Type to IPFIX IE.
 * @remark Based on RFC3954, Section 6.1. and available IPFIX IE from IANA
 */
static const struct nf2ipx_opts nf2ipx_opts_table[] = {
    // "System"    -> iana:exportingProcessId
    {IPX_NF9_SCOPE_SYSTEM,     144U, 4U},
    // "Interface" -> iana:ingressInterface
    {IPX_NF9_SCOPE_INTERFACE,   10U, 4U},
    // "Line Card" -> iana:lineCardId
    {IPX_NF9_SCOPE_LINE_CARD,  141U, 4U},
    // "Cache"     -> ??? (maybe iana:meteringProcessId)
    // {IPX_NF9_SCOPE_CACHE, xxx, xxx},
    // "Template"  -> iana:templateId
    {IPX_NF9_SCOPE_TEMPLATE,   145U, 2U}
};

/// Number of record the the Options conversion table
#define NF2IPX_OPTS_TABLE_SIZE (sizeof(nf2ipx_opts_table) / sizeof(nf2ipx_opts_table[0]))

/// Auxiliary conversion structure from NetFlow Field to IPFIX Inf. Element ID
struct nf2ipx_data {
    struct {
        /// NetFlow Field identification
        uint16_t id;
        /**
         * Required size of the field
         * @note If the field doesn't have this size, conversion cannot be performed.
         */
        uint16_t size;
    } netflow; ///< NetFlow Field definition
    struct {
        /// New IPFIX IE ID
        uint16_t id;
        /// New IPFIX IE Enterprise Number
        uint32_t en;
        /// Size of converted field
        uint16_t size;
    } ipfix; ///< IPFIX Field Definition

    /// Conversion instruction
    struct nf2ipx_instr instr;
};

/**
 * @brief NetFlow Data conversion table
 *
 * Each field represents mapping from NetFlow v9 to IPFIX IE and conversion instruction.
 * Only incompatible fields that MUST be converted should go here.
 */
static const struct nf2ipx_data nf2ipx_data_table[] = {
    // Conversion from relative to absolute TS: "LAST_SWITCHED"  -> iana:flowEndMilliseconds
    {{IPX_NF9_IE_LAST_SWITCHED,  4U}, {153U, 0U, 8U}, {NF2IPX_ITYPE_TS, 8U}},
    // Conversion from relative to absolute TS: "FIRST_SWITCHED" -> iana:flowStartMilliseconds
    {{IPX_NF9_IE_FIRST_SWITCHED, 4U}, {152U, 0U, 8U}, {NF2IPX_ITYPE_TS, 8U}}
};

/// Number of record the the Data conversion table */
#define NF2IPX_DATA_TABLE_SIZE (sizeof(nf2ipx_data_table) / sizeof(nf2ipx_data_table[0]))

/// Internal converter structure
struct ipx_nf9_conv {
    /// Instance identification (only for log!)
    char *ident;
    /// Verbosity level
    enum ipx_verb_level vlevel;

    /// Sequence number of the next expected NetFlow Message
    uint32_t nf9_seq_next;
    /// Have we already processed at least one NetFlow message
    bool nf9_seq_valid;
    /// Sequence number of the next converted IPFIX Message
    uint32_t ipx_seq_next;

    struct {
        /// Pointer to newly generated IPFIX Message
        uint8_t *ipx_msg;
        /// Pointer to the first byte after committed memory
        uint8_t *write_ptr;
        /// Allocated size of the IPFIX Message
        size_t ipx_size_alloc;
        /// Used (i.e. filled) size of the IPFIX Message
        size_t ipx_size_used;
        /// Message context (Source, ODID, Stream)
        const struct ipx_msg_ctx *msg_ctx;
        /// Number of all processed NetFlow records (Data + Templates) during conversion
        uint16_t recs_processed;
        /// Number of converted and added "Data" NetFlow records into the IPFIX Message
        uint16_t drecs_converted;
    } data; ///< Data of currently converted messages

    /// Template lookup table - 2-level table  (256 x 256)
    struct tmplts_l1_table l1_table;
};

/**
 * @def CONV_ERROR
 * @brief Macro for printing an error message of a converter
 * @param[in] conv    Converter
 * @param[in] fmt     Format string (see manual page for "printf" family)
 * @param[in] ...     Variable number of arguments for the format string
 */
#define CONV_ERROR(conv, fmt, ...)                                                               \
    if ((conv)->vlevel >= IPX_VERB_ERROR) {                                                      \
        const struct ipx_msg_ctx *msg_ctx = (conv)->data.msg_ctx;                                \
        ipx_verb_print(IPX_VERB_ERROR, "ERROR: %s: [%s, ODID: %" PRIu32 "] " fmt "\n",           \
            (conv)->ident, msg_ctx->session->ident, msg_ctx->odid, ## __VA_ARGS__);              \
    }

/**
 * @def CONV_WARNING
 * @brief Macro for printing a warning message of a converter
 * @param[in] conv    Converter
 * @param[in] fmt     Format string (see manual page for "printf" family)
 * @param[in] ...     Variable number of arguments for the format string
 */
#define CONV_WARNING(conv, fmt, ...)                                                             \
    if ((conv)->vlevel >= IPX_VERB_WARNING) {                                                    \
        const struct ipx_msg_ctx *msg_ctx = (conv)->data.msg_ctx;                                \
        ipx_verb_print(IPX_VERB_WARNING, "WARNING: %s: [%s, ODID: %" PRIu32 "] " fmt "\n",       \
            (conv)->ident, msg_ctx->session->ident, msg_ctx->odid, ## __VA_ARGS__);              \
    }

/**
 * @def CONV_INFO
 * @brief Macro for printing an info message of a converter
 * @param[in] conv    Converter
 * @param[in] fmt     Format string (see manual page for "printf" family)
 * @param[in] ...     Variable number of arguments for the format string
 */
#define CONV_INFO(conv, fmt, ...)                                                                \
    if ((conv)->vlevel >= IPX_VERB_INFO) {                                                       \
        const struct ipx_msg_ctx *msg_ctx = (conv)->data.msg_ctx;                                \
        ipx_verb_print(IPX_VERB_INFO, "INFO: %s: [%s, ODID: %" PRIu32 "] " fmt "\n",             \
            (conv)->ident, msg_ctx->session->ident, msg_ctx->odid, ## __VA_ARGS__);              \
    }

/**
 * @def CONV_DEBUG
 * @brief Macro for printing a debug message of a converter
 * @param[in] conv    Converter
 * @param[in] fmt     Format string (see manual page for "printf" family)
 * @param[in] ...     Variable number of arguments for the format string
 */
#define CONV_DEBUG(conv, fmt, ...)                                                               \
    if ((conv)->vlevel >= IPX_VERB_DEBUG) {                                                      \
        const struct ipx_msg_ctx *msg_ctx = (conv)->data.msg_ctx;                                \
        ipx_verb_print(IPX_VERB_DEBUG, "DEBUG: %s: [%s, ODID: %" PRIu32 "] " fmt "\n",           \
            (conv)->ident, msg_ctx->session->ident, msg_ctx->odid, ## __VA_ARGS__);              \
    }


ipx_nf9_conv_t *
ipx_nf9_conv_init(const char *ident, enum ipx_verb_level vlevel)
{
    struct ipx_nf9_conv *res = calloc(1, sizeof(*res));
    if (!res) {
        return NULL;
    }

    nf9_tmplts_init(&res->l1_table);
    res->ident = strdup(ident);
    if (!res->ident) {
        free(res);
        return NULL;
    }

    res->vlevel = vlevel;
    return res;
}

void
ipx_nf9_conv_destroy(struct ipx_nf9_conv *conv)
{
    // Destroy templates and lookup tables
    nf9_tmplts_destroy(&conv->l1_table);

    // Destroy the main structure
    free(conv->ident);
    free(conv);
}

/**
 * @brief Initialize an internal memory buffer for a new IPFIX Message
 * @param[in] conv Converter internals
 */
static inline void
conv_mem_init(ipx_nf9_conv_t *conv)
{
    conv->data.ipx_msg = NULL;
    conv->data.write_ptr = NULL;
    conv->data.ipx_size_used = 0;
    conv->data.ipx_size_alloc = 0;
}

/**
 * @brief Destroy the internal memory buffer for a new IPFIX Message
 * @param conv Converter internals
 */
static inline void
conv_mem_destroy(ipx_nf9_conv_t *conv)
{
    free(conv->data.ipx_msg);
    conv->data.ipx_msg = NULL;
}

/**
 * @brief Release IPFIX Message from the
 * @warning Do not use any buffer manipulation functions after call, except conv_mem_init()
 * @param conv Converter internals
 * @return Pointer to the IPFIX Message
 */
static inline uint8_t *
conv_mem_release(ipx_nf9_conv_t *conv)
{
    uint8_t *tmp = conv->data.ipx_msg;
    conv->data.ipx_msg = NULL;
    return tmp;
}

/**
 * @brief Reserve a memory in the new IPFIX Message
 *
 * The function makes sure that at least @p size bytes of the memory after the pointer
 * (i.e. conv_mem_ptr_now()) will be prepared for user modification.
 *
 * @warning The buffer can be reallocated and a user MUST consider that all pointers
 *   to the IPFIX Message are not valid anymore!
 * @param[in] conv Converter internals
 * @param[in] size Required size (in bytes)
 * @return #IPX_OK on success
 * @return #IPX_ERR_NOMEM in case of memory allocation error
 */
static inline int
conv_mem_reserve(ipx_nf9_conv_t *conv, size_t size)
{
    if (conv->data.ipx_size_used + size <= conv->data.ipx_size_alloc) {
        // Nothing to do
        return IPX_OK;
    }

    // Reallocate - use multiples of 1024 to avoid too many reallocations
    size_t new_alloc = conv->data.ipx_size_used + size;
    new_alloc /= 1024U;
    new_alloc += 1U;
    new_alloc *= 1024U;
    uint8_t *new_msg = realloc(conv->data.ipx_msg, new_alloc * sizeof(uint8_t));
    if (!new_msg) {
        return IPX_ERR_NOMEM;
    }

    conv->data.ipx_msg = new_msg;
    conv->data.write_ptr = new_msg + conv->data.ipx_size_used;
    conv->data.ipx_size_alloc = new_alloc;
    return IPX_OK;
}

/**
 * @brief Commit a user defined part of the new IPFIX Message
 *
 * User defined size of the memory after the current position pointer (i.e. conv_mem_ptr_now()) will
 * be marked as filled and the pointer will be moved forward.
 * @param[in] conv Converter internals
 * @param[in] size Memory size to commit
 */
static inline void
conv_mem_commit(ipx_nf9_conv_t *conv, size_t size)
{
    conv->data.ipx_size_used += size;
    conv->data.write_ptr = conv->data.ipx_msg + conv->data.ipx_size_used;
    assert(conv->data.ipx_size_used <= conv->data.ipx_size_alloc);
}

/**
 * @brief Get a pointer into the new IPFIX Message after the last committed byte
 *
 * @warning
 *   Keep on mind, the returned pointer MUST be considered as invalid after calling
 *   conv_mem_reserve() function. There is a chance that the returned address doesn't point
 *   to the IPFIX Message anymore!
 * @param[in] conv Converter internals
 * @return Pointer
 */
void *
conv_mem_ptr_now(ipx_nf9_conv_t *conv)
{
    return (void *) (conv->data.write_ptr);
}

/**
 * @brief Get a pointer into the new IPFIX Message with a given offset
 *
 * @warning
 *   Keep on mind, the returned pointer MUST be considered as invalid after calling
 *   conv_mem_reserve() function. There is a chance that the returned address doesn't point
 *   to the IPFIX Message anymore!
 * @warning
 *   Accessing offsets after reserved memory is undefined!
 * @param[in] conv   Converter internals
 * @param[in] offset Offset from the start of the Message
 * @return Pointer
 */
void *
conv_mem_ptr_offset(ipx_nf9_conv_t *conv, size_t offset)
{
    return (void *) (conv->data.ipx_msg + offset);
}

/**
 * @brief Get offset of the "commit" pointer (represents memory area labeled as filled)
 * @param[in] conv Converter internals
 * @return Offset from the start of the Message
 */
static inline size_t
conv_mem_pos_get(ipx_nf9_conv_t *conv)
{
    return conv->data.ipx_size_used;
}

/**
 * @brief Move the "commit" pointer back to the specific place in the IPFIX Message
 *
 * Memory after a user given @p offset from the beginning of IPFIX Message is considered as
 * unspecified (not committed). User can move the pointer only back in the memory, to make sure
 * that there are no undefined regions. Typical usage is to "remove" already committed IPFIX
 * Message parts.
 *
 * @warning Setting position after reserved memory is undefined operation!
 * @param[in] conv   Converter internals
 * @param[in] offset Offset from the start of the Message
 */
static inline void
conv_mem_pos_set(ipx_nf9_conv_t *conv, size_t offset)
{
    conv->data.ipx_size_used = offset;
    conv->data.write_ptr = conv->data.ipx_msg + conv->data.ipx_size_used;
    assert(conv->data.ipx_size_used <= conv->data.ipx_size_alloc);
}

// -----------------------------------------------------------------------------------------

/**
 * @brief Get a conversion instruction for conversion from NetFlow to IPFIX Data Field
 * @param[in] nf_id NetFlow Field ID
 * @return Pointer to the instruction or NULL (no conversion required)
 */
static inline const struct nf2ipx_data *
conv_data_map(uint16_t nf_id)
{
    for (size_t i = 0; i < NF2IPX_DATA_TABLE_SIZE; ++i) {
        // Is there a mapping to different IPFIX Element?
        const struct nf2ipx_data *map_rec = &nf2ipx_data_table[i];
        if (map_rec->netflow.id != nf_id) {
            continue;
        }

        // Success
        return map_rec;
    }

    return NULL;
}

/**
 * @brief Get a conversion instruction for conversion from NetFlow to IPFIX Scope Field ID
 * @param[in] nf_id NetFlow Scope Field ID
 * @return Pointer to the instruction or NULL (unsupported scope field)
 */
static inline const struct nf2ipx_opts *
conv_opts_map(uint16_t nf_id)
{
    for (size_t i = 0; i < NF2IPX_OPTS_TABLE_SIZE; ++i) {
        const struct nf2ipx_opts *rec = &nf2ipx_opts_table[i];
        if (rec->nf_id != nf_id) {
            continue;
        }
        // Found!
        return rec;
    }

    return NULL; // Not found
}

/**
 * @brief Append an IPFIX Template to the new IPFIX Message
 *
 * Place the template right after the last committed byte and commit the size of the added template.
 * @warning The IPFIX Message might be reallocated!
 * @param[in] conv Converter internals
 * @param[in] trec Template record to append
 * @return #IPX_OK on success
 * @return #IPX_ERR_NOMEM in case of memory allocation error
 */
static inline int
conv_tmplt_append(ipx_nf9_conv_t *conv, const struct nf9_trec *trec)
{
    size_t size2cpy = trec->ipx_size;
    if (conv_mem_reserve(conv, size2cpy) != IPX_OK) {
        CONV_ERROR(conv, "A memory allocation failed (%s:%d).", __FILE__, __LINE__);
        return IPX_ERR_NOMEM;
    }

    assert(size2cpy != 0);
    void *mem_ptr = conv_mem_ptr_now(conv);
    memcpy(mem_ptr, trec->ipx_data, size2cpy);

    // Commit the modifications
    conv_mem_commit(conv, size2cpy);
    return IPX_OK;
}

/**
 * @brief Add IPFIX (Options) Template from the Template table
 *
 * Try to find the NetFlow template and compare it with a previously parsed Template with the same
 * ID. If the templates are the same, append its IPFIX Template counterpart to the IPFIX Message.
 *
 * @warning The IPFIX Message might be reallocated!
 * @param[in] conv Converter internals
 * @param[in] it   Template Set iterator with a template to convert
 * @param[in] type Template type (::IPX_NF9_SET_TMPLT or ::IPX_NF9_SET_OPTS_TMPLT)
 * @return #IPX_OK on success and the IPFIX Template has been added to the Set.
 * @return #IPX_ERR_NOTFOUND if the template is unknown or doesn't match previously defined one.
 * @return #IPX_ERR_DENIED if the templates match but for format compatibility reasons its
 *   conversion is disabled (i.e. no template is added to the (Options) Template Set)
 * @return #IPX_ERR_NOMEM in case of memory allocation error
 */
static inline int
conv_tmplt_from_table(ipx_nf9_conv_t *conv, const struct ipx_nf9_tset_iter *it, uint16_t type)
{
    uint16_t tid = ntohs(it->ptr.trec->template_id); // TID is always on the same place
    struct nf9_trec *trec = nf9_tmplts_find(&conv->l1_table, tid);
    if (!trec || trec->type != type) {
        // Template not found
        return IPX_ERR_NOTFOUND;
    }

    if (trec->nf9_size != it->size || memcmp(trec->nf9_data, it->ptr.trec, it->size) != 0) {
        // Templates doesn't match
        return IPX_ERR_NOTFOUND;
    }

    if (trec->action == REC_ACT_DROP) {
        // The template and its data records cannot be converter due to format incompatibilities
        return IPX_ERR_DENIED;
    }

    return conv_tmplt_append(conv, trec);
}

/// Auxiliary structure for template conversion
struct conv_tmplt_aux {
    /// New converter template
    struct nf9_trec *tmplt;
    /// Template Set iterator with the current template to process
    const struct ipx_nf9_tset_iter *it;

    /// Pointer to the start of NetFlow field definitions to process
    const struct ipx_nf9_tmplt_ie *ie_nf9_ptr;
    /// Pointer to the start of IPFIX field definitions to add
    fds_ipfix_tmplt_ie *ie_ipx_ptr;
};

/**
 * @brief Convert a NetFlow Template header to an IPFIX Template header (aux. function)
 *
 * Pointers to the template field definitions to be process by conv_tmplt_process_field() are set
 * appropriately.
 * @param[in] aux     Auxiliary conversion structure
 * @param[in] fset_id FlowSet ID (::IPX_NF9_SET_TMPLT or ::IPX_NF9_SET_OPTS_TMPLT)
 */
static inline void
conv_tmplt_process_hdr(struct conv_tmplt_aux *aux, uint16_t fset_id)
{
    if (fset_id == IPX_NF9_SET_TMPLT) {
        // Template header
        const struct ipx_nf9_trec *nf9_trec = aux->it->ptr.trec;
        struct fds_ipfix_trec *ipx_ptr = (struct fds_ipfix_trec *) aux->tmplt->ipx_data;
        ipx_ptr->template_id = nf9_trec->template_id;
        ipx_ptr->count = nf9_trec->count;

        aux->ie_ipx_ptr = &ipx_ptr->fields[0];
        aux->ie_nf9_ptr = &nf9_trec->fields[0];
        aux->tmplt->ipx_size = (uint8_t *) aux->ie_ipx_ptr - (uint8_t *) ipx_ptr; // Size of added data
        return;
    }

    // Options Template header
    assert(fset_id == IPX_NF9_SET_OPTS_TMPLT);
    const struct ipx_nf9_opts_trec *nf9_trec = aux->it->ptr.opts_trec;
    struct fds_ipfix_opts_trec *ipx_ptr = (struct fds_ipfix_opts_trec *) aux->tmplt->ipx_data;
    ipx_ptr->template_id = nf9_trec->template_id;
    ipx_ptr->scope_field_count = htons(aux->it->scope_cnt);
    ipx_ptr->count = htons(aux->it->field_cnt);

    aux->ie_ipx_ptr = &ipx_ptr->fields[0];
    aux->ie_nf9_ptr = &nf9_trec->fields[0];
    aux->tmplt->ipx_size = (uint8_t *) aux->ie_ipx_ptr - (uint8_t *) ipx_ptr; // Size of added data
}

/**
 * @brief Convert a NetFlow (Options) Template into an IPFIX (Options) Template (aux. function)
 *
 * The (Options) Template header is converted to the new format, followed by conversion of
 * template field definitions. In case Options Scope fields, a lookup function is used to
 * find new IPFIX IE definitions instead of incompatible NetFlow enumeration.
 *
 * @warning
 *   The Template conversion structure (aux->tmplt) can be reallocated during conversion!
 *   Pointers to its records must be considered as invalid after calling this function!
 * @param[in] conv    Converter internals
 * @param[in] aux     Auxiliary conversion parameters (the Template, converter internals, etc.)
 * @param[in] fset_id FlowSet ID (::IPX_NF9_SET_TMPLT or ::IPX_NF9_SET_OPTS_TMPLT)
 * @return #IPX_OK on success and the template is successfully converted to IPFIX
 * @return #IPX_ERR_DENIED IPFIX if the Template cannot be converted due to format incompatibility
 * @return #IPX_ERR_NOMEM in case of memory allocation error
 */
static inline int
conv_tmplt_process(ipx_nf9_conv_t *conv, struct conv_tmplt_aux *aux, uint16_t fset_id)
{
    // Parse header and prepare field pointers
    conv_tmplt_process_hdr(aux, fset_id);

    fds_ipfix_tmplt_ie *ipx_ie_ptr = aux->ie_ipx_ptr;
    uint16_t tid = ntohs(aux->it->ptr.trec->template_id);

    uint16_t fields_total = aux->it->field_cnt;
    uint16_t fields_scope = aux->it->scope_cnt;
    size_t cpy_size = 0; // Size (in bytes) to copy without conversion of data record
    size_t nf9_drec_len = 0;
    size_t ipx_drec_len = 0;

    // Calculate size of NetFlow Data record (we need to know it even if conversion fails)
    for (size_t i = 0; i < fields_total; ++i) {
        nf9_drec_len += ntohs(aux->ie_nf9_ptr[i].length);
    }
    assert(nf9_drec_len < UINT16_MAX);
    aux->tmplt->nf9_drec_len = nf9_drec_len;

    if (aux->tmplt->type == IPX_NF9_SET_OPTS_TMPLT && fields_scope == 0) {
        // IPFIX prohibits Options Template without Scope fields
        CONV_WARNING(conv, "Unable to convert an Options Template (ID: %" PRIu16 ") from NetFlow "
           "to IPFIX due to missing Options Fields. Data records of this template will be dropped!",
           tid);
        return IPX_ERR_DENIED;
    }

    for (size_t i = 0; i < fields_total; ++i) {
        const struct ipx_nf9_tmplt_ie nf_ie_def = aux->ie_nf9_ptr[i];
        uint16_t ie_id = ntohs(nf_ie_def.id);
        uint16_t ie_size = ntohs(nf_ie_def.length);

        if (i < fields_scope) {
            // Process Scope field -> try to find mapping of Scope identifiers
            const struct nf2ipx_opts *opts_map = conv_opts_map(ie_id);
            if (!opts_map || opts_map->ipx_max_size < ie_size) {
                CONV_WARNING(conv, "Unable to convert an Options Template (ID %" PRIu16 ") from "
                    "NetFlow to IPFIX due to unknown Scope Field conversion. Options records of "
                    "this template will be dropped!", tid);
                return IPX_ERR_DENIED;
            }

            ipx_ie_ptr->ie.id = htons(opts_map->ipx_id);
            ipx_ie_ptr->ie.length = nf_ie_def.length;
            ipx_ie_ptr++;

            cpy_size += ie_size;
            ipx_drec_len += ie_size;
            continue;
        }

        // Non-scope field -> check if the field should be remapped to different IPFIX field
        const struct nf2ipx_data *data_map = conv_data_map(ie_id);
        if (data_map == NULL) {
            // Conversion doesn't exist
            cpy_size += ie_size;
            ipx_drec_len += ie_size;

            if (ie_id < NF_INCOMP_ID_MIN) {
                ipx_ie_ptr->ie.id = nf_ie_def.id;
                ipx_ie_ptr->ie.length = nf_ie_def.length;
                ipx_ie_ptr++;
                continue;
            }

            // Incompatible fields -> add Enterprise Number
            ipx_ie_ptr->ie.id = htons(ie_id | (uint16_t) 0x8000); // The highest bit must be set!
            ipx_ie_ptr->ie.length = nf_ie_def.length;
            ipx_ie_ptr++;

            uint32_t en_id = ((ie_id & 0x8000) == 0) ? NF_INCOMP_EN_LOW : NF_INCOMP_EN_HIGH;
            ipx_ie_ptr->enterprise_number = htonl(en_id);
            ipx_ie_ptr++;
            continue;
        }

        // ---- Conversion exists ----
        if (data_map->netflow.size != ie_size) {
            CONV_WARNING(conv, "Conversion from NetFlow (Field ID %" PRIu16 ") to IPFIX (EN: "
                "%" PRIu32 ", ID %" PRIu16 ") cannot be performed due to unexpected NetFlow field "
                "size. Template ID %" PRIu16 " and its data records will be ignored!",
                ie_id, data_map->ipfix.en, data_map->ipfix.id, tid);
            return IPX_ERR_DENIED;
        }
        ipx_drec_len += data_map->ipfix.size;

        if (cpy_size > 0) {
            // Add an instruction to copy X bytes of an original data record before conversion
            struct nf2ipx_instr instr = {.itype = NF2IPX_ITYPE_CPY, .size = cpy_size};
            if (nf9_trec_instr_add(&aux->tmplt, instr) != IPX_OK) {
                CONV_ERROR(conv, "A memory allocation failed (%s:%d).", __FILE__, __LINE__);
                return IPX_ERR_NOMEM;
            }
            cpy_size = 0;
        }

        // Add an instruction to convert data field
        if (nf9_trec_instr_add(&aux->tmplt, data_map->instr) != IPX_OK) {
            CONV_ERROR(conv, "A memory allocation failed (%s:%d).", __FILE__, __LINE__);
            return IPX_ERR_NOMEM;
        }

        // Add IPFIX Field ID to the new template
        uint16_t new_id = data_map->ipfix.id;
        if (data_map->ipfix.en != 0) {
            new_id |= (uint16_t) 0x8000; // Set the highest bit
        }

        ipx_ie_ptr->ie.id = htons(new_id);
        ipx_ie_ptr->ie.length = htons(data_map->ipfix.size);
        ipx_ie_ptr++;

        if (data_map->ipfix.en == 0) {
            continue;
        }

        // Add Enterprise Number
        ipx_ie_ptr->enterprise_number = htonl(data_map->ipfix.en);
        ipx_ie_ptr++;
        continue;
    }

    if (cpy_size > 0) {
        // Add an instruction to copy the rest of the original message
        struct nf2ipx_instr instr = {.itype = NF2IPX_ITYPE_CPY, .size = cpy_size};
        if (nf9_trec_instr_add(&aux->tmplt, instr) != IPX_OK) {
            CONV_ERROR(conv, "A memory allocation failed (%s:%d).", __FILE__, __LINE__);
            return IPX_ERR_NOMEM;
        }
    }

    // Update template parameters
    size_t ipfix_fields_size = (uint8_t *) ipx_ie_ptr - (uint8_t *) aux->ie_ipx_ptr;
    aux->tmplt->ipx_size += ipfix_fields_size; // Add size of added fields
    aux->tmplt->ipx_drec_len = ipx_drec_len;

    if (ipx_drec_len > (size_t) (UINT16_MAX - FDS_IPFIX_MSG_HDR_LEN - FDS_IPFIX_SET_HDR_LEN)) {
        CONV_WARNING(conv, "Unable to convert an (Options) Template (ID %" PRIu16 ") from NetFlow "
            "to IPFIX. Size of a single Data record exceeds the maximum size of an IPFIX message. "
            "Records of this template will be dropped!", tid);
        return IPX_ERR_DENIED;
    }

    return IPX_OK;
}

/**
 * @brief Parse and add IPFIX (Options) Template from a NetFlow (Options) Template record
 *
 * The function tries to parse and convert a NetFlow (Options) Template record to IPFIX (Options)
 * Template record and append it to the new IPFIX Message. Once the Template is parsed, it is
 * also added into the internal Template table of already parsed templates and the next time
 * the template must be converted, it is possible to find it using conv_tmplt_from_table().
 *
 * @param[in] conv    Converter internals
 * @param[in] it      Template iterator (points to Template to be parsed)
 * @param[in] fset_id Template type (::IPX_NF9_SET_TMPLT or ::IPX_NF9_SET_OPTS_TMPLT)
 * @return #IPX_OK on success
 * @return #IPX_ERR_DENIED if the IPFIX Template cannot be added due to format incompatibility.
 *   However, a dummy record that signalizes unsupported conversion is added into the Template table.
 * @return #IPX_ERR_NOMEM in case of a memory allocation error
 */
static inline int
conv_tmplt_from_data(ipx_nf9_conv_t *conv, const struct ipx_nf9_tset_iter *it, uint16_t fset_id)
{
    assert(fset_id == IPX_NF9_SET_TMPLT || fset_id == IPX_NF9_SET_OPTS_TMPLT);
    uint16_t tid = ntohs(it->ptr.trec->template_id);

    // Prepare memory for a new IPFIX Template (the worst possible case) and copy of NetFlow tmplt.
    size_t max_size = 6U + (8U * it->field_cnt); // 6 = Opts. Template. header, 8 = IE ID + EN
    struct nf9_trec *template = nf9_trec_new(it->size, max_size);
    if (!template) {
        CONV_ERROR(conv, "A memory allocation failed (%s:%d).", __FILE__, __LINE__);
        return IPX_ERR_NOMEM;
    }

    // Copy the NetFlow Template
    memcpy(template->nf9_data, it->ptr.trec, it->size);
    template->nf9_size = it->size;
    template->action = REC_ACT_CONVERT;
    template->type = fset_id;

    // Process the (Options) Template header and fields
    struct conv_tmplt_aux aux;
    aux.tmplt = template; // Warning: template could be reallocated during conversion!
    aux.it = it;
    int rc = conv_tmplt_process(conv, &aux, fset_id);
    template = aux.tmplt; // Template might have been reallocated during processing!

    if (rc == IPX_ERR_NOMEM) {
        // Memory allocation failed
        nf9_trec_destroy(template);
        return IPX_ERR_NOMEM;
    } else if (rc == IPX_ERR_DENIED) {
        // Unable to convert the Options Template -> "drop" it and its Data record
        free(template->ipx_data);
        template->action = REC_ACT_DROP;
        template->ipx_data = NULL;
        template->ipx_size = 0;
        template->ipx_drec_len = 0;

        if (nf9_tmplts_insert(&conv->l1_table, tid, template) != IPX_OK) {
            // Failed to insert the "dummy drop" template
            CONV_ERROR(conv, "A memory allocation failed (%s:%d).", __FILE__, __LINE__);
            nf9_trec_destroy(template);
            return IPX_ERR_NOMEM;
        }

        return IPX_ERR_DENIED;
    }

    // Insert the template to the internal table
    if (nf9_tmplts_insert(&conv->l1_table, tid, template) != IPX_OK) {
        CONV_ERROR(conv, "A memory allocation failed (%s:%d).", __FILE__, __LINE__);
        nf9_trec_destroy(template);
        return IPX_ERR_NOMEM;
    }

    // Copy the template to the new message
    return conv_tmplt_append(conv, template);
}

/**
 * @brief Convert NetFlow (Options) Template FlowSet to IPFIX (Options) Template Set
 *
 * The function creates and appends a new IPFIX (Options) Template Set with converted
 * (Options) Template records to the new IPFIX Message. For each NetFlow (Options) Template
 * record a conversion function is called and the new IPFIX Template record is stored for future
 * parsing and Data record conversions.
 * @param[in] conv        Converter internals
 * @param[in] flowset_hdr NetFlow (Options) Template Set to be converted
 * @return #IPX_OK on success
 * @return #IPX_ERR_NOMEM in case of a memory allocation error
 * @return #IPX_ERR_FORMAT if the NetFlow (Options) Template FlowSet is malformed and cannot be
 *   converted.
 */
int
conv_process_tset(ipx_nf9_conv_t *conv, const struct ipx_nf9_set_hdr *flowset_hdr)
{
    uint16_t nf_fsid = ntohs(flowset_hdr->flowset_id);
    assert(nf_fsid == IPX_NF9_SET_TMPLT || nf_fsid == IPX_NF9_SET_OPTS_TMPLT);

    // Add (Options) Template Set header
    if (conv_mem_reserve(conv, FDS_IPFIX_SET_HDR_LEN) != IPX_OK) {
        CONV_ERROR(conv, "A memory allocation failed (%s:%d).", __FILE__, __LINE__);
        return IPX_ERR_NOMEM;
    }

    // Get current offset and commit Template Set header (size and ID will be filled later)
    size_t hdr_offset = conv_mem_pos_get(conv);
    conv_mem_commit(conv, FDS_IPFIX_SET_HDR_LEN); // Size and ID will be filled later

    // Convert all (Options) Templates
    uint16_t tmplt_added = 0;
    uint16_t tmplt_processed = 0;

    struct ipx_nf9_tset_iter it;
    ipx_nf9_tset_iter_init(&it, flowset_hdr);
    int rc_conv = IPX_OK;
    int rc_iter;

    while (rc_conv == IPX_OK && (rc_iter = ipx_nf9_tset_iter_next(&it)) == IPX_OK) {
        tmplt_processed++;

        uint16_t tid = ntohs(it.ptr.trec->template_id); // TID position is the same for both types
        CONV_DEBUG(conv, "Processing a definition of %s ID %" PRIu16 "...",
            (nf_fsid == IPX_NF9_SET_TMPLT) ? "Template" : "Options Template", tid);

        // Check if the template has been already processed
        int rval = conv_tmplt_from_table(conv, &it, nf_fsid);
        switch (rval) {
        case IPX_OK:           // IPFIX Template has been added
            tmplt_added++;
            CONV_INFO(conv, "A definition of the (Options) Template ID %" PRIu16 " has been "
                "converted.", tid);
            continue;
        case IPX_ERR_DENIED:   // IPFIX Template cannot be added due to format incompatibility
            CONV_INFO(conv, "A definition of the (Options) Template ID %" PRIu16 " has been "
                "dropped due to format incompatibility (see a previous warning for more details).",
                tid);
            continue;
        case IPX_ERR_NOMEM:    // Memory allocation error
            rc_conv = IPX_ERR_NOMEM;
            continue;
        case IPX_ERR_NOTFOUND: // Not found -> process the template
            break;
        default:
            assert(false);     // Unexpected return code
            break;
        }

        // Convert the NetFlow template to IPFIX and add it to the template manager
        rval = conv_tmplt_from_data(conv, &it, nf_fsid);
        switch (rval) {
        case IPX_OK:
            tmplt_added++;
            CONV_INFO(conv, "A definition of the (Options) Template ID %" PRIu16 " has been "
                "converted.", tid);
            continue;
        case IPX_ERR_DENIED:    // IPFIX Template cannot be added due to format incompatibility
            continue;
        case IPX_ERR_NOMEM:     // Memory allocation error
            rc_conv = IPX_ERR_NOMEM;
            continue;
        default:
            assert(false);      // Unexpected return code
            break;
        }
    }

    if (rc_conv != IPX_OK) {
        return rc_conv;
    }

    if (rc_iter != IPX_EOC) {
        // Iterator failed!
        CONV_ERROR(conv, "%s", ipx_nf9_tset_iter_err(&it));
        return rc_iter;
    }

    // Update number of processed records
    conv->data.recs_processed += tmplt_processed;

    if (tmplt_added == 0) {
        // No template has been added -> "uncommit" Template Set header
        CONV_DEBUG(conv, "Converted (Options) Template Set is empty! Removing its Template Set "
            "header.", '\0');
        conv_mem_pos_set(conv, hdr_offset);
        return IPX_OK;
    }

    // Update IPFIX (Options) Template Set header
    size_t set_size = conv_mem_pos_get(conv) - hdr_offset;
    if (set_size > MAX_SET_CONTENT_LEN) {
        CONV_ERROR(conv, "Unable to convert NetFlow v9 (Options) Template Set (Flow Set ID: "
            "%" PRIu16 ") to IPFIX due to exceeding maximum content size.", nf_fsid);
        return IPX_ERR_FORMAT;
    }

    struct fds_ipfix_tset *hdr_ptr = conv_mem_ptr_offset(conv, hdr_offset);
    hdr_ptr->header.flowset_id = (nf_fsid == IPX_NF9_SET_TMPLT)
        ? htons(FDS_IPFIX_SET_TMPLT) : htons(FDS_IPFIX_SET_OPTS_TMPLT);
    hdr_ptr->header.length = htons((uint16_t) set_size); // Cast is safe, value has been checked
    return IPX_OK;
}

/**
 * @brief Convert (NetFlow) relative timestamp to absolute timestamp
 * @param[in] hdr   NetFlow Message header (required for an exporter timestamps)
 * @param[in] nf_ts Relative NetFlow timestamp stored in a Data record (in Network byte order)
 * @return Converted absolute timestamp (Unix timestamp in milliseconds) (in Host byte order)
 */
static inline uint64_t
conv_ts_rel2abs(const struct ipx_nf9_msg_hdr *hdr, const uint32_t *nf_ts)
{
    const uint64_t hdr_exp = ntohl(hdr->unix_sec) * 1000ULL;
    const uint64_t hdr_sys = ntohl(hdr->sys_uptime);
    const uint64_t rec_sys = ntohl(*nf_ts);
    return (hdr_exp - hdr_sys) + rec_sys;
}

/**
 * @brief Convert a NetFlow data record to IPFIX Data record
 *
 * The function executes instructions described in the internal Template record to perform Data
 * record conversion. After conversion the IPFIX Data record is appended to the new IPFIX Message.
 * @param[in] conv    Converter internals
 * @param[in] nf9_msg NetFlow Message header (necessary for timestamp conversion)
 * @param[in] nf9_rec NetFlow Data record to convert
 * @param[in] tmplt   Internal template record with conversion instructions
 * @return #IPX_OK on success
 * @return #IPX_ERR_NOMEM in case of a memory allocation error
 */
static inline int
conv_process_drec(ipx_nf9_conv_t *conv, const struct ipx_nf9_msg_hdr *nf9_msg,
    const uint8_t *nf9_rec, const struct nf9_trec *tmplt)
{
    assert(tmplt->action == REC_ACT_CONVERT);
    assert(tmplt->instr_size > 0);

    // Reserve enough memory for converted IPFIX record
    if (conv_mem_reserve(conv, tmplt->ipx_drec_len) != IPX_OK) {
        CONV_ERROR(conv, "A memory allocation failed (%s:%d).", __FILE__, __LINE__);
        return IPX_ERR_NOMEM;
    }

    // Execute conversion instructions
    const uint8_t *nf9_pos = nf9_rec;
    uint8_t *ipx_pos = conv_mem_ptr_now(conv);

    for (size_t i = 0; i < tmplt->instr_size; ++i) {
        const struct nf2ipx_instr *instr = &tmplt->instr_data[i];
        switch (instr->itype) {
        case NF2IPX_ITYPE_CPY:
            // Just copy memory
            memcpy(ipx_pos, nf9_pos, instr->size);
            nf9_pos += instr->size;
            ipx_pos += instr->size;
            break;
        case NF2IPX_ITYPE_TS:
            // Convert relative timestamp to absolute timestamp
            *((uint64_t *) ipx_pos) = htobe64(conv_ts_rel2abs(nf9_msg, (uint32_t *) nf9_pos));
            nf9_pos += 4U; // NetFlow TS (FIRST_SWITCHED/LAST_SWITCHED)
            ipx_pos += 8U; // IPFIX TS (iana:flowStartMilliseconds/flowEndMilliseconds)
            break;
        default:
            CONV_ERROR(conv, "(internal) Invalid NetFlow-to-IPFIX conversion instruction", '\0');
            return IPX_ERR_NOMEM; // This will start component termination
        }
    }

    // Commit written memory
    conv_mem_commit(conv, tmplt->ipx_drec_len);
    return IPX_OK;
}

/**
 * @brief Convert NetFlow Data FlowSet to IPFIX Data Set
 *
 * The function creates and appends a new IPFIX Data Set (with converted Data records) to the new
 * IPFIX Message. For each NetFlow Data record a conversion function is called.
 * @param[in] conv        Converter internals
 * @param[in] flowset_hdr NetFlow Data FlowSet to be converted
 * @param[in] nf9_hdr     NetFlow Message header (necessary for timestamp conversions)
 * @return #IPX_OK on success
 * @return #IPX_ERR_NOMEM in case of a memory allocation error
 * @return #IPX_ERR_FORMAT if the NetFlow Data FlowSet is malformed and cannot be converted.
 */
int
conv_process_dset(ipx_nf9_conv_t *conv, const struct ipx_nf9_set_hdr *flowset_hdr,
    const struct ipx_nf9_msg_hdr *nf9_hdr)
{
    uint16_t tid = ntohs(flowset_hdr->flowset_id);
    assert(tid >= IPX_NF9_SET_MIN_DSET);

    // Try to find a template in the manager
    const struct nf9_trec *tmplt = nf9_tmplts_find(&conv->l1_table, tid);
    if (!tmplt) {
        // Template NOT found!
        CONV_WARNING(conv, "Unable to convert NetFlow v9 Data Set (FlowSet ID: %" PRIu16 ") to "
            "IPFIX due to missing NetFlow template. The Data FlowSet and its records will be "
            "dropped!", tid);
        return IPX_OK;
    }

    if (tmplt->action == REC_ACT_DROP) {
        // Data Set will be ignored!
        CONV_DEBUG(conv, "Unable to convert NetFlow v9 Data Set (FlowSet ID: %" PRIu16 ") to "
            "IPFIX. Its template wasn't properly converted from NetFlow to IPFIX.", tid);
        // However, we still have to calculate number of records in the Set
        uint16_t data_size = ntohs(flowset_hdr->length) - IPX_NF9_SET_HDR_LEN;
        uint16_t rec_cnt = data_size / tmplt->nf9_drec_len;
        conv->data.recs_processed += rec_cnt;
        return IPX_OK;
    }

    // Add Data Set header (parameters will be filled later)
    if (conv_mem_reserve(conv, FDS_IPFIX_SET_HDR_LEN) != IPX_OK) {
        CONV_ERROR(conv, "A memory allocation failed (%s:%d).", __FILE__, __LINE__);
        return IPX_ERR_NOMEM;
    }

    size_t hdr_offset = conv_mem_pos_get(conv);
    conv_mem_commit(conv, FDS_IPFIX_SET_HDR_LEN);

    // Convert all records in the Data Set
    uint16_t rec_processed = 0;
    struct ipx_nf9_dset_iter it;
    ipx_nf9_dset_iter_init(&it, flowset_hdr, tmplt->nf9_drec_len);
    int rc_conv = IPX_OK;
    int rc_iter;

    while (rc_conv == IPX_OK && (rc_iter = ipx_nf9_dset_iter_next(&it)) == IPX_OK) {
        rec_processed++;
        rc_conv = conv_process_drec(conv, nf9_hdr, it.rec, tmplt);
    }

    if (rc_conv != IPX_OK) {
        // Converter failed (a proper error message has been already printed)
        return rc_conv;
    }

    if (rc_iter != IPX_EOC) {
        CONV_ERROR(conv, "%s", ipx_nf9_dset_iter_err(&it));
        return rc_iter;
    }

    // Update number of processed records
    conv->data.recs_processed += rec_processed;
    conv->data.drecs_converted += rec_processed;

    // Update IPFIX Data Set header
    size_t set_size = conv_mem_pos_get(conv) - hdr_offset;
    if (set_size > MAX_SET_CONTENT_LEN) {
        CONV_ERROR(conv, "Unable to convert NetFlow v9 Data Set (FlowSet ID: %" PRIu16 ") to "
            "IPFIX due to exceeding maximum content size.", tid);
        return IPX_ERR_FORMAT;
    }

    struct fds_ipfix_dset *hdr_ptr = conv_mem_ptr_offset(conv, hdr_offset);
    hdr_ptr->header.flowset_id = flowset_hdr->flowset_id;
    hdr_ptr->header.length = htons((uint16_t) set_size); // Case is safe, value has been checked
    return IPX_OK;
}

/**
 * @brief Convert a NetFlow Message to an IPFIX Message
 *
 * For each FlowSet in the NetFlow Message is called a particular converter to equivalent IPFIX Set.
 * The converted message is stored to the internal IPFIX Message buffer, however, the header of
 * the IPFIX Message header is not filled.
 *
 * @param[in] conv     Converter internals
 * @param[in] nf9_msg  NetFlow v9 Message data
 * @param[in] nf9_size NetFlow v9 Message size (in bytes)
 * @return #IPX_OK on success
 * @return #IPX_ERR_FORMAT if the NetFlow Message is malformed
 * @return #IPX_ERR_NOMEM in case of a memory allocation error
 */
int
conv_process_msg(ipx_nf9_conv_t *conv, const struct ipx_nf9_msg_hdr *nf9_msg, uint16_t nf9_size)
{
    /* Prepare a new IPFIX message
     * Typical NetFlow messages consist of data records and we usually modify only timestamps
     * (conversion from relative to absolute value), therefore, we should just add (2 x 4B) per
     * record. However, keep in mind that templates (with non-compatible field, i.e. ID > 127) can
     * be also part of the message and we need to change their template definition (i.e. add
     * Enterprise Numbers).
     */
    size_t new_size = (size_t) nf9_size + (8U * ntohs(nf9_msg->count));
    if (conv_mem_reserve(conv, new_size) != IPX_OK) {
        CONV_ERROR(conv, "A memory allocation failed (%s:%d).", __FILE__, __LINE__);
        return IPX_ERR_NOMEM;
    }
    // Add/commit IPFIX Message header (it will be filled later when we know all parameters)
    conv_mem_commit(conv, FDS_IPFIX_MSG_HDR_LEN);

    // Iterate over all NetFlow FlowSets and convert them to IPFIX Sets
    struct ipx_nf9_sets_iter it;
    ipx_nf9_sets_iter_init(&it, nf9_msg, nf9_size);

    int rc_conv = IPX_OK;
    int rc_iter;
    while (rc_conv == IPX_OK && (rc_iter = ipx_nf9_sets_iter_next(&it)) == IPX_OK) {
        uint16_t flowset_id = ntohs(it.set->flowset_id);

        // Try to convert the FlowSet
        if (flowset_id >= IPX_NF9_SET_MIN_DSET) {
            // Data FlowSet
            rc_conv = conv_process_dset(conv, it.set, nf9_msg);
        } else if (flowset_id == IPX_NF9_SET_TMPLT || flowset_id == IPX_NF9_SET_OPTS_TMPLT) {
            // (Options) Template FlowSet
            rc_conv = conv_process_tset(conv, it.set);
        } else {
            // Unknown FlowSet ID -> skip
            CONV_INFO(conv, "Ignoring FlowSet with unsupported ID %" PRIu16, flowset_id);
            rc_conv = IPX_OK;
        }
    }

    if (rc_conv != IPX_OK) {
        // Converter failed (a proper error message has been already printed)
        return rc_conv;
    }

    if (rc_iter != IPX_EOC) {
        // Iterator failed!
        CONV_ERROR(conv, "%s", ipx_nf9_sets_iter_err(&it));
        return rc_iter;
    }


    return IPX_OK;
}

/**
 * @brief Compare sequence numbers (with wraparound support)
 * @param t1 First number
 * @param t2 Second number
 * @return  The  function  returns an integer less than, equal to, or greater than zero if the
 *   first number @p t1 is found, respectively, to be less than, to match, or be greater than
 *   the second number.
 */
static inline int
conv_seq_num_cmp(uint32_t t1, uint32_t t2)
{
    if (t1 == t2) {
        return 0;
    }

    if ((t1 - t2) & 0x80000000) { // test the "sign" bit
        return (-1);
    } else {
        return 1;
    }
}

int
ipx_nf9_conv_process(ipx_nf9_conv_t *conv, ipx_msg_ipfix_t *wrapper)
{
    int rc;
    const struct ipx_nf9_msg_hdr *nf9_hdr = (const struct ipx_nf9_msg_hdr *) wrapper->raw_pkt;
    const uint16_t nf9_size = wrapper->raw_size;

    // Initialize conversion internals
    conv_mem_init(conv);
    conv->data.msg_ctx = &wrapper->ctx;
    conv->data.recs_processed = 0;  // Will be updated by conversion functions
    conv->data.drecs_converted = 0;

    // Check the Message header
    if (nf9_size < IPX_NF9_MSG_HDR_LEN) {
        CONV_ERROR(conv, "Length of a NetFlow (v9) Message is smaller than its header size.", '\0');
        conv_mem_destroy(conv);
        return IPX_ERR_FORMAT;
    }

    if (ntohs(nf9_hdr->version) != IPX_NF9_VERSION) {
        CONV_ERROR(conv, "Invalid version number of a NetFlow Message (expected 9)", '\0');
        conv_mem_destroy(conv);
        return IPX_ERR_FORMAT;
    }

    // Check Sequence number
    uint32_t msg_seq = ntohl(nf9_hdr->seq_number);
    CONV_DEBUG(conv, "Converting a NetFlow Message v9 (seq. num. %" PRIu32 ") to an IPFIX Message "
        "(new seq. num. %" PRIu32 ")", msg_seq, conv->ipx_seq_next);

    bool is_oos = false; // Old (out of sequence message)
    if (conv->nf9_seq_next != msg_seq) {
        is_oos = true;

        if (!conv->nf9_seq_valid) {
            // The first message to convert
            conv->nf9_seq_valid = true;
            conv->nf9_seq_next = msg_seq + 1;
        } else {
            // Out of sequence message
            CONV_WARNING(conv, "Unexpected Sequence number (expected: %" PRIu32 ", got: %" PRId32 ")",
                conv->nf9_seq_next, msg_seq);
            if (conv_seq_num_cmp(msg_seq, conv->nf9_seq_next) > 0) {
                // Newer than expected (expect that all previous messages are lost)
                conv->nf9_seq_next = msg_seq + 1;
            }
        }
    }
    conv->nf9_seq_valid = true;

    // Update expected Sequence number of the next NetFlow message
    if (!is_oos) {
        ++conv->nf9_seq_next;
    }

    // Convert the message
    rc = conv_process_msg(conv, nf9_hdr, nf9_size);
    if (rc != IPX_OK) {
        // Something went wrong!
        conv_mem_destroy(conv);
        return rc;
    }

    // Check number of record in the message
    if (conv->data.recs_processed != ntohs(nf9_hdr->count)) { // TODO: improve in case of ignored templates
        CONV_WARNING(conv, "Number of records in NetFlow v9 Message header doesn't match number of "
            "records found in the Message (expected: %" PRIu16 ", found: %" PRIu16 ")",
            ntohs(nf9_hdr->count), conv->data.recs_processed);
    }

    // Fill the new IPFIX Message header
    size_t ipx_size = conv_mem_pos_get(conv);
    if (ipx_size > UINT16_MAX) {
        CONV_ERROR(conv, "Unable to convert NetFlow v9 to IPFIX. Size of the converted message "
            "exceeds the maximum size of an IPFIX Message! (before: %" PRIu16 " B, after: %zu B)",
            nf9_size, ipx_size);
        conv_mem_destroy(conv);
        return IPX_ERR_FORMAT;
    }

    struct fds_ipfix_msg_hdr *ipx_ptr = conv_mem_ptr_offset(conv, 0);
    ipx_ptr->version = htons(FDS_IPFIX_VERSION);
    ipx_ptr->length = htons((uint16_t) ipx_size); // Cast is safe, size has been checked!
    ipx_ptr->export_time = nf9_hdr->unix_sec;
    ipx_ptr->seq_num = htonl(conv->ipx_seq_next);
    ipx_ptr->odid = nf9_hdr->source_id;
    // Update sequence number of the new IPFIX Message
    conv->ipx_seq_next += conv->data.drecs_converted;

    // Finally, replace the converted NetFlow Message with the new IPFIX Message
    free(wrapper->raw_pkt);
    wrapper->raw_pkt = conv_mem_release(conv);
    wrapper->raw_size = (uint16_t) ipx_size;
    return IPX_OK;
}

void
ipx_nf9_conv_verb(ipx_nf9_conv_t *conv, enum ipx_verb_level v_new)
{
    conv->vlevel = v_new;
}
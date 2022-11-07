/**
 * \file modifier.c
 * \author Adam Zvara <xzvara01@stud.fit.vutbr.cz>
 * \brief Component for modifying IPFIX messages
 * \date 2023
 */

#include <ipfixcol2.h>
#include <libfds.h>

#include "message_ipfix.h"
#include "modifier.h"

/**
 * \def MODIFIER_ERR_CHECK
 * \brief Free memory after filter or append function fail
 * \param[in] rc        Return value of function
 * \param[in] rec       Record to free
 * \param[in] rec_raw   Pointer to raw drec memory (is skipped if NULL)
 * \param[in] tmplt     Pointer to parsed template
 */
#define MODIFIER_ERR_CHECK(rc, rec, rec_raw, tmplt)             \
    if ((rc) != IPX_OK) {                                       \
        if ((rec_raw) != NULL) {                                \
            free((rec)->data);                                  \
        }                                                       \
        free((rec));                                            \
        fds_template_destroy((tmplt));                          \
        return NULL;                                            \
    }

ipx_modifier_t *
ipx_modifier_create(const struct ipx_modifier_field *fields,
    size_t fields_size,
    void *cb_data,
    const fds_iemgr_t *iemgr,
    const enum ipx_verb_level *vlevel,
    const char *ident)
{
    ipx_modifier_t *modifier = calloc(1, sizeof(*modifier));
    if (modifier == NULL) {
        return NULL;
    }

    // Allocate contexts for transport sessions
    modifier->sessions.ctx = malloc(sizeof(*modifier->sessions.ctx) * IPX_MODIFIER_DEF_CTX);
    if (modifier->sessions.ctx == NULL) {
        free(modifier);
        return NULL;
    }

    // Set identificator
    modifier->ident = strdup(ident);
    if (modifier->ident == NULL) {
        free(modifier->sessions.ctx);
        free(modifier);
        return NULL;
    }

    modifier->fields = fields;
    modifier->fields_cnt = fields_size;
    modifier->cb_data = cb_data;
    modifier->sessions.ctx_alloc = IPX_MODIFIER_DEF_CTX;
    modifier->iemgr = iemgr;

    // Verbosity
    if (vlevel == NULL) {
        modifier->vlevel = ipx_verb_level_get();
    } else {
        modifier->vlevel = *vlevel;
    }

    MODIFIER_DEBUG(modifier, "Succesfully created");
    return modifier;
}

void
ipx_modifier_destroy(ipx_modifier_t *mod)
{
    for (size_t i = 0; i < mod->sessions.ctx_valid; i++) {
        fds_tmgr_destroy(mod->sessions.ctx[i].mgr);
    }

    free(mod->sessions.ctx);
    free(mod->ident);
    free(mod);
}

void
ipx_modifier_verb(ipx_modifier_t *mod, enum ipx_verb_level *v_new, enum ipx_verb_level *v_old)
{
    if (v_old != NULL) {
        *v_old = mod->vlevel;
    }

    if (v_new != NULL) {
        mod->vlevel = *v_new;
    }
}

void
ipx_modifier_set_adder_cb(ipx_modifier_t *mod, const modifier_adder_cb_t adder)
{
    mod->cb_adder = adder;
}

void
ipx_modifier_set_filter_cb(ipx_modifier_t *mod, const modifier_filter_cb_t filter)
{
    mod->cb_filter = filter;
}

inline const fds_tmgr_t *
ipx_modifier_get_manager(ipx_modifier_t *mod)
{
    if (mod != NULL && mod->curr_ctx) {
        return mod->curr_ctx->mgr;
    }
    return NULL;
}

inline const fds_iemgr_t *
ipx_modifier_get_iemgr(ipx_modifier_t *mod)
{
    if (mod != NULL) {
        return mod->iemgr;
    }
    return NULL;
}

inline int
ipx_modifier_set_iemgr(ipx_modifier_t *mod, struct fds_iemgr *iemgr)
{
    if (mod == NULL) {
        return IPX_ERR_ARG;
    }

    mod->iemgr = iemgr;
    return IPX_OK;
}

/**
 * \brief Destroy session garbage structure with its content
 * \param[in] grb Session garbage
 */
static void
modifier_ctx_garbage_destroy(struct session_garbage *grb)
{
    if (grb == NULL) {
        return;
    }

    for (size_t i = 0; i < grb->rec_cnt; ++i) {
        fds_tmgr_destroy(grb->mgrs[i]);
    }

    free(grb->mgrs);
    free(grb);
}

/**
 * \brief Move given modifier stream_odid_ctx fields into garbage structure
 *
 * \warning This function only creates garbage message, modifier contexts must
 *  me moved manualy
 *
 * \param[in] mod   Modifer
 * \param[in] begin Index of first context to delete
 * \param[in] end   Index past the last index to delete
 * \return Garbage message or NULL (memory allocation error)
 */
static ipx_msg_garbage_t *
modifier_ctx_to_garbage(ipx_modifier_t *mod, size_t begin, size_t end)
{
    assert(begin < end);

    // Prepare data structures
    struct session_garbage *garbage = malloc(sizeof(*garbage));
    if (!garbage) {
        return NULL;
    }

    garbage->rec_cnt = end - begin;
    garbage->mgrs = malloc(garbage->rec_cnt * sizeof(*garbage->mgrs));
    if (!garbage->mgrs) {
        free(garbage);
        return NULL;
    }

    // Copy records
    for (size_t idx = begin, pos = 0; idx < end; ++idx, ++pos) {
        assert(pos < garbage->rec_cnt);
        garbage->mgrs[pos] = mod->sessions.ctx[idx].mgr;
    }

    // Wrap the garbage
    ipx_msg_garbage_cb cb = (ipx_msg_garbage_cb) &modifier_ctx_garbage_destroy;
    ipx_msg_garbage_t *msg = ipx_msg_garbage_create(garbage, cb);
    if (!msg) {
        free(garbage->mgrs);
        free(garbage);
        return NULL;
    }

    return msg;
}

/**
 * \brief Compare two modifier context structures
 * \param[in] rec1 First structure
 * \param[in] rec2 Second structure
 * \return An integer less than, equal to, or greater than zero if the first argument is considered
 *   to be respectively less than, equal to, or greater than the second.
 */
static int
modifier_ctx_cmp(const void *rec1, const void *rec2)
{
    const struct session_odid_ctx *rec_l = rec1;
    const struct session_odid_ctx *rec_r = rec2;

    // Array MUST be sorted primary by Transport Session
    if (rec_l->session != rec_r->session) {
        return (rec_l->session < rec_r->session) ? (-1) : 1;
    }

    if (rec_l->odid == rec_r->odid) {
        return 0;
    }

    return (rec_l->odid < rec_r->odid) ? (-1) : 1;
}

/**
 * \brief Sort an array of modifier contexts (transport sessions)
 * \param mod Modifier structure
 */
static void
modifier_ctx_sort(ipx_modifier_t *mod)
{
    const size_t rec_cnt = mod->sessions.ctx_valid;
    const size_t rec_size = sizeof(*mod->sessions.ctx);
    qsort(mod->sessions.ctx, rec_cnt, rec_size, &modifier_ctx_cmp);
}

/**
 * \brief Find transport session in modifier
 *
 * \param[in] mod Modifier structure
 * \param[in] msg_ctx Message context
 * \return Modifier context structure or NULL (when not found)
 */
static struct session_odid_ctx *
modifier_ctx_find(ipx_modifier_t *mod, const struct ipx_msg_ctx *msg_ctx)
{
    const size_t rec_cnt = mod->sessions.ctx_valid;
    const size_t rec_size = sizeof(*mod->sessions.ctx);
    struct session_odid_ctx key;
    key.session = msg_ctx->session;
    key.odid = msg_ctx->odid;

    return bsearch(&key, mod->sessions.ctx, rec_cnt, rec_size, &modifier_ctx_cmp);
}

/**
 * \brief Restart current context in modifier
 *
 * Restarting context means clearing out all templates from context manager
 * and moving them into garbage message which is passed in argument. It
 * also includes reseting counter of last used template ID.
 *
 * \param[in]  mod      Modifier
 * \param[out] tmgr_gc  Template manager garbage structure
 * \return #IPX_OK on success, if current context is not set garbage is set to NULL
 * \return #IPX_ERR_NOMEM on memory allocation error (garbage is unchanged)
 */
static int
modifier_ctx_restart(ipx_modifier_t *mod, ipx_msg_garbage_t **gb_msg)
{
    if (mod->curr_ctx == NULL) {
        *gb_msg = NULL;
        return IPX_OK;
    }

    int rc;
    fds_tgarbage_t *mgr_garbage;

    // Clear template manager and get garbage
    fds_tmgr_clear(mod->curr_ctx->mgr);
    rc = fds_tmgr_garbage_get(mod->curr_ctx->mgr, &mgr_garbage);
    if (rc) {
        // Memory allocation error
        return rc;
    }

    if (mgr_garbage != NULL) {
        ipx_msg_garbage_cb cb = (ipx_msg_garbage_cb) &fds_tmgr_garbage_destroy;
        *gb_msg = ipx_msg_garbage_create(mgr_garbage, cb);
    }

    // Reset ID counter
    mod->curr_ctx->next_id = FDS_IPFIX_SET_MIN_DSET;

    return IPX_OK;
}

/**
 * \brief Set template manager for given message context
 *
 * Template manager stores templates for given transport session.
 * If manager for does not exist create new one based on session type.
 *
 * \param[in] mod     Modifier
 * \param[in] msg_ctx IPFIX message context
 * \return #IPX_OK if manager is set, otherwise #IPX_ERR_NOMEM and manager is not valid
 */
static int
modifier_ctx_get(ipx_modifier_t *mod, const struct ipx_msg_ctx *msg_ctx)
{
    // Try to find stream context in modifier context array
    struct session_odid_ctx *ctx = modifier_ctx_find(mod, msg_ctx);
    if (ctx != NULL) {
        mod->curr_ctx = ctx;
        return IPX_OK;
    }

    // This stream context does not exist, create new one
    if (mod->sessions.ctx_valid == mod->sessions.ctx_alloc) {
        const size_t alloc_new = 2 * mod->sessions.ctx_alloc;
        const size_t alloc_size = alloc_new * sizeof(*mod->sessions.ctx);
        struct session_odid_ctx *recs_new = realloc(mod->sessions.ctx, alloc_size);
        if (!recs_new) {
            return IPX_ERR_NOMEM;
        }

        mod->sessions.ctx = recs_new;
        mod->sessions.ctx_alloc = alloc_new;
    }

    ctx = &(mod->sessions.ctx[mod->sessions.ctx_valid]);
    ctx->session = msg_ctx->session;
    ctx->odid = msg_ctx->odid;
    ctx->next_id = FDS_IPFIX_SET_MIN_DSET;

    // Create new template manager
    ctx->mgr = fds_tmgr_create(msg_ctx->session->type);
    if (ctx->mgr == NULL) {
        return IPX_ERR_NOMEM;
    }

    // TODO: think about snapshot timeout in future
    // fds_tmgr_set_snapshot_timeout(ctx->mgr, 30);

    // Set IE manager for new template manager
    const struct fds_iemgr *iemgr = ipx_modifier_get_iemgr(mod);
    if (fds_tmgr_set_iemgr(ctx->mgr, iemgr)) {
        fds_tmgr_destroy(ctx->mgr);
        return IPX_ERR_NOMEM;
    }

    mod->sessions.ctx_valid++;
    modifier_ctx_sort(mod);
    mod->curr_ctx = modifier_ctx_find(mod, msg_ctx);
    assert(mod->curr_ctx != NULL);

    MODIFIER_DEBUG(mod, "Message with new context found (%s, ODID: %" PRIu32 ")",
        ctx->session->ident, ctx->odid);

    return IPX_OK;
}

int
ipx_modifier_add_session(ipx_modifier_t *mod, ipx_msg_ipfix_t *msg, ipx_msg_garbage_t **garbage)
{
    if (mod == NULL || msg == NULL) {
        return IPX_ERR_ARG;
    }

    int rc;
    *garbage = NULL;

    // Set modifier manager for given message context
    rc = modifier_ctx_get(mod, &msg->ctx);
    if (rc != IPX_OK) {
        return rc;
    }

    // Set export time
    const struct fds_ipfix_msg_hdr *msg_data = (struct fds_ipfix_msg_hdr *)(msg)->raw_pkt;
    rc = fds_tmgr_set_time(mod->curr_ctx->mgr, ntohl(msg_data->export_time));
    if (rc != FDS_OK) {
        switch (rc) {
        case FDS_ERR_DENIED:
            // Messages with invalid sequence number for TCP session should be blocked by parser
            // but just in case ...
            MODIFIER_ERROR(mod, "Trying to set time in history for TCP session (%s odid:%d)",
                mod->curr_ctx->session->ident, mod->curr_ctx->odid);
            return IPX_ERR_FORMAT;
        case FDS_ERR_NOTFOUND:
            // Export time is too old, I don't care because I don't take any templates from manager but
            // only create new ones and store them in it
            return IPX_OK;
        case FDS_ERR_NOMEM:
            MODIFIER_ERROR(mod, "Memory allocation error (%s:%d)", __FILE__, __LINE__);
            return IPX_ERR_NOMEM;
        default:
            MODIFIER_ERROR(mod, "Unexpected error from fds_tmgr_set_time function (%s:%d)",
                __FILE__, __LINE__);
            return IPX_ERR_DENIED;
        }
    }

    // Clear garbage from manager
    fds_tgarbage_t *fds_garbage;
    if (fds_tmgr_garbage_get(mod->curr_ctx->mgr, &fds_garbage) == FDS_OK && fds_garbage != NULL) {
        ipx_msg_garbage_cb cb = (ipx_msg_garbage_cb) &fds_tmgr_garbage_destroy;
        *garbage = ipx_msg_garbage_create(fds_garbage, cb);
    }

    MODIFIER_DEBUG(mod, "Modifying IPFIX message [%s, ODID: %" PRIu32 "] (Seq. number %" PRIu32 ")",
        msg->ctx.session->ident, msg->ctx.odid, ntohl(msg_data->seq_num));

    return IPX_OK;
}

int
ipx_modifier_remove_session(struct ipx_modifier *mod, const struct ipx_session *session,
    ipx_msg_garbage_t **garbage)
{
    size_t idx_start; // Index of first occurence of session records
    size_t idx_end;   // Index of next valid occurence of session records (for next session)

    // Find starting index of session
    for (idx_start = 0; idx_start < mod->sessions.ctx_valid; ++idx_start) {
        if (mod->sessions.ctx[idx_start].session == session) {
            // Remove reference of current context if it will be deleted
            if (mod->curr_ctx && mod->curr_ctx->session == session) {
                mod->curr_ctx = NULL;
            }
            break;
        }
    }

    if (idx_start == mod->sessions.ctx_valid) {
        // Session not found
        return IPX_ERR_NOTFOUND;
    }

    // Find index of next valid session
    for (idx_end = idx_start + 1; idx_end < mod->sessions.ctx_valid; ++idx_end) {
        // Remove reference of current context if it will be deleted
        if (mod->curr_ctx != NULL && mod->curr_ctx->session == session) {
            mod->curr_ctx = NULL;
        }

        if (mod->sessions.ctx[idx_end].session != session) {
            break;
        }
    }

    // Move session data into garbage
    ipx_msg_garbage_t *garbage_msg = modifier_ctx_to_garbage(mod, idx_start, idx_end);
    /* In case of memory allocation error we cannot free structures here because someone
     * still could use them, which causes memory leak but not segfault */

    while (idx_end < mod->sessions.ctx_valid) {
        mod->sessions.ctx[idx_start++] = mod->sessions.ctx[idx_end++];
    }

    // Update number of valid records
    mod->sessions.ctx_valid = idx_start;
    *garbage = garbage_msg;

    return IPX_OK;
}


/**
 * \brief Initialize output buffers
 *
 * \note Setting buffer length value to negative number makes buffer invalid and
 * value from it will not be read
 *
 * \param[in] buffers    Output buffers
 * \param[in] buffer_cnt Amount of items in buffers array
 */
static inline void
output_buffers_init(struct ipx_modifier_output *buffers, size_t buffer_cnt)
{
    for (size_t i = 0; i < buffer_cnt; i++) {
        buffers[i].length = -1;
    }
}

/**
 * \brief Calculate size of new fields in template (in bytes)
 *
 * \warning Only fields with valid values in output buffer (length >= 0)
 *  are used
 *
 * \param[in]  fields       Fields array
 * \param[in]  buffers      Output buffers
 * \param[in]  fields_cnt   Fields array cnt
 * \return Size of all (template) fields with valid output value
 */
static inline size_t
new_template_size(const struct ipx_modifier_field *fields,
    const struct ipx_modifier_output *buffers,
    const size_t fields_cnt)
{
    size_t new_size = 0;
    for (size_t i = 0; i < fields_cnt; i++) {
        if (buffers[i].length >= 0) {
            new_size += (fields[i].en ? 8 : 4);
        }
    }
    return new_size;
}

/**
 * \brief Calculate size needed for new data records
 *
 * \param[in] buffers    Output buffers
 * \param[in] fields_cnt Fields array cnt
 * \return Space needed for new data records in output buffers
 */
static inline size_t
get_buffers_size(const struct ipx_modifier_output *buffers, const size_t fields_cnt)
{
    size_t new_size = 0;
    for (size_t i = 0; i < fields_cnt; i++) {
        if (buffers[i].length >= 0) {
            // Take the worst case scanario where each new record
            // has variable length with 3 prefix octets
            new_size += buffers[i].length + 3;
        }
    }
    return new_size;
}

/**
 * \brief Add new element into template
 *
 * \param[in]     field Field to add at end of raw template pointed to by tmp
 * \param[in,out] raw   Points to the start of new field
 * \return Size of added field (4 or 8)
 */
static uint8_t
ipfix_template_add_field(const struct ipx_modifier_field field, uint16_t *raw)
{
    union fds_ipfix_tmplt_ie_u *tfield = (union fds_ipfix_tmplt_ie_u *) raw;

    if (field.en == 0) {
        // No enterprise number (4B size)
        tfield->ie.id = htons(field.id);
        tfield->ie.length = htons(field.length);
        return 4;
    } else {
        // Enterprise number defined (4B + 4B size)
        tfield->ie.id = htons(field.id | (1 << 15));
        tfield->ie.length = htons(field.length);
        tfield = (union fds_ipfix_tmplt_ie_u *) (raw + 2);
        tfield->enterprise_number = htonl(field.en);
        return 8;
    }

    // Should not be reached
    assert(false);
}

/**
 * \brief Append new fields to template based on given output values
 *
 * \param[in] tmplt      Original template
 * \param[in] fields     Array of new fields
 * \param[in] buffers    Array of output values
 * \param[in] fields_cnt Number of items in fields (and buffers) array
 * \return Parsed modified template or NULL (memory allocation error)
 */
static struct fds_template *
ipfix_template_add_fields(const struct fds_template *tmplt,
    const struct ipx_modifier_field *fields,
    const struct ipx_modifier_output *buffers,
    const size_t fields_cnt)
{
    // Allocate memory for extended raw template
    size_t new_size = new_template_size(fields, buffers, fields_cnt);
    uint8_t *raw_tmplt = malloc(tmplt->raw.length + new_size);
    if (raw_tmplt == NULL) {
        return NULL;
    }
    memcpy(raw_tmplt, tmplt->raw.data, tmplt->raw.length);

    uint8_t new_cnt = tmplt->fields_cnt_total;
    uint16_t tmplt_length = tmplt->raw.length;
    uint8_t *iter = raw_tmplt + tmplt_length;
    uint16_t field_size;

    // Iterate through all valid values in output buffers and append their fields
    for (size_t i = 0; i < fields_cnt; i++) {
        if (buffers[i].length >= 0) {
            field_size = ipfix_template_add_field(fields[i], (uint16_t *) iter);
            iter += field_size;
            tmplt_length += field_size;
            new_cnt++;
        }
    }

    // Update number of fields
    struct fds_ipfix_trec *trec = (struct fds_ipfix_trec *) raw_tmplt;
    trec->count = htons(new_cnt);

    // Parse modified template
    struct fds_template *new_tmplt = NULL;
    fds_template_parse(FDS_TYPE_TEMPLATE, raw_tmplt, &tmplt_length, &new_tmplt);

    free(raw_tmplt);
    return new_tmplt;
}

/**
 * \brief Append new fields to data record
 *
 * \note Variable length fields will contain 1 or 3 length prefix octets
 *  based on their length in output buffer. For length < 255 single prefix octet
 *  is used. Otherwise 3 octets are used.
 *
 * \param[in,out] rec         Original/modified record
 * \param[in]     fields      Field definitions
 * \param[in]     output      Output buffers containing new values
 * \param[in]     arr_cnt     Size of output buffer
 * \return #IPX_OK on success, otherwise #IPX_ERR_NOMEM
 */
static int
ipfix_msg_add_drecs(struct fds_drec *rec,
    const struct ipx_modifier_field *fields,
    const struct ipx_modifier_output *output,
    const size_t arr_cnt)
{
    // Allocate memory for raw data record, copy previous record
    size_t append_size = get_buffers_size(output, arr_cnt);
    uint8_t *new_data = malloc(rec->size + append_size);
    if (new_data == NULL) {
        return IPX_ERR_NOMEM;
    }
    memcpy(new_data, rec->data, rec->size);

    // Iterate through values in output buffer and append them to data record
    uint8_t *it = new_data + rec->size;
    for (size_t i = 0; i < arr_cnt; i++) {
        if (output[i].length < 0) {
            // Skip invalid value in buffer
            continue;
        }

        if (fields[i].length == FDS_IPFIX_VAR_IE_LEN) {
            // Append length octet prefix for variable length record
            if (output[i].length < 255) {
                memcpy(it++, &(output[i].length), 1);
                rec->size += 1;
            } else {
                *it = 0xFF;
                memcpy(++it, &(output[i].length), 2);
                it += 2;
                rec->size += 3;
            }
        }

        // Copy actual data from output buffer
        memcpy(it, output[i].raw, output[i].length);
        it += output[i].length;
        rec->size += output[i].length;
    }

    rec->data = new_data;
    return IPX_OK;
}

/**
 * \brief Remove fields from template based on given filter
 *
 * \warning Only non-option templates are accepted.
 *
 * \param[in] tmplt     Original template
 * \param[in] filter    Filter array
 * \return Parsed modified template or NULL (memory allocation error)
 */
static struct fds_template *
ipfix_template_remove_fields(const struct fds_template *tmplt, const uint8_t *filter)
{
    assert(tmplt->type == FDS_TYPE_TEMPLATE);

    // Allocate memory for new raw template
    uint8_t *raw_tmplt = malloc(tmplt->raw.length);
    if (raw_tmplt == NULL) {
        return NULL;
    }

    uint16_t fields_cnt = tmplt->fields_cnt_total;

    // Pointers to deleted fields
    uint8_t *to_delete[fields_cnt];
    // Offset of next valid field for each deleted field
    uint16_t offsets[fields_cnt];

    // Iterate through fields and store fields to be removed
    int current_offset = 4;
    uint8_t *it = tmplt->raw.data + current_offset;
    uint16_t length = tmplt->raw.length;
    uint16_t field_len;

    for (size_t i = 0; i < fields_cnt; i++) {
        // For non standard fields EN is present (4+4B)
        field_len = tmplt->fields[i].en ? 8 : 4;
        if (filter[i]) {
            to_delete[i] = it;
            offsets[i] = current_offset + field_len;
            length -= field_len;
        }
        it += field_len;
        current_offset += field_len;
    }

    // Pointer to original message always pointing to start of previous
    // sequence of non-filtered data records
    uint8_t *original_ptr = tmplt->raw.data;
    // Points to memory after last data record in new message
    uint8_t *new_ptr = raw_tmplt;

    size_t copy_record_size;
    for (size_t i = 0; i < tmplt->fields_cnt_total; i++) {
        if (filter[i]) {
            // This field is filtered, copy all drecs starting from original_ptr till now (to_delete)
            fields_cnt--;
            copy_record_size = to_delete[i] - original_ptr;
            if (copy_record_size) {
                // Copy only if there is something to copy (copy_record_size != 0)
                memcpy(new_ptr, original_ptr, copy_record_size);
                new_ptr += copy_record_size;
            }
            original_ptr = tmplt->raw.data + offsets[i];
        }
    }
    // Copy the rest of the message
    memcpy(new_ptr, original_ptr, tmplt->raw.length - (original_ptr - tmplt->raw.data));
    *((uint16_t *)(raw_tmplt)+1) = htons(fields_cnt);

    // Parse new template and replace it in record
    struct fds_template *new_template = NULL;
    fds_template_parse(FDS_TYPE_TEMPLATE, raw_tmplt, &length, &new_template);

    free(raw_tmplt);
    return new_template;
}

/**
 * \brief Remove fields from data record based on filter
 *
 * Filter is an array of integers for each field in record template. For each
 * non-zero value in filter, data field at same position is removed from message.
 *
 * \param[in,out] rec         Original record
 * \param[in]     filter      Filter array
 */
static void
ipfix_msg_remove_drecs(struct fds_drec *rec, const uint8_t *filter)
{
    uint16_t filter_size = rec->tmplt->fields_cnt_total;

    // Pointers to deleted fields
    uint8_t *to_delete[filter_size];
    // Offset of next valid record for each deleted field
    uint16_t offsets[filter_size];

    // Address of previous record used to determine if record has variable length
    uint8_t *prev_rec = rec->data;
    uint16_t rec_size = rec->size;
    int idx = 0;
    int length_octets, rc;

    struct fds_drec_iter it;
    fds_drec_iter_init(&it, rec, FDS_DREC_PADDING_SHOW);

    // Iterate fields, if given field should be deleted,
    // store pointer to its start and offset of next field
    while ((rc = fds_drec_iter_next(&it)) != FDS_EOC) {
        length_octets = it.field.data - prev_rec;
        if (filter[idx]) {
            to_delete[idx] = it.field.data - length_octets;
            offsets[idx] = (it.field.data + it.field.size) - rec->data;
            rec_size -= it.field.size + length_octets;
        }
        prev_rec = it.field.data + it.field.size;
        assert(idx++ < filter_size);
    }

    // Go through filter in reverse order and delete selected records
    while (--idx >= 0) {
        if (filter[idx]) {
            uint16_t offset = offsets[idx];
            memmove(to_delete[idx], rec->data + offset, rec->size - offset);
        }
    }

    rec->size = rec_size;
}

int
ipx_modifier_filter(struct fds_drec *rec, uint8_t *filter)
{
    if(rec == NULL || filter == NULL) {
        return IPX_ERR_ARG;
    }

    // Modify template
    struct fds_template *tmplt = ipfix_template_remove_fields(rec->tmplt, filter);
    if (tmplt == NULL) {
        return IPX_ERR_NOMEM;
    }

    // Modify record
    ipfix_msg_remove_drecs(rec, filter);
    rec->tmplt = tmplt;
    rec->snap = NULL;

    return IPX_OK;
}

int
ipx_modifier_append(struct fds_drec *rec,
    const struct ipx_modifier_field *fields,
    const struct ipx_modifier_output *buffers,
    const size_t fields_cnt)
{
    if (rec == NULL || fields == NULL || buffers == NULL) {
        return IPX_ERR_ARG;
    }

    // Modify template
    struct fds_template *tmplt = ipfix_template_add_fields(rec->tmplt, fields, buffers, fields_cnt);
    if (tmplt == NULL) {
        return IPX_ERR_NOMEM;
    }

    // Modify record
    if (ipfix_msg_add_drecs(rec, fields, buffers, fields_cnt) != IPX_OK) {
        fds_template_destroy(tmplt);
        return IPX_ERR_NOMEM;
    }
    rec->tmplt = tmplt;
    rec->snap = NULL;

    return IPX_OK;
}

/**
 * \brief Data used for comparing templates
 */
struct tmplt_cmp_data {
    /** Template to compare with                        */
    struct fds_template *t2;
    /** ID of matched template (or 0 if none matched)   */
    int id;
};

/**
 * \brief Compare templates based on field definitions (ID is skipped)
 * \note True and false are values to indicate continuation/end of iteration in snapshot_for loop
 * \note Real return value is passed back in \p data
 *
 * \param[in]     t1   First template
 * \param[in,out] data Structure tmplt_cmp_data containing second template and return value
 * \return True if templates DO NOT match, otherwise false
 */
static bool
template_cmp(const struct fds_template *t1, void *data)
{
    struct tmplt_cmp_data *cmp = (struct tmplt_cmp_data *) data;

    // +- 2 to skip first field (ID)
    if (!memcmp(t1->raw.data+2, cmp->t2->raw.data+2, cmp->t2->raw.length-2)) {
        // Templates are equal, return id in cmp_data structure and stop iteration
        cmp->id = t1->id;
        return false;
    }
    return true;
}

/**
 * \brief Search for template in manager, add new if it does not exist
 *
 * Iterate through all templates in current snapshot and
 * compare them with given template (without IDs). If template
 * DOES exist, do not do anything. If template DOES NOT exist, add
 * new template to manager.
 *
 * \param[in] mgr       Template manager
 * \param[in] template  Template to store
 *
 * \return Template ID if template was successfully added
 * \return #FDS_OK if template already exists
 * \return #FDS_ERR_DENIED for invalid combination of session and template
 * \return #FDS_ERR_ARG if time context of manager is not set
 * \return #FDS_ERR_NOMEM for memory allocation error
 */
static int
template_store(fds_tmgr_t *mgr, struct fds_template *template)
{
    int rc;

    // Check if template exists
    const fds_tsnapshot_t *snapshot;
    rc = fds_tmgr_snapshot_get(mgr, &snapshot);
    if (rc != FDS_OK) {
        return rc;
    }
    struct tmplt_cmp_data cdata = { .t2 = template, .id = 0 };
    fds_tsnapshot_for(snapshot, &template_cmp, &cdata);

    // Template already exists
    if (cdata.id != 0) {
        return IPX_OK;
    }

    // Try to add template
    rc = fds_tmgr_template_add(mgr, template);
    if (rc != FDS_OK) {
        return rc;
    }

    return template->id;
}

/**
 * \brief Generate and set new ID for template
 *
 * \param[in] mod   Modifier
 * \param[in] tmplt Modified template
 * \return #IPX_OK on success, #IPX_ERR_LIMIT when maximum number of template IDs has been reached
 */
static int
template_set_new_id(ipx_modifier_t *mod, struct fds_template *tmplt)
{
    uint16_t new_id = mod->curr_ctx->next_id;

    if (new_id == 0) {
        // No more IDs are availiable!
        MODIFIER_WARNING(mod, "No more IDs availiable for session %s (ODID: %d)",
            mod->curr_ctx->session->ident, mod->curr_ctx->odid);
        return IPX_ERR_LIMIT;
    }

    tmplt->id = new_id;
    uint16_t *raw_id = (uint16_t *) tmplt->raw.data;
    *raw_id = htons(new_id);
    mod->curr_ctx->next_id = ++new_id;
    return IPX_OK;
}

struct fds_drec *
ipx_modifier_modify(ipx_modifier_t *mod, const struct fds_drec *rec, ipx_msg_garbage_t **garbage)
{
    if (mod == NULL || rec == NULL) {
        return NULL;
    }

    if (!mod->curr_ctx) {
        MODIFIER_ERROR(mod, "Attempting to modify record without context being set");
        return NULL;
    }

    int rc;
    *garbage = NULL;

    // Create copy of original record and template
    struct fds_drec *new_rec = malloc(sizeof(*new_rec));
    if (new_rec == NULL) {
        return NULL;
    }
    memcpy(new_rec, rec, sizeof(*new_rec));

    struct fds_template *new_tmplt = fds_template_copy(rec->tmplt);
    if (new_tmplt == NULL) {
        free(new_rec);
        return NULL;
    }
    new_rec->tmplt = new_tmplt;

    // Initialize filter and filter out fields from drec and template
    uint8_t filter[rec->tmplt->fields_cnt_total];
    memset(filter, 0, rec->tmplt->fields_cnt_total);
    if (mod->cb_filter) {
        mod->cb_filter(rec, filter, mod->cb_data);
        MODIFIER_ERR_CHECK(ipx_modifier_filter(new_rec, filter), new_rec, NULL, new_tmplt);
    }

    // Initialize outputs and add new values to drec and template
    struct ipx_modifier_output buffers[mod->fields_cnt];
    output_buffers_init(buffers, mod->fields_cnt);
    if (mod->cb_adder) {
        mod->cb_adder(rec, buffers, mod->cb_data);
        MODIFIER_ERR_CHECK(ipx_modifier_append(new_rec, mod->fields, buffers, mod->fields_cnt),
            new_rec, new_rec->data, new_tmplt);
    }

    // Need to free copy only if template was modified
    if (mod->cb_adder || mod->cb_filter) {
        fds_template_destroy(new_tmplt);
    }

    new_tmplt = (struct fds_template *) new_rec->tmplt;

    // Try to create new ID for template
    rc = template_set_new_id(mod, new_tmplt);
    if (rc == IPX_ERR_LIMIT) {
        // No more IDs available, restart context
        modifier_ctx_restart(mod, garbage);
        // Need to set new ID again
        rc = template_set_new_id(mod, new_tmplt);
        assert(rc == IPX_OK);
    }

    rc = template_store(mod->curr_ctx->mgr, new_tmplt);

    if (rc < FDS_OK) {
        // Something bad happened
        switch (rc) {
        case FDS_ERR_DENIED:
            MODIFIER_ERROR(mod, "Invalid combination of template and session (%s odid:%d)",
                mod->curr_ctx->session->ident, mod->curr_ctx->odid);
            break;
        case FDS_ERR_ARG:
            MODIFIER_ERROR(mod, "Time context of manager is not set");
            break;
        case FDS_ERR_NOMEM:
            MODIFIER_ERROR(mod, "A memory allocation failed (%s:%d)",  __FILE__, __LINE__);
            break;
        default:
            MODIFIER_ERROR(mod, "Unexpected error occured (%s:%d)", __FILE__, __LINE__);
            break;
        }
        return NULL;
    } else if (rc > FDS_OK) {
        MODIFIER_DEBUG(mod, "Added new template %" PRIu16 " from template %" PRIu16 "",
            rc, rec->tmplt->id);
    }

    // Update record snapshot
    const fds_tsnapshot_t *snap;
    rc = fds_tmgr_snapshot_get(mod->curr_ctx->mgr, &snap);
    MODIFIER_ERR_CHECK(rc, new_rec, new_rec->data, new_tmplt);
    new_rec->snap = snap;

    // Set definitions to new fields (preserve == true)
    fds_template_ies_define(new_tmplt, mod->iemgr, true);

    return new_rec;
}
/**
 * \author Michal Režňák
 * \date   8/28/17
 */
#include <string.h>
#include <ipfixcol2/templater.h>
#include <ipfixcol2/converters.h>
#include <libfds/iemgr.h>
#include "tmpl_common.h"

#define FIRST_BIT(_value_)    ((_value_) &  (0x8000)) // TODO
#define NO_FIRST_BIT(_value_) ((_value_) & ~(0x8000)) // TODO

/**
 * \brief Change 'last_identical' to \p false to all previous elements with same ID
 * \param[in,out] field   Field of elements
 * \param[in]     index Number of elements
 */
void
change_identical(struct ipx_tmpl_template_field *field, int index)
{
    for (int i = index -1; i >= 0; --i) {
        if (field[i].id != field[index].id) {
            continue;
        }
        if (field[i].en != field[index].en) {
            continue;
        }

        field[i].last_identical = false;
    }
}

/**
 * \brief Set all elements that are last_identical to true
 * \param[in,out] dst   Field of elements
 * \param[in]     count Number of elements
 */
void
set_last_identical(struct ipx_tmpl_template_field *dst, size_t count)
{
    int64_t field = 0;
    int64_t index;

    for (size_t i = 0; i < count; ++i) {
        index = (1 << (dst[i].id % 64));
        dst[i].last_identical = true;

        if (!(index & field)) {
            field += index;
        } else {
            change_identical(dst, i);
        }
    }
}

/**
 * \brief Find if any of the element in a field is defined multiple times
 * \param[in] fields Field of elements
 * \param[in] count  Number of elements
 * \return True if is, false if not
 */
bool
has_multiple_definitions(struct ipx_tmpl_template_field *fields, size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        for (size_t j = i+1; j < count; ++j) {
            if (fields[j].id != fields[i].id) {
                continue;
            }
            if (fields[j].en != fields[i].en) {
                continue;
            }

            return true;
        }
    }

    return false;
}

/**
 * \brief Find if any of the element in a filed is variable length
 * \param[in] fields Field of elements
 * \param[in] count  Number of elements
 * \return True if any is, false if not
 */
bool
has_dynamic(const struct ipx_tmpl_template_field *fields, size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        if (fields[i].length >= IPFIX_VAR_IE_LENGTH) {
            return true;
        }
    }
    return false;
}

/**
 * \brief Parse one element from \p src to the \p dst
 * \param[in]  tmpl   Templater
 * \param[out] dst    Destination elements
 * \param[in]  src    Source element
 * \param[in]  offset Offset of the element
 * \return Length of the element
 */
uint16_t
field_parse(ipx_tmpl_t *tmpl, struct ipx_tmpl_template_field *dst, const template_ie *src, uint16_t offset)
{
    uint64_t id;
    ipx_get_uint_be(&src->ie.id, INT16_SIZE, &id);
    if (FIRST_BIT(id)) {
        ipx_get_uint_be(&(src+1)->enterprise_number, INT32_SIZE, (uint64_t*) &dst->en);
    } else {
        dst->en = 0;
    }

    uint64_t len;
    ipx_get_uint_be(&src->ie.length, INT16_SIZE, &len);

    dst->id         = (uint16_t) NO_FIRST_BIT(id);
    dst->length     = (uint16_t) len;
    dst->offset     = (uint16_t) offset;

    if (tmpl->iemgr != NULL) {
        dst->definition = fds_iemgr_elem_find_id(tmpl->iemgr, dst->en, dst->id);
    } else {
        dst->definition =  NULL;
    }

    if (dst->length == IPFIX_VAR_IE_LENGTH) {
        return 1;
    }
    return dst->length;
}

/**
 * \brief Parse fields from a \p src to the \p dst.
 * \param[in]  tmpl    Templater
 * \param[out] dst     Destination field of elements
 * \param[in]  src     Source field of elements
 * \param[in]  count   Number of elements
 * \param[in]  max_len Maximal length of elements together
 * \param[out] offset  Total offset of the elements
 * \return Length of the records together
 */
int
fields_parse(ipx_tmpl_t *tmpl, struct ipx_tmpl_template_field *dst, const template_ie *src, uint16_t count,
             uint16_t max_len, uint16_t *offset)
{
    uint16_t index = 0;
    size_t i;
    for (i = 0; i < count; ++i, ++index) {
        if (index*INT32_SIZE > max_len) {
            return IPX_ERR;
        }
        *offset += field_parse(tmpl, &dst[i], &src[index], *offset);

        // move to another field if PEN is present
        if (dst[i].en > 0) {
            index++;
        }
    }

    set_last_identical(dst, i);
    return index*INT32_SIZE;
}

/**
 * \brief Copy anything of the defined length
 * \param[in] src Source binary blob
 * \param[in] len Length of the source
 * \return Copied \p src
 */
void *
copy(const void *src, size_t len)
{
    void *res = malloc(len);
    memcpy(res, src, len);
    return res;
}

bool
template_save_properties(ipx_tmpl_t* tmpl, ipx_tmpl_template_t* res,
                         const struct ipfix_template_record* rec, uint16_t len)
{
    uint64_t id = 0;
    ipx_get_uint_be(&rec->template_id, INT16_SIZE, &id);
    if (id < 258) {
        return false;
    }

    len += sizeof(rec) - sizeof(rec->fields);
    res->template_type                = IPX_TEMPLATE;
    res->options_type                 = IPX_OPTS_NO_OPTIONS;
    res->id                           = (uint16_t) id;
    res->raw.data                     = copy((void *) rec, len);
    res->raw.length                   = len;
    res->time.first                   = tmpl->current.time;
    res->time.last                    = tmpl->current.time;
    res->time.end                     = 0;
    res->number_packet                = tmpl->current.count;
    res->next                     = NULL;
    res->fields_cnt_scope             = 0;
    res->properties.has_multiple_defs = has_multiple_definitions(res->fields, res->fields_cnt_total);
    res->properties.has_dynamic       = has_dynamic(res->fields, res->fields_cnt_total);

    return true;
}

ipx_tmpl_template_t *
template_create(size_t count)
{
    return malloc(sizeof(ipx_tmpl_template_t) + count * sizeof(struct ipx_tmpl_template_field));
}

ipx_tmpl_template_t *
template_copy(ipx_tmpl_template_t *src)
{
    ipx_tmpl_template_t *res = template_create(src->fields_cnt_total);
    *res = *src;
    res->raw.data = copy(src->raw.data, src->raw.length);
    return res;
}

ipx_tmpl_template_t *
template_copy_end(ipx_tmpl_t *tmpl, ipx_tmpl_template_t *src)
{
    ipx_tmpl_template_t *res = template_copy(src);
    res->time.end = tmpl->current.time;
    res->time.last = tmpl->current.time;
    res->next = src;
    return res;
}

int
template_convert(ipx_tmpl_t *tmpl, const struct ipfix_template_record *rec, uint16_t max_len,
                 struct ipx_tmpl_template *res)
{
    uint16_t offset = 0;
    ipx_get_uint_be(&rec->count , INT16_SIZE, (uint64_t*) &res->fields_cnt_total);
    const int len = fields_parse(tmpl, res->fields, rec->fields, res->fields_cnt_total,
                                 max_len - TEMPL_HEAD_SIZE, &offset);
    if (len < 0) {
        return len;
    }

    res->data_length = offset;
    if (!template_save_properties(tmpl, res, rec, (uint16_t) len)) {
        return IPX_ERR;
    }
    return res->raw.length;
}

/**
 * \brief Replace all previous definitions of elements in a template with new definitions
 * from a new iemgr
 * \param[in,out] tmpl Templater
 * \param[in]     mgr  Iemgr
 */
void
fields_reset_iemgr(ipx_tmpl_template_t *temp, fds_iemgr_t *mgr)
{
    for (int i = 0; i < temp->fields_cnt_total; ++i) {
        temp->fields[i].definition = fds_iemgr_elem_find_id(mgr, temp->id, temp->fields[i].id);
    }
}

int
templates_reset_iemgr(ipx_tmpl_t *tmpl, fds_iemgr_t *mgr)
{
    ipx_tmpl_template_t *res = NULL;
    ipx_tmpl_template_t *tmp = NULL;
    for (size_t i = 0; i < vectm_get_count(tmpl->templates); ++i) {
        tmp = template_copy_end(tmpl, vectm_get_template(tmpl->templates, i));
        if (tmp == NULL) {
            return IPX_NOMEM;
        }
        res = template_copy(tmp->next);
        if (res == NULL) {
            return IPX_ERR;
        }

        fields_reset_iemgr(res, mgr);
        res->next = tmp;
        vectm_set_index(tmpl, tmpl->templates, i, res);
    }
    return IPX_OK;
}

/**
 * \brief Change all definitions of elements to NULL in template
 * \param[in,out] temp Template
 */
void
fields_null_iemgr(ipx_tmpl_template_t *temp)
{
    for (int i = 0; i < temp->fields_cnt_total; ++i) {
        temp->fields[i].definition = NULL;
    }
}

int
templates_null_iemgr(ipx_tmpl_t* tmpl)
{
    ipx_tmpl_template_t *res = NULL;
    ipx_tmpl_template_t *tmp = NULL;
    for (size_t i = 0; i < vectm_get_count(tmpl->templates); ++i) {
        tmp = template_copy_end(tmpl, vectm_get_template(tmpl->templates, i));
        if (tmp == NULL) {
            return IPX_NOMEM;
        }
        res = template_copy(tmp->next);
        if (res == NULL) {
            return IPX_ERR;
        }

        fields_null_iemgr(res);
        res->next = tmp;
        vectm_set_index(tmpl, tmpl->templates, i, res);
    }
    return IPX_OK;
}

/**
 * \brief Check if all fields in template are identical with fields in record
 * \param[in] template Template
 * \param[in] rec      Template record
 * \param[in] max_len  Maximal length
 * \return True if are identical, otherwise false
 */
bool
fields_identical(const struct ipx_tmpl_template_field *fields, const union template_ie_u *elems,
                 uint16_t count, uint16_t max_len)
{
    int len = 0;
    for (int i = 0; i < count && len < max_len; ++i) {
        len += INT32_SIZE;
        if (elems[i].ie.id != fields[i].id) {
            return false;
        }
        if (elems[i].ie.length != fields[i].length) {
            return false;
        }
        if (NO_FIRST_BIT(elems[i].ie.id)) {
            continue;
        }

        len += INT32_SIZE;
        if (fields[i].en != elems[++i].enterprise_number) {
            return false;
        }
    }

    return true;
}

/**
 * \brief Check if templates are identical
 * \param[in] template Template
 * \param[in] rec      Template record
 * \param[in] max_len  Maximal length
 * \return
 */
bool
templates_identical(const ipx_tmpl_template_t *template, const struct ipfix_template_record *rec,
                    uint16_t max_len)
{
    if (template->id               != rec->template_id
     || template->fields_cnt_total != rec->count) {
        return false;
    }

    return fields_identical(template->fields, rec->fields, rec->count, max_len);
}

int
template_overwrite(ipx_tmpl_t *tmpl, ipx_tmpl_template_t *template,
                       const struct ipfix_template_record *rec, uint16_t max_len, ssize_t index)
{
    if (templates_identical(template, rec, max_len)) {
        return IPX_OK;
    }
    if (template->time.end == 0 && !tmpl->flag.can_overwrite) {
        return IPX_ERR;
    }
    if (tmpl->current.time < template->time.last) {
        return IPX_ERR;
    }

    ipx_tmpl_template_t *res = template_create(rec->count);
    int len = template_convert(tmpl, rec, max_len, res);
    if (len < 0) {
        return len;
    }

    res->next = template_copy_end(tmpl, template);
    vectm_set_index(tmpl, tmpl->templates, (size_t) index, res);
    return len;
}

int
template_add(ipx_tmpl_t *tmpl, const struct ipfix_template_record *rec, uint16_t max_len)
{
    uint64_t id;
    ipx_get_uint_be(&rec->template_id, INT16_SIZE, &id);
    struct ipx_tmpl_template *res = NULL;
    ssize_t index = vectm_find_index(tmpl->templates, (uint16_t) id);
    if (index >= 0) {
        res = vectm_get_template(tmpl->templates, (size_t) index);
        return template_overwrite(tmpl, res, rec, max_len, index);
    }

    uint64_t count;
    ipx_get_uint_be(&rec->count, INT16_SIZE, &count);
    res = template_create(count);
    if (res == NULL) {
        return IPX_NOMEM;
    }
    const int len = template_convert(tmpl, rec, max_len, res);
    if (len < 0) {
        free(res);
        return IPX_ERR;
    };
    vectm_add(tmpl, tmpl->templates, res);
    return len;
}

int
template_parse(ipx_tmpl_t *tmpl, const struct ipfix_template_record *rec, uint16_t max_len)
{
    uint64_t count;
    ipx_get_uint_be(&rec->count, 2, &count);
    if (count == 0) {
        uint64_t id;
        ipx_get_uint_be(&rec->template_id, 2, &id);
        ipx_tmpl_template_remove(tmpl, (uint16_t) id);
        return TEMPL_HEAD_SIZE;
    }
    return template_add(tmpl, rec, max_len);
}

int
templates_parse(ipx_tmpl_t* tmpl, struct ipfix_template_record* recs, uint16_t max_len)
{
    int tmp = 0;
    for (uint16_t len = TEMPL_SET_HEAD_SIZE; len < max_len; len += tmp) {
        tmp = template_parse(tmpl, recs, max_len - len);
        if (tmp < 0) {
            return tmp;
        }
        recs += tmp;
    }

    vectm_sort(tmpl->templates);
    return IPX_OK;
}

ipx_tmpl_t *
tmpl_time_find(const ipx_tmpl_t* src, uint64_t time)
{
    if (src == NULL) {
        return NULL;
    }

    const ipx_tmpl_t *res = NULL;
    res = src->snapshot;
    while(res != NULL) {
        if (res->current.time <= time) {
            return (ipx_tmpl_t*) res;
        }
        res = res->snapshot;
    }
    return NULL;
}

const ipx_tmpl_t *
tmpl_copy(ipx_tmpl_t *tmpl)
{
    ipx_tmpl_t *res = malloc(sizeof(ipx_tmpl_t));
    memcpy(res, tmpl, sizeof(ipx_tmpl_t));
    res->templates = vectm_copy(tmpl->templates);
    res->flag.modified = false;
    return res;
}

void
snapshot_create(ipx_tmpl_t* tmpl)
{
    if (tmpl->snapshot != NULL
        && (tmpl->current.time < tmpl->snapshot->current.time)) {
        return;
    }

    const ipx_tmpl_t *res = tmpl_copy(tmpl);
    tmpl->snapshot = res;
    tmpl->flag.modified = false;
}

ipx_tmpl_template_t *
template_find_with_time(const ipx_tmpl_t* tmpl, uint16_t id)
{
    if (tmpl->flag.modified) {
        snapshot_create((ipx_tmpl_t*) tmpl);
    }

    ipx_tmpl_t *tmp = tmpl_time_find(tmpl, tmpl->current.time);
    if (tmp == NULL) {
        return NULL;
    }

    ipx_tmpl_template_t *res = vectm_find(tmp->templates, id);
    if (res == NULL) {
        return NULL;
    }

    if ((res->time.end > tmpl->current.time || res->time.end == 0)
        && res->time.first <= tmpl->current.time) {
        return res;
    }
    return NULL;
}

void
template_destroy(ipx_tmpl_template_t *src)
{
    free(src->raw.data);
    free(src);
}

void
opts_template_save_properties(ipx_tmpl_t *tmpl, ipx_tmpl_template_t *res,
                              const struct ipfix_options_template_record *rec, uint16_t len)
{
    len += sizeof(rec) - sizeof(rec->fields);
    res->template_type                = IPX_TEMPLATE_OPTIONS;
    res->options_type                 = IPX_OPTS_NO_OPTIONS; // TODO
    ipx_get_uint_be(&rec->template_id, INT16_SIZE, (uint64_t*) &res->id);
    res->raw.data                     = copy((void *) rec, len);
    res->raw.length                   = len;
    res->time.first                   = tmpl->current.time;
    res->time.last                    = tmpl->current.time;
    res->time.end                     = 0;
    res->number_packet                = tmpl->current.count;
    res->next                     = NULL;
    ipx_get_uint_be(&rec->scope_field_count, INT16_SIZE, (uint64_t*) &res->fields_cnt_scope);
    res->properties.has_multiple_defs = has_multiple_definitions(res->fields, res->fields_cnt_total);
    res->properties.has_dynamic       = has_dynamic(res->fields, res->fields_cnt_total);
}

int
opts_template_convert(ipx_tmpl_t *tmpl, const struct ipfix_options_template_record *rec,
                      uint16_t max_len, ipx_tmpl_template_t *res)
{
    uint16_t offset = 0;
    ipx_get_uint_be(&rec->count, INT16_SIZE, (uint64_t*) &res->fields_cnt_total);
    const int len = fields_parse(tmpl, res->fields, rec->fields, res->fields_cnt_total,
                                 max_len - OPTS_TEMPL_HEAD_SIZE, &offset);
    if (len < 0) {
        return len;
    }

    res->data_length = offset;
    opts_template_save_properties(tmpl, res, rec, (uint16_t) len);
    return res->raw.length;
}

/**
 * \brief Check if templates are identical
 * \param[in] template Template
 * \param[in] rec      Template record
 * \param[in] max_len  Maximal length
 * \return
 */
bool
opts_templates_identical(const ipx_tmpl_template_t *template, const struct ipfix_options_template_record *rec,
                    uint16_t max_len)
{
    if (template->id               != rec->template_id
     || template->fields_cnt_total != rec->count
     || template->fields_cnt_scope != rec->scope_field_count) {
        return false;
    }

    return fields_identical(template->fields, rec->fields, rec->count, max_len); // TODO check scopes
}

int
opts_template_overwrite(ipx_tmpl_t *tmpl, ipx_tmpl_template_t *template,
                const struct ipfix_options_template_record *rec, uint16_t max_len, ssize_t index)
{
    if (opts_templates_identical(template, rec, max_len)) {
        return IPX_OK;
    }

    if (template->time.end == 0 && !tmpl->flag.can_overwrite) {
        return IPX_ERR;
    }
    if (tmpl->current.time < template->time.last) {
        return IPX_ERR;
    }

    ipx_tmpl_template_t *res = template_create(rec->count);
    int len = opts_template_convert(tmpl, rec, max_len, res);
    if (len < 0) {
        return len;
    }

    res->next = template_copy_end(tmpl, template);
    vectm_set_index(tmpl, tmpl->templates, (size_t) index, res);
    return len;
}

int
opts_template_add(ipx_tmpl_t *tmpl, const struct ipfix_options_template_record *rec,
                  uint16_t max_len)
{
    struct ipx_tmpl_template *res = NULL;
    ssize_t index = vectm_find_index(tmpl->templates, rec->template_id);
    if (index >= 0) {
        res = vectm_get_template(tmpl->templates, (size_t) index);
        return opts_template_overwrite(tmpl, res, rec, max_len, index);
    }

    uint64_t count;
    ipx_get_uint_be(&rec->count, INT16_SIZE, &count);
    res = template_create(count);
    if (res == NULL) {
        return IPX_NOMEM;
    }
    const int len = opts_template_convert(tmpl, rec, max_len, res);
    if (len < 0) {
        free(res);
        return IPX_ERR;
    };
    vectm_add(tmpl, tmpl->templates, res);
    return len;
}

int
ops_templates_parse(ipx_tmpl_t* tmpl, struct ipfix_options_template_record* recs,
                    uint16_t max_len)
{
    int tmp = 0;
    for (uint16_t len = TEMPL_SET_HEAD_SIZE; len < max_len; len += tmp) {
        tmp = ipx_tmpl_options_template_parse(tmpl, recs++, max_len - len);
        if (tmp < 0) {
            return tmp;
        }
    }

    vectm_sort(tmpl->templates);
    return IPX_OK;
}

int
template_remove(ipx_tmpl_t *tmpl, size_t index)
{
    ipx_tmpl_template_t *res = vectm_get_template(tmpl->templates, index);
    if (res->time.end != 0) { // TODO Withdrawal of template that was previously withdrawal
        return IPX_OK;
    }
    vectm_set_index(tmpl, tmpl->templates, index, template_copy_end(tmpl, res));
    return IPX_OK;
}

int
template_remove_all_with_id(ipx_tmpl_t *tmpl, size_t index)
{
    template_remove(tmpl, index);
    vectm_set_die_time(tmpl->templates, index, 1);
    return IPX_OK;
}

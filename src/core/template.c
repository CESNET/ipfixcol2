/**
 * \file   src/core/template.h
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief  Template (source file)
 * \date   October 2017
 */
/*
 * Copyright (C) 2017 CESNET, z.s.p.o.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of the Company nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * ALTERNATIVELY, provided that this notice is retained in full, this
 * product may be distributed under the terms of the GNU General Public
 * License (GPL) version 2 or later, in which case the provisions
 * of the GPL apply INSTEAD OF those given above.
 *
 * This software is provided ``as is'', and any express or implied
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose are disclaimed.
 * In no event shall the company or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even
 * if advised of the possibility of such damage.
 *
 */

#include <ipfixcol2/template.h>
#include <ipfixcol2/ipfix_structures.h>
#include <stddef.h> // size_t
#include <arpa/inet.h> // ntohs
#include <string.h> // memcpy
#include <stdlib.h>
#include <assert.h>
#include <ipfixcol2/message_ipfix.h>


#define TEMPLATE_STRUCT_SIZE(elem_cnt) \
    sizeof(struct ipfix_template_record) \
    - sizeof(template_ie) \
    + ((elem_cnt) * sizeof(template_ie))


/** Return only first bit from a _value_ */
#define EN_BIT_GET(_value_)  ((_value_) & (uint16_t) 0x8000)
/** Return _value_ without the first bit */
#define EN_BIT_MASK(_value_) ((_value_) & (uint16_t )0x7FFF)


static inline struct ipx_template *
template_create_empty(uint16_t field_cnt)
{
    return calloc(1, TEMPLATE_STRUCT_SIZE(field_cnt));
}

static int
template_parse_header(enum ipx_template_type type, const void *ptr, uint16_t *len,
    struct ipx_template **tmplt)
{
    assert(type == IPX_TYPE_TEMPLATE || type == IPX_TYPE_TEMPLATE_OPTIONS);
    const size_t size_normal = sizeof(struct ipfix_template_record) - sizeof(template_ie);
    const size_t size_opts = sizeof(struct ipfix_options_template_record) - sizeof(template_ie);

    uint16_t template_id;
    uint16_t fields_total;
    uint16_t fields_scope = 0;
    uint16_t header_size = size_normal;

    if (*len < size_normal) { // the header must be at least 4 bytes long
        return IPX_ERR_FORMAT;
    }

    /*
     * Because Options Template header is superstructure of "Normal" Template header we can use it
     * also for parsing "Normal" Template. Just use only shared fields...
     */
    struct ipfix_options_template_record *rec = ptr;
    template_id = ntohs(rec->template_id);
    if (template_id < IPFIX_SET_MIN_DATA_SET_ID) {
        return IPX_ERR_FORMAT;
    }

    fields_total = ntohs(rec->count);
    if (fields_total != 0 && type == IPX_TYPE_TEMPLATE_OPTIONS) {
        // It is not a withdrawal template, so it must be definitely an Options Template
        if (*len < size_opts) { // the header must be at least 6 bytes long
            return IPX_ERR_FORMAT;
        }

        header_size = size_opts;
        fields_scope = ntohs(rec->scope_field_count);
        if (fields_scope == 0 || fields_scope > fields_total) {
            return IPX_ERR_FORMAT;
        }
    }

    struct ipx_template *ptr = template_create_empty(fields_total);
    if (!ptr) {
        return IPX_ERR_NOMEM;
    }

    ptr->type = type;
    ptr->id = template_id;
    ptr->fields_cnt_total = fields_total;
    ptr->fields_cnt_scope = fields_scope;
    *tmplt = ptr;
    *len = header_size;
    return IPX_OK;
}

static int
template_parse_fields(struct ipx_template *tmplt, template_ie *field_ptr, uint16_t *len)
{
    const uint16_t field_size = sizeof(template_ie);
    const uint16_t fields_cnt = tmplt->fields_cnt_total;
    struct ipx_tfield *tfield_ptr = &tmplt->fields[0];
    uint16_t len_remain = *len;

    for (uint16_t i = 0; i < fields_cnt; ++i, ++tfield_ptr, ++field_ptr, len_remain -= field_size) {
        // Parse Information Element ID
        if (len_remain < field_size) {
            // Unexpected end of the template
            return IPX_ERR_FORMAT;
        }

        tfield_ptr->length = ntohs(field_ptr->ie.length);
        tfield_ptr->id = ntohs(field_ptr->ie.id);
        if (EN_BIT_GET(tfield_ptr->id) == 0) {
            continue;
        }

        // Parse Enterprise Number
        len_remain -= field_size;
        if (len_remain < field_size) {
            // Unexpected end of the template
            return IPX_ERR_FORMAT;
        }

        ++field_ptr;
        tfield_ptr->id = EN_BIT_MASK(tfield_ptr->id);
        tfield_ptr->en = ntohl(field_ptr->enterprise_number);
    }

    *len -= len_remain;
    return IPX_OK;
}

/**
 *
 * \note Only the following flags could be set:
 *   - ::IPX_TFIELD_SCOPE
 *   - ::IPX_TFIELD_MULTI_IE
 *   - ::IPX_TFIELD_LAST_IE
 * @param tmplt
 */
static int
template_fields_calc_features(struct ipx_template *tmplt)
{
    const uint16_t fields_total = tmplt->fields_cnt_total;
    const uint16_t fields_scope = tmplt->fields_cnt_scope;

    // Label Scope fields
    for (uint16_t i = 0; i < fields_scope; ++i) {
        tmplt->fields[i].flags |= IPX_TFIELD_SCOPE;
    }

    // Label Multiple and Last fields
    uint64_t hash = 0;

    for (int i = fields_total - 1; i >= 0; --i) {
        struct ipx_tfield *tfield_ptr = &tmplt->fields[i];

        // Calculate "hash" from IE ID
        uint64_t my_hash = (1 << (tfield_ptr->id % 64));
        if ((hash & my_hash) == 0) {
            // No one has the same "hash" -> this is definitely the last
            tfield_ptr->flags |= IPX_TFIELD_LAST_IE;
            hash |= my_hash;
            continue;
        }

        // Someone has the same hash. Let's check if there is already exactly the same IE.
        bool same_found = false;
        for (uint16_t x = i + 1; i < fields_total; ++i) {
            struct ipx_tfield *tfield_older = &tmplt->fields[x];
            if (tfield_ptr->id != tfield_older->id || tfield_ptr->en != tfield_older->en) {
                continue;
            }

            // Oh... we have a match
            tfield_ptr->flags |= IPX_TFIELD_MULTI_IE;
            tfield_older->flags |= IPX_TFIELD_MULTI_IE;
            same_found = true;
            break;
        }

        if (!same_found) {
            tfield_ptr->flags |= IPX_TFIELD_LAST_IE;
        }
    }

    return IPX_OK;
}



static int
template_calc_features(struct ipx_template *tmplt)
{
    // First, calculate basic flags of each field
    int ret_code;
    if ((ret_code = template_fields_calc_features(tmplt)) != IPX_OK) {
        return ret_code;
    }

    // Calculate flags of the whole template
    const uint16_t fields_total = tmplt->fields_cnt_total;
    uint32_t data_len = 0; // Get (minimum) data length of a record referenced by this template

    for (uint16_t i = 0; i < fields_total; ++i) {
        const struct ipx_tfield *field_ptr = &tmplt->fields[i];
        if (field_ptr->flags & IPX_TFIELD_MULTI_IE) {
            tmplt->flags |= IPX_TEMPLATE_HAS_MULTI_IE;
        }

        uint16_t field_len = field_ptr->length;
        if (field_len == IPFIX_VAR_IE_LENGTH) {
            // Variable length Information Element must be at least 1 byte long
            tmplt->flags |= IPX_TEMPLATE_HAS_DYNAMIC;
            data_len += 1;
            continue;
        }

        data_len += field_len;
    }

    // Check if a record described by this templates fits into an IPFIX message
    const uint16_t max_rec_size = UINT16_MAX // Maximum length of an IPFIX message
        - sizeof(struct ipfix_header)        // IPFIX message header
        - sizeof(struct ipfix_set_header);   // IPFIX set header
    if (max_rec_size < data_len) {
        // Too long data record
        return IPX_ERR_FORMAT;
    }

    // Recognize Options Template
    if (tmplt->type == IPX_TYPE_TEMPLATE_OPTIONS) {
        // TODO
    }

    tmplt->data_length = data_len;
    return IPX_OK;
}

int
ipx_template_parse(enum ipx_template_type type, const void *ptr, uint16_t *len,
    struct ipx_template **tmplt)
{
    assert(type == IPX_TYPE_TEMPLATE || type == IPX_TYPE_TEMPLATE_OPTIONS);
    struct ipx_template *template;
    uint16_t len_header, len_fields, len_real;
    int ret_code;

    // Parse a header
    len_header = *len;
    ret_code = template_parse_header(type, ptr, &len_header, &template);
    if (ret_code != IPX_OK) {
        return ret_code;
    }

    if (template->fields_cnt_total == 0) {
        // No fields
        *len = len_header;
        *tmplt = template;
        return IPX_OK;
    }

    // Parse fields
    template_ie *fields_ptr = (template_ie *)(((uint8_t *) ptr) + len_header);
    len_fields = *len - len_header;
    ret_code = template_parse_fields(template, fields_ptr, &len_fields);
    if (ret_code != IPX_OK) {
        ipx_template_destroy(template);
        return ret_code;
    }

    // Copy raw
    len_real = len_header + len_fields;
    template->raw.length = len_real;
    template->raw.data = (uint8_t *) malloc(len_real);
    if (!template->raw.data) {
        ipx_template_destroy(template);
        return IPX_ERR_NOMEM;
    }

    // Calculate features of fields and the template
    ret_code = template_calc_features(template);
    if (ret_code != IPX_OK) {
        ipx_template_destroy(template);
        return ret_code;
    }

    *len = len_real;
    *tmplt = template;
    return IPX_OK;
}


struct ipx_template *
ipx_template_copy(const struct ipx_template *tmplt)
{
    const size_t size_main = TEMPLATE_STRUCT_SIZE(tmplt->fields_cnt_total);
    const size_t size_raw = tmplt->raw.length;

    struct ipx_template *cpy_main = malloc(size_main);
    uint8_t *cpy_raw = malloc(size_raw);
    if (!cpy_main || !cpy_raw) {
        free(cpy_main);
        free(cpy_raw);
        return NULL;
    }

    memcpy(cpy_main, tmplt, size_main);
    memcpy(cpy_raw, tmplt->raw.data, size_raw);
    cpy_main->raw.data = cpy_raw;
    return cpy_main;
}

void
ipx_template_destroy(struct ipx_template *tmplt)
{
    free(tmplt->raw.data);
    free(tmplt);
}
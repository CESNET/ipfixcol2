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

/**
 * Calculate size of template structure (based on number of fields)
 */
#define TEMPLATE_STRUCT_SIZE(elem_cnt) \
    sizeof(struct ipfix_template_record) \
    - sizeof(template_ie) \
    + ((elem_cnt) * sizeof(template_ie))

/** Return only first bit from a _value_ */
#define EN_BIT_GET(_value_)  ((_value_) & (uint16_t) 0x8000)
/** Return _value_ without the first bit */
#define EN_BIT_MASK(_value_) ((_value_) & (uint16_t )0x7FFF)
/** Get the length of an array */
#define ARRAY_SIZE(_arr_) (sizeof(_arr_) / sizeof((_arr_)[0]))

/**
 * \brief Find the first occurrence of a field in a template
 * \param[in] tmplt Template structure
 * \param[in] id    Information Element ID
 * \param[in] en    Enpterprise Number (PEN)
 * \return Pointer to the field definition or NULL.
 */
static const struct ipx_tfield *
opts_find_field(const struct ipx_template *tmplt, uint16_t id, uint32_t en)
{
    const uint16_t field_cnt = tmplt->fields_cnt_total;
    for (uint16_t i = 0; i < field_cnt; ++i) {
        const struct ipx_tfield *ptr = &tmplt->fields[i];
        if (ptr->id != id || ptr->en != en) {
            continue;
        }

        return ptr;
    }

    return NULL;
}

/** Required field identification            */
struct opts_req_id {
    uint16_t id; /**< Information Element ID */
    uint32_t en; /**< Enterprise Number      */
};

/**
 * \brief Check presence of required non-scope Information Elements (IEs)
 * \note All scope IEs are ignored!
 * \param[in] tmplt   Template structure
 * \param[in] recs    Array of required Information Elements
 * \param[in] rec_cnt Number of elements in the array
 * \return If all required IEs are present, the function will return true. Otherwise, returns false.
 */
static bool
opts_has_required(const struct ipx_template *tmplt, const struct opts_req_id *recs,
    size_t rec_cnt)
{
    const uint16_t idx_start = tmplt->fields_cnt_scope;
    const uint16_t idx_end = tmplt->fields_cnt_total;

    // Try to find each required field
    for (size_t rec_idx = 0; rec_idx < rec_cnt; ++rec_idx) {
        const struct opts_req_id rec = recs[rec_idx];
        const struct ipx_tfield *field_ptr = NULL;
        uint16_t field_idx;

        for (field_idx = idx_start; field_idx < idx_end; ++field_idx) {
            field_ptr = &tmplt->fields[field_idx];
            if (rec.id != field_ptr->id || rec.en != field_ptr->en) {
                continue;
            }

            break;
        }

        if (field_idx == idx_end) {
            // Required field not found!
            return false;
        }
    }

    return true;
}

/**
 * \brief Check presence of non-scope observation time interval
 *
 * The function will try to find any 2 "observationTimeXXX" Information Elements, where XXX is
 * one of following: Seconds, Milliseconds, Microseconds, Nanoseconds.
 * \note All scope IEs are ignored!
 * \param[in] tmplt Template structure
 * \return If the interval is present, returns true. Otherwise, returns false.
 */
static bool
opts_has_obs_time(const struct ipx_template *tmplt)
{
    unsigned int matches = 0;

    const uint16_t fields_start = tmplt->fields_cnt_scope;
    const uint16_t fields_end = tmplt->fields_cnt_total;
    for (uint16_t i = fields_start; i < fields_end; ++i) {
        const struct ipx_tfield *field_ptr = &tmplt->fields[i];
        if (field_ptr->en != 0) {
            continue;
        }

        /* We are looking for IEs observationTimeXXX with different precision
         * observationTimeSeconds (322) - observationTimeNanoseconds (325)
         */
        if (field_ptr->id < 322 || field_ptr->id > 325) {
            continue;
        }

        matches++;
        if (matches > 2) {
            // Too many matches
            return false;
        }
    }
    return (matches == 2);
}

/**
 * \brief Detect Options Template types of Metering Process
 *
 * If one or more types are detected, appropriate flag(s) will be set.
 * \note Based on RFC 7011, Section 4.1. - 4.2..
 * \param[in] tmplt Template structure
 */
static void
opts_detect_mproc(struct ipx_template *tmplt)
{
    // Metering Process Template
    const uint16_t IPFIX_IE_ODID = 149; // observationDomainId
    const uint16_t IPFIX_IE_MPID = 143; // meteringProcessId
    const struct ipx_tfield *odid_ptr = opts_find_field(tmplt, IPFIX_IE_ODID, 0);
    const struct ipx_tfield *mpid_ptr = opts_find_field(tmplt, IPFIX_IE_MPID, 0);
    if (odid_ptr == NULL && mpid_ptr == NULL) {
        // At least one field must be defined
        return;
    }

    // Check scope fields
    const struct ipx_tfield *ptrs[] = {odid_ptr, mpid_ptr};
    for (int i = 0; i < ARRAY_SIZE(ptrs); ++i) {
        const struct ipx_tfield *ptr = ptrs[i];
        if (ptr == NULL) {
            // Item not found, skip
            continue;
        }

        if (ptr->flags & IPX_TFIELD_SCOPE == 0) {
            // The field found, but not in the scope!
            return;
        }

        if (ptr->flags & IPX_TFIELD_MULTI_IE) {
            // Multiple definitions are not expected!
            return;
        }
    }

    // Check non-scope fields
    static const struct opts_req_id ids_mproc[] = {
        {40, 0}, // exportedOctetTotalCount
        {41, 0}, // exportedMessageTotalCount
        {42, 0}  // exportedFlowRecordTotalCount
    };

    if (opts_has_required(tmplt, ids_mproc, ARRAY_SIZE(ids_mproc))) {
        // Ok, this is definitely "The Metering Process Statistics Options Template"
        tmplt->opts_types |= IPX_OPTS_MPROC_STAT;
    }

    static const struct opts_req_id ids_mproc_stat[] = {
        {164, 0}, // ignoredPacketTotalCount
        {165, 0}  // ignoredOctetTotalCount
    };
    if (!opts_has_required(tmplt, ids_mproc_stat, ARRAY_SIZE(ids_mproc_stat))) {
        // Required fields not found
        return;
    }

    if (opts_has_obs_time(tmplt)) {
        // Ok, this is definitely "The Metering Process Reliability Statistics Options Template"
        tmplt->opts_types |= IPX_OPTS_MPROC_RELIABILITY_STAT;
    }
}

/**
 * \brief Detect Options Template type of Exporting Process
 *
 * If the type is detected, an appropriate flag will be set.
 * \note Based on RFC 7011, Section 4.3.
 * \param[in] tmplt Template structure
 */
static void
opts_detect_eproc(struct ipx_template *tmplt)
{
    const uint16_t IPFIX_IE_EXP_IPV4 = 130; // exporterIPv4Address
    const uint16_t IPFIX_IE_EXP_IPV6 = 131; // exporterIPv6Address
    const uint16_t IPFIX_IE_EXP_PID = 144;  // exportingProcessId

    // Check scope fields
    bool eid_found = false;
    const uint16_t eid[] = {IPFIX_IE_EXP_IPV4, IPFIX_IE_EXP_IPV6, IPFIX_IE_EXP_PID};
    for (size_t i = 0; i < ARRAY_SIZE(eid); ++i) {
        const struct ipx_tfield *field_ptr = opts_find_field(tmplt, eid[i], 0);
        if (!field_ptr) {
            // Not found
            continue;
        }

        if (field_ptr->flags & IPX_TFIELD_SCOPE && field_ptr->flags & IPX_TFIELD_LAST_IE) {
            eid_found = true;
            break;
        }
    }

    if (!eid_found) {
        return;
    }

    // Check non-scope fields
    static const struct opts_req_id ids_exp[] = {
        {166, 0}, // notSentFlowTotalCount
        {167, 0}, // notSentPacketTotalCount
        {168, 0}  // notSentOctetTotalCount
    };
    if (!opts_has_required(tmplt, ids_exp, ARRAY_SIZE(ids_exp))) {
        // Required fields not found
        return;
    }

    if (opts_has_obs_time(tmplt)) {
        // Ok, this is definitely "The Exporting Process Reliability Statistics Options Template"
        tmplt->opts_types |= IPX_OPTS_EPROC_RELIABILITY_STAT;
    }
}

/**
 * \brief Detect Options Template type of Flow keys
 *
 * If the type is detected, an appropriate flag will be set.
 * \note Based on RFC 7011, Section 4.4.
 * \param[in] tmplt Template structure
 */
static void
opts_detect_flowkey(struct ipx_template *tmptl)
{
    // Check scope Field
    const uint16_t IPFIX_IE_TEMPLATE_ID = 145;
    struct ipx_template *id_ptr = opts_find_field(tmptl, IPFIX_IE_TEMPLATE_ID, 0);
    if (id_ptr == NULL) {
        // Not found
        return;
    }

    if (id_ptr->flags & IPX_TFIELD_SCOPE == 0 || id_ptr->flags & IPX_TFIELD_MULTI_IE) {
        // Not scope field or multiple definitions
        return;
    }

    // Check non-scope fields
    static const struct opts_req_id ids_key[] = {
        {173, 0} // flowKeyIndicator
    };
    if (opts_has_required(tmptl, ids_key, ARRAY_SIZE(ids_key))) {
        // Ok, this is definitely "The Flow Keys Options Template"
        tmptl->opts_types |= IPX_OPTS_FKEYS;
    }
}

/**
 * \brief Detect Options Template type of Information Element definition
 *
 * If the type is detected, an appropriate flag will be set.
 * \note Based on RFC 5610, Section 3.9.
 * \param[in] tmplt Template structure
 */
static void
opts_detect_ietype(struct ipx_template *tmplt)
{
    const uint16_t IPX_IE_IE_ID = 303; // informationElementId
    const uint16_t IPX_IE_PEN = 346; // privateEnterpriseNumber
    const struct ipx_tfield *ie_id_ptr = opts_find_field(tmplt, IPX_IE_IE_ID, 0);
    const struct ipx_tfield *pen_ptr = opts_find_field(tmplt, IPX_IE_PEN, 0);

    // Check scope fields
    const struct ipx_tfield *ptrs[] = {ie_id_ptr, pen_ptr};
    for (int i = 0; i < ARRAY_SIZE(ptrs); ++i) {
        const struct ipx_tfield *ptr = ptrs[i];
        if (ptr == NULL) {
            // Required item not found
            return;
        }

        if (ptr->flags & IPX_TFIELD_SCOPE == 0) {
            // The field found, but not in the scope!
            return;
        }

        if (ptr->flags & IPX_TFIELD_MULTI_IE) {
            // Multiple definitions are not expected!
            return;
        }
    }

    // Mandatory non-scope fields
    static const struct opts_req_id ids_type[] = {
        {339, 0}, // informationElementDataType
        {344, 0}, // informationElementSemantics
        {341, 0}  // informationElementName
    };
    if (!opts_has_required(tmplt, ids_type, ARRAY_SIZE(ids_type))) {
        // Ok, this is definitely "The Information Element Type Options Template"
        tmplt->opts_types |= IPX_OPTS_IE_TYPE;
    }
}

/**
 * \brief Detect all known types of Options Template and set appropriate flags.
 * \param[in] tmplt Template structure
 */
static void
opts_detect(struct ipx_template *tmplt)
{
    assert(tmplt->type == IPX_TYPE_TEMPLATE_OPTIONS);

    opts_detect_mproc(tmplt);
    opts_detect_eproc(tmplt);
    opts_detect_flowkey(tmplt);
    opts_detect_ietype(tmplt);
}

/**
 * \brief Create an empty template structure
 * \note All parameters are set to zero.
 * \param[in] field_cnt Number of Field Specifiers
 * \return Point to the structure or NULL.
 */
static inline struct ipx_template *
template_create_empty(uint16_t field_cnt)
{
    return calloc(1, TEMPLATE_STRUCT_SIZE(field_cnt));
}

/**
 * \brief Parse a raw template header and create a new template structure
 *
 * The new template structure will be prepared for adding appropriate number of Field Specifiers
 * based on information from the raw template.
 * \param[in]      type  Type of the template
 * \param[in]      ptr   Pointer to the template header
 * \param[in, out] len   [in] Maximal length of the raw template /
 *                       [out] real length of the header of the raw template (in octets)
 * \param[out]     tmplt New template structure
 * \return On success, the function will set the parameters \p len,\p tmplt and return #IPX_OK.
 *   Otherwise, the parameters will be unchanged and the function will return #IPX_ERR_FORMAT
 *   or #IPX_ERR_NOMEM.
 */
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

/**
 * \brief Parse Field Specifiers of a raw template
 *
 * Go through the Field Specifiers of the raw template and add them into the structure of the
 * parsed template \p tmplt.
 * \param[in]     tmplt     Template structure
 * \param[in]     field_ptr Pointer to the first specifier.
 * \param[in,out] len       [in] Maximal remaining length of the raw template /
 *                          [out] real length of the raw Field Specifiers (in octets).
 * \return On success, the function will set the parameter \p len and return #IPX_OK. Otherwise,
 *   the parameter will be unchanged and the function will return #IPX_ERR_FORMAT
 */
static int
template_parse_fields(struct ipx_template *tmplt, const template_ie *field_ptr, uint16_t *len)
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
 * \brief Set feature flags of all Field Specifiers in a template
 *
 * Only ::IPX_TFIELD_SCOPE, ::IPX_TFIELD_MULTI_IE, and ::IPX_TFIELD_LAST_IE can be determined
 * based on a structure of the template. Other flags require external information.
 * \note Global flags of the template as a whole are not modified.
 * \param[in] tmplt Template structure
 */
static void
template_fields_calc_flags(struct ipx_template *tmplt)
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
        uint64_t my_hash = (1ULL << (tfield_ptr->id % 64));
        if ((hash & my_hash) == 0) {
            // No one has the same "hash" -> this is definitely the last
            tfield_ptr->flags |= IPX_TFIELD_LAST_IE;
            hash |= my_hash;
            continue;
        }

        // Someone has the same hash. Let's check if there is exactly the same IE.
        bool same_found = false;
        for (int x = i + 1; i < fields_total; ++i) {
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
}

/**
 * \brief Calculate template parameters
 *
 * Feature flags of each Field Specifier will set as described in the documentation of the
 * template_fields_calc_flags() function. Regarding the global feature flags of the template,
 * only the features ::IPX_TEMPLATE_HAS_MULTI_IE and ::IPX_TEMPLATE_HAS_DYNAMIC of the template
 * will be detected and set. The expected length of appropriate data records will be calculated
 * based on the length of individual Specifiers.
 *
 * In case the template is Option Template, the function will also try to detect known type(s).
 * \param[in] tmplt Template structure
 * \return On success, returns #IPX_OK. If any parameter is not valid, the function will return
 *   #IPX_ERR_FORMAT.
 */
static int
template_calc_features(struct ipx_template *tmplt)
{
    // First, calculate basic flags of each field
    template_fields_calc_flags(tmplt)

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
        opts_detect(tmplt);
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

    // TODO: raw?
    if (template->fields_cnt_total == 0) {
        // No fields
        *len = len_header;
        *tmplt = template;
        return IPX_OK;
    }

    // Parse fields
    const template_ie *fields_ptr = (template_ie *)(((uint8_t *) ptr) + len_header);
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
    memcpy(template->raw.data, ptr, len_real);

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

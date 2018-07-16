/**
 * \file translator.c
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \author Imrich Stoffa <xstoff02@stud.fit.vutbr.cz>
 * \brief Conversion of IPFIX to LNF format (source file)
 */

/* Copyright (C) 2015 - 2017 CESNET, z.s.p.o.
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
 * This software is provided ``as is, and any express or implied
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

#include <string.h>
#include <inttypes.h>
#include "lnfstore.h"
#include "translator.h"

// Prototypes
struct translator_table_rec;
static int
translate_uint(const struct fds_drec_field *field,
        const struct translator_table_rec *def, uint8_t *buffer_ptr);
static int
translate_ip(const struct fds_drec_field *field,
        const struct translator_table_rec *def, uint8_t *buffer_ptr);
static int
translate_mac(const struct fds_drec_field *field,
        const struct translator_table_rec *def, uint8_t *buffer_ptr);
static int
translate_tcpflags(const struct fds_drec_field *field,
        const struct translator_table_rec *def, uint8_t *buffer_ptr);
static int
translate_time(const struct fds_drec_field *field,
        const struct translator_table_rec *def, uint8_t *buffer_ptr);

/**
 * \typedef translator_func
 * \brief Definition of conversion function
 * \param[in]  field      Pointer to an IPFIX field to convert
 * \param[in]  def        Definition of data conversion
 * \param[out] buffer_ptr Pointer to the conversion buffer where LNF value
 *   will be stored
 * \return On success return 0. Otherwise returns a non-zero value.
 */
typedef int (*translator_func)(const struct fds_drec_field *field,
    const struct translator_table_rec *def, uint8_t *buffer_ptr);

/**
 * \brief Conversion record
 */
struct translator_table_rec
{
    /** Identification of the IPFIX Information Element              */
    struct ipfix_s {
        /** Private Enterprise Number of the Information Element     */
        uint32_t pen;
        /** ID of the Information Element within the PEN             */
        uint16_t ie;
    } ipfix;

    /** Identification of the corresponding LNF Field */
    struct lnf_s {
        /** Field identification                      */
        int id;
        /** Internal size of the Field                */
        int size;
        /** Internal type of the Field                */
        int type;
    } lnf;

    /** Conversion function */
    translator_func func;
};

/**
 * \brief Global translator table
 * \warning Size of each LNF field is always 0 because a user must create its
 *   own instance of the table and fill the correct size from LNF using
 *   lnf_fld_info API function.
 */
static const struct translator_table_rec translator_table_global[] = {
    {{0,   1}, {LNF_FLD_DOCTETS,     0, 0}, translate_uint},
    {{0,   2}, {LNF_FLD_DPKTS,       0, 0}, translate_uint},
    {{0,   3}, {LNF_FLD_AGGR_FLOWS,  0, 0}, translate_uint},
    {{0,   4}, {LNF_FLD_PROT,        0, 0}, translate_uint},
    {{0,   5}, {LNF_FLD_TOS,         0, 0}, translate_uint},
    {{0,   6}, {LNF_FLD_TCP_FLAGS,   0, 0}, translate_tcpflags},
    {{0,   7}, {LNF_FLD_SRCPORT,     0, 0}, translate_uint},
    {{0,   8}, {LNF_FLD_SRCADDR,     0, 0}, translate_ip},
    {{0,   9}, {LNF_FLD_SRC_MASK,    0, 0}, translate_uint},
    {{0,  10}, {LNF_FLD_INPUT,       0, 0}, translate_uint},
    {{0,  11}, {LNF_FLD_DSTPORT,     0, 0}, translate_uint},
    {{0,  12}, {LNF_FLD_DSTADDR,     0, 0}, translate_ip},
    {{0,  13}, {LNF_FLD_DST_MASK,    0, 0}, translate_uint},
    {{0,  14}, {LNF_FLD_OUTPUT,      0, 0}, translate_uint},
    {{0,  15}, {LNF_FLD_IP_NEXTHOP,  0, 0}, translate_ip},
    {{0,  16}, {LNF_FLD_SRCAS,       0, 0}, translate_uint},
    {{0,  17}, {LNF_FLD_DSTAS,       0, 0}, translate_uint},
    {{0,  18}, {LNF_FLD_BGP_NEXTHOP, 0, 0}, translate_ip},
    /* These elements are not supported, because of implementation complexity.
     * However, these elements are not very common in IPFIX flows and in case
     * of Netflow flows, these elements are converted in the preprocessor to
     * timestamp in flowStartMilliseconds/flowEndMilliseconds.
    {{0,  21}, {LNF_FLD_LAST,        0, 0}, translate_time},
    {{0,  22}, {LNF_FLD_FIRST,       0, 0}, translate_time},
    */
    {{0,  23}, {LNF_FLD_OUT_BYTES,   0, 0}, translate_uint},
    {{0,  24}, {LNF_FLD_OUT_PKTS,    0, 0}, translate_uint},
    {{0,  27}, {LNF_FLD_SRCADDR,     0, 0}, translate_ip},
    {{0,  28}, {LNF_FLD_DSTADDR,     0, 0}, translate_ip},
    {{0,  29}, {LNF_FLD_SRC_MASK,    0, 0}, translate_uint},
    {{0,  30}, {LNF_FLD_DST_MASK,    0, 0}, translate_uint},
    /* LNF_FLD_ specific id missing, DSTPORT overlaps
    {{0,  32}, {LNF_FLD_DSTPORT,     0, 0}, translate_uint },
    */
    {{0,  38}, {LNF_FLD_ENGINE_TYPE, 0, 0}, translate_uint},
    {{0,  39}, {LNF_FLD_ENGINE_ID,   0, 0}, translate_uint},
    {{0,  55}, {LNF_FLD_DST_TOS,     0, 0}, translate_uint},
    {{0,  56}, {LNF_FLD_IN_SRC_MAC,  0, 0}, translate_mac},
    {{0,  57}, {LNF_FLD_OUT_DST_MAC, 0, 0}, translate_mac},
    {{0,  58}, {LNF_FLD_SRC_VLAN,    0, 0}, translate_uint},
    {{0,  59}, {LNF_FLD_DST_VLAN,    0, 0}, translate_uint},
    {{0,  61}, {LNF_FLD_DIR,         0, 0}, translate_uint},
    {{0,  62}, {LNF_FLD_IP_NEXTHOP,  0, 0}, translate_ip},
    {{0,  63}, {LNF_FLD_BGP_NEXTHOP, 0, 0}, translate_ip},
    /* Not implemented
    {{0,  70}, { LNF_FLD_MPLS_LABEL, 0, 0}, translate_mpls}, //this refers to base of stack
    {{0,  71}, { LNF_FLD_MPLS_LABEL, 0, 0}, translate_mpls},
    {{0,  72}, { LNF_FLD_MPLS_LABEL, 0, 0}, translate_mpls},
    {{0,  73}, { LNF_FLD_MPLS_LABEL, 0, 0}, translate_mpls},
    {{0,  74}, { LNF_FLD_MPLS_LABEL, 0, 0}, translate_mpls},
    {{0,  75}, { LNF_FLD_MPLS_LABEL, 0, 0}, translate_mpls},
    {{0,  76}, { LNF_FLD_MPLS_LABEL, 0, 0}, translate_mpls},
    {{0,  77}, { LNF_FLD_MPLS_LABEL, 0, 0}, translate_mpls},
    {{0,  78}, { LNF_FLD_MPLS_LABEL, 0, 0}, translate_mpls},
    {{0,  79}, { LNF_FLD_MPLS_LABEL, 0, 0}, translate_mpls},
    */
    {{0,  80}, {LNF_FLD_OUT_SRC_MAC, 0, 0}, translate_mac},
    {{0,  81}, {LNF_FLD_IN_DST_MAC,  0, 0}, translate_mac},
    {{0,  89}, {LNF_FLD_FWD_STATUS,  0, 0}, translate_uint},
    {{0, 128}, {LNF_FLD_BGPNEXTADJACENTAS, 0, 0}, translate_uint},
    {{0, 129}, {LNF_FLD_BGPPREVADJACENTAS, 0, 0}, translate_uint},
    {{0, 130}, {LNF_FLD_IP_ROUTER,   0, 0}, translate_ip},
    {{0, 131}, {LNF_FLD_IP_ROUTER,   0, 0}, translate_ip},
    {{0, 148}, {LNF_FLD_CONN_ID,     0, 0}, translate_uint},
    {{0, 150}, {LNF_FLD_FIRST,       0, 0}, translate_time},
    {{0, 151}, {LNF_FLD_LAST,        0, 0}, translate_time},
    {{0, 152}, {LNF_FLD_FIRST,       0, 0}, translate_time},
    {{0, 153}, {LNF_FLD_LAST,        0, 0}, translate_time},
    {{0, 154}, {LNF_FLD_FIRST,       0, 0}, translate_time},
    {{0, 155}, {LNF_FLD_LAST,        0, 0}, translate_time},
    {{0, 156}, {LNF_FLD_FIRST,       0, 0}, translate_time},
    {{0, 157}, {LNF_FLD_LAST,        0, 0}, translate_time},
    {{0, 176}, {LNF_FLD_ICMP_TYPE,   0, 0}, translate_uint},
    {{0, 177}, {LNF_FLD_ICMP_CODE,   0, 0}, translate_uint},
    {{0, 178}, {LNF_FLD_ICMP_TYPE,   0, 0}, translate_uint},
    {{0, 179}, {LNF_FLD_ICMP_CODE,   0, 0}, translate_uint},
    {{0, 225}, {LNF_FLD_XLATE_SRC_IP,   0, 0}, translate_ip},
    {{0, 226}, {LNF_FLD_XLATE_DST_IP,   0, 0}, translate_ip},
    {{0, 227}, {LNF_FLD_XLATE_SRC_PORT, 0, 0}, translate_uint},
    {{0, 228}, {LNF_FLD_XLATE_DST_PORT, 0, 0}, translate_uint},
    {{0, 230}, {LNF_FLD_EVENT_FLAG,     0, 0}, translate_uint}, //not sure
    {{0, 233}, {LNF_FLD_FW_XEVENT,      0, 0}, translate_uint},
    {{0, 234}, {LNF_FLD_INGRESS_VRFID,  0, 0}, translate_uint},
    {{0, 235}, {LNF_FLD_EGRESS_VRFID,   0, 0}, translate_uint},
    {{0, 258}, {LNF_FLD_RECEIVED,       0, 0}, translate_time},
    {{0, 281}, {LNF_FLD_XLATE_SRC_IP,   0, 0}, translate_ip},
    {{0, 282}, {LNF_FLD_XLATE_DST_IP,   0, 0}, translate_ip}
};

/**
 * \brief Set a value of an unsigned integer
 *
 * \param[out] field    Pointer to the data field
 * \param[in]  lnf_type Type of a LNF field
 * \param[in]  value    New value
 * \return On success returns #IPX_OK.
 * \return If the \p value cannot fit in the \p field of the defined \p size, stores a saturated
 *   value and returns the value #IPX_ERR_TRUNC.
 * \return If the \p lnf_type is not supported, a value of the \p field is unchanged and returns
 *   #IPX_ERR_ARG.
 */
static inline int
translate_set_uint_lnf(void *field, int lnf_type, uint64_t value)
{
    switch (lnf_type) {
    case LNF_UINT64:
        *((uint64_t *) field) = value;
        return IPX_OK;

    case LNF_UINT32:
        if (value > UINT32_MAX) {
            *((uint32_t *) field) = UINT32_MAX; // byte conversion not required
            return IPX_ERR_TRUNC;
        }

        *((uint32_t *) field) = (uint32_t) value;
        return IPX_OK;

    case LNF_UINT16:
        if (value > UINT16_MAX) {
            *((uint16_t *) field) = UINT16_MAX; // byte conversion not required
            return IPX_ERR_TRUNC;
        }

        *((uint16_t *) field) = (uint16_t) value;
        return IPX_OK;

    case LNF_UINT8:
        if (value > UINT8_MAX) {
            *((uint8_t *) field) = UINT8_MAX;
            return IPX_ERR_TRUNC;
        }

        *((uint8_t *) field) = (uint8_t) value;
        return IPX_OK;

    default:
        return IPX_ERR_ARG;
    }
}

/**
 * \brief Convert an unsigned integer
 * \details \copydetails ::translator_func
 */
static int
translate_uint(const struct fds_drec_field *field,
    const struct translator_table_rec *def, uint8_t *buffer_ptr)
{
    // Get a value of IPFIX field
    uint64_t value;
    if (fds_get_uint_be(field->data, field->size, &value) != FDS_OK) {
        // Failed
        return 1;
    }

    // Store the value to the buffer
    if (translate_set_uint_lnf(buffer_ptr, def->lnf.type, value) == IPX_ERR_ARG) {
        // Failed
        return 1;
    }

    return 0;
}

/**
 * \brief Convert an IP address
 * \details \copydetails ::translator_func
 */
static int
translate_ip(const struct fds_drec_field *field,
    const struct translator_table_rec *def, uint8_t *buffer_ptr)
{
    (void) def;

    switch (field->size) {
    case 4: // IPv4
        memset(buffer_ptr, 0x0, sizeof(lnf_ip_t));
        ((lnf_ip_t *) buffer_ptr)->data[3] = *(uint32_t *) field->data;
        break;
    case 16: // IPv6
        memcpy(buffer_ptr, field->data, field->size);
        break;
    default:
        // Invalid size of the field
        return 1;
    }

    return 0;
}

/**
 * \brief Convert TCP flags
 * \note TCP flags can be also stored in 16bit in IPFIX, but LNF file supports
 *   only 8 bits flags. Therefore, we need to truncate the field if necessary.
 * \details \copydetails ::translator_func
 */
static int
translate_tcpflags(const struct fds_drec_field *field,
    const struct translator_table_rec *def, uint8_t *buffer_ptr)
{
    (void) def;

    switch (field->size) {
    case 1:
        *buffer_ptr = *field->data;
        break;
    case 2: {
        uint16_t new_value = ntohs(*(uint16_t *) field->data);
        *buffer_ptr = (uint8_t) new_value; // Preserve only bottom 8 bites
        }
        break;
    default:
        // Invalid size of the field
        return 1;
    }

    return 0;
}

/**
 * \brief Convert a MAC address
 * \note We have to keep the address in network byte order. Therefore, we
 *   cannot use a converter for unsigned int
 * \details \copydetails ::translator_func
 */
static int
translate_mac(const struct fds_drec_field *field,
    const struct translator_table_rec *def, uint8_t *buffer_ptr)
{
    if (field->size != 6 || def->lnf.size != 6) {
        return 1;
    }

    memcpy(buffer_ptr, field->data, 6U);
    return 0;
}

/**
 * \brief Convert a timestamp
 * \details \copydetails ::translator_func
 */
static int
translate_time(const struct fds_drec_field *field,
    const struct translator_table_rec *def, uint8_t *buffer_ptr)
{
    if (field->info->en != 0) {
        // Non-standard field are not supported right now
        return 1;
    }

    // Determine data type of a timestamp
    enum fds_iemgr_element_type type;
    switch (field->info->id) {
    case 150: // flowStartSeconds
    case 151: // flowEndSeconds
        type = FDS_ET_DATE_TIME_SECONDS;
        break;
    case 152: // flowStartMilliseconds
    case 153: // flowEndMilliseconds
        type = FDS_ET_DATE_TIME_MILLISECONDS;
        break;
    case 154: // flowStartMicroseconds
    case 155: // flowEndMicroseconds
        type = FDS_ET_DATE_TIME_MICROSECONDS;
        break;
    case 156: // flowStartNanoseconds
    case 157: // flowEndNanoseconds
        type = FDS_ET_DATE_TIME_NANOSECONDS;
        break;
    case 258: // collectionTimeMilliseconds
        type = FDS_ET_DATE_TIME_MILLISECONDS;
        break;
    default:
        // Other fields are not supported
        return 1;
    }

    // Get the timestamp in milliseconds
    uint64_t value;
    if (fds_get_datetime_lp_be(field->data, field->size, type, &value) != FDS_OK) {
        // Failed
        return 1;
    }

    // Store the value
    if (translate_set_uint_lnf(buffer_ptr, def->lnf.type, value) != IPX_OK) {
        // Failed (note: truncation doesn't make sense)
        return 1;
    }

    return 0;
}

// Size of conversion buffer
#define REC_BUFF_SIZE (65535)
// Size of translator table
#define TRANSLATOR_TABLE_SIZE \
    (sizeof(translator_table_global) / sizeof(translator_table_global[0]))

struct translator_s {
    /** Instance context (only for log!) */
    ipx_ctx_t *ctx;
    /** Private conversion table         */
    struct translator_table_rec table[TRANSLATOR_TABLE_SIZE];
    /** Record conversion buffer         */
    uint8_t rec_buffer[REC_BUFF_SIZE];
};

/**
 * \brief Compare conversion definitions
 * \note Only definitions of IPFIX Information Elements are used for comparison
 *   i.e. other values can be undefined.
 * \param[in] p1 Left definition
 * \param[in] p2 Right definition
 * \return The same as memcmp i.e. < 0, == 0, > 0
 */
static int
transtator_cmp(const void *p1, const void *p2)
{
    const struct translator_table_rec *elem1, *elem2;
    elem1 = (const struct translator_table_rec *) p1;
    elem2 = (const struct translator_table_rec *) p2;

    uint64_t elem1_val = ((uint64_t) elem1->ipfix.pen) << 16 | elem1->ipfix.ie;
    uint64_t elem2_val = ((uint64_t) elem2->ipfix.pen) << 16 | elem2->ipfix.ie;

    if (elem1_val == elem2_val) {
        return 0;
    } else {
        return (elem1_val < elem2_val) ? (-1) : 1;
    }
}

translator_t *
translator_init(ipx_ctx_t *ctx)
{
    translator_t *instance = calloc(1, sizeof(*instance));
    if (!instance) {
        IPX_CTX_ERROR(ctx, "Unable to allocate memory (%s:%d)", __FILE__, __LINE__);
        return NULL;
    }

    // Copy conversion table and sort it (just for sure)
    const size_t table_size = sizeof(translator_table_global);
    memcpy(instance->table, translator_table_global, table_size);

    const size_t table_elem_size = sizeof(instance->table[0]);
    qsort(instance->table, TRANSLATOR_TABLE_SIZE, table_elem_size, transtator_cmp);

    // Update information about LNF fields
    for (size_t i = 0; i < TRANSLATOR_TABLE_SIZE; ++i) {
        struct translator_table_rec *rec = &instance->table[i];

        int size = 0;
        int type = LNF_NONE;

        // Get the size and type of the LNF field
        int size_ret;
        int type_ret;

        size_ret = lnf_fld_info(rec->lnf.id, LNF_FLD_INFO_SIZE, &size, sizeof(size));
        type_ret = lnf_fld_info(rec->lnf.id, LNF_FLD_INFO_TYPE, &type, sizeof(type));
        if (size_ret == LNF_OK && type_ret == LNF_OK) {
            rec->lnf.size = size;
            rec->lnf.type = type;
            continue;
        }

        // Failed
        IPX_CTX_ERROR(ctx, "lnf_fld_info(): Failed to get a size/type of a LNF element (id: %d)",
            rec->lnf.id);
        free(instance);
        return NULL;
    }

    instance->ctx = ctx;
    return instance;
}

void
translator_destroy(translator_t *trans)
{
    free(trans);
}

int
translator_translate(translator_t *trans, struct fds_drec *ipfix_rec, lnf_rec_t *lnf_rec,
    uint16_t flags)
{
    lnf_rec_clear(lnf_rec);

    // Initialize a record iterator
    struct fds_drec_iter it;
    fds_drec_iter_init(&it, ipfix_rec, flags);

    // Try to convert all IPFIX fields
    struct translator_table_rec key;
    struct translator_table_rec *def;
    int converted_fields = 0;

    const size_t table_rec_cnt = TRANSLATOR_TABLE_SIZE;
    const size_t table_rec_size = sizeof(trans->table[0]);
    uint8_t * const buffer_ptr = trans->rec_buffer;

    while (fds_drec_iter_next(&it) != FDS_EOC) {
        // Find a conversion function
        const struct fds_tfield *info = it.field.info;

        key.ipfix.ie = info->id;
        key.ipfix.pen = info->en;

        def = bsearch(&key, trans->table, table_rec_cnt, table_rec_size, transtator_cmp);
        if (!def) {
            // Conversion definition not found
            continue;
        }

        if (def->func(&it.field, def, buffer_ptr) != 0) {
            // Conversion function failed
            IPX_CTX_WARNING(trans->ctx, "Failed to converter a IPFIX IE field  (ID: %" PRIu16 ", "
                "PEN: %" PRIu32 ") to LNF field.", info->id, info->en);
            continue;
        }

        if (lnf_rec_fset(lnf_rec, def->lnf.id, buffer_ptr) != LNF_OK) {
            // Setter failed
            IPX_CTX_WARNING(trans->ctx, "Failed to store a IPFIX IE field (ID: %" PRIu16 ", "
                "PEN: %" PRIu32 ") to a LNF record.", info->id, info->en);
            continue;
        }
        converted_fields++;
    }

    // Cleanup
    return converted_fields;
}

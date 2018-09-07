/**
 * \file translator.c
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \author Tomas Cejka <cejkat@cesnet.cz>
 * \brief Conversion of IPFIX to UniRec format (source file)
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

#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "translator.h"
#include "fields.h"
#include "map.h"
#include <unirec/unirec.h>

// Forward declaration
struct translator_rec;

/**
 * \typedef translator_func
 * \brief Conversion function prototype
 * \param[in,out] trans Translator instance (UniRec record will be modified)
 * \param[in]     rec   Translator record (description of source IPFIX and destination UniRec fields)
 * \param[in]     field IPFIX field data
 * \return On success return 0. Otherwise returns a non-zero value.
 */
typedef int (*translator_func)(translator_t *trans, const struct translator_rec *rec,
    const struct fds_drec_field *field);

/** Size of variable length field                                       */
#define UNIREC_VAR_SIZE (-1)

/** Translator record                                                   */
struct translator_rec {
    struct {
        /** Private Enterprise number                                   */
        uint32_t pen;
        /** Information Element ID                                      */
        uint16_t id;
        /** Field type                                                  */
        enum fds_iemgr_element_type type;
        /** Field semantic                                              */
        enum fds_iemgr_element_semantic sem;
    } ipfix; /** IPFIX field identification                             */

    struct {
        /** Field ID                                                    */
        ur_field_id_t id;
        /** Field type                                                  */
        ur_field_type_t type;
        /** Size of the field (#UNIREC_VAR_SIZE if variable length)     */
        int size;
        /** Index of the field in "required fields" array               */
        int req_idx;
    } unirec; /** UniRec field identification                           */

    /** Translator function                                             */
    translator_func func;
};

/** Internal structure of IPFIX to Unirec translator                    */
struct translator_s {
    /** Instance context (only for log!)                                */
    ipx_ctx_t *ctx;

    struct {
        /** Conversion records                                          */
        struct translator_rec *recs;
        /** Number of records                                           */
        size_t size;
    } table; /** Conversion table                                       */

    struct {
        /** Record structure                                            */
        void *data;
        /** Reference to a UniRec template                              */
        const ur_template_t *ur_tmplt;
    } record;

    struct {
        /**
         * Required fields to be filled
         * \note 1 = the field MUST be filled but it hasn't been yet
         * \note 0 = the field has been filled or it is optional
         */
        uint8_t *req_fields;
        /**
         * Template of required fields
         * \note: 1 = the field is required, 0 = the field is optional
         */
        uint8_t *req_tmplt;
        /** UniRec identification name of the corresponding fields
         * \note For logging purpose
         * */
        char **req_names;
        /** Number of fields (of above arrays)                          */
        size_t size;
    } progress; /**< Auxiliary conversion variables                     */
};

/**
 * \def UINT_CONV
 * \brief Convert IPFIX unsigned integer to UniRec (un)signed integer (aux. macro)
 *
 * Based on a data semantic of source integer, if the value cannot fit into the destination
 * integer, extra bits are discarded (in case of flags and identifiers) or saturated value is used.
 * \param[in] dst_type Destination type
 * \param[in] dst_ptr  Pointer to the destination memory
 * \param[in] src_val  Source value
 * \param[in] max_val  Maximum value to be stored in the destination type
 * \param[in] sem      Data semantic of the source integer
 */
#define UINT_CONV(dst_type, dst_ptr, src_val, max_val, sem)                                \
    if ((src_val) > (max_val) && ((sem) != FDS_ES_FLAGS && (sem) != FDS_ES_IDENTIFIER)) {  \
        *((dst_type *) (dst_ptr)) = (max_val);                                             \
    } else {                                                                               \
        *((dst_type *) (dst_ptr)) = (dst_type) (src_val);                                  \
    }

/**
 * \brief Convert IPFIX unsigned integer to UniRec (un)signed integer
 * \param[in,out] trans Translator instance (UniRec record will be modified)
 * \param[in]     rec   Translator record (description of source IPFIX and destination UniRec fields)
 * \param[in]     field IPFIX field data
 * \return On success returns 0. Otherwise returns a non-zero value.
 */
static int
translate_uint(translator_t *trans, const struct translator_rec *rec,
    const struct fds_drec_field *field)
{
    uint64_t value;
    if (fds_get_uint_be(field->data, field->size, &value) != FDS_OK) {
        return 1; // Conversion failed
    }

    ur_field_id_t ur_id = rec->unirec.id;
    void *field_ptr = ur_get_ptr_by_id(trans->record.ur_tmplt, trans->record.data, ur_id);
    const enum fds_iemgr_element_semantic ipx_sem = rec->ipfix.sem;

    switch (rec->unirec.type) {
    case UR_TYPE_UINT64:
        *((uint64_t *) field_ptr) = value;
        break;
    case UR_TYPE_UINT32:
        UINT_CONV(uint32_t, field_ptr, value, UINT32_MAX, ipx_sem);
        break;
    case UR_TYPE_UINT16:
        UINT_CONV(uint16_t, field_ptr, value, UINT16_MAX, ipx_sem);
        break;
    case UR_TYPE_UINT8:
    case UR_TYPE_CHAR:
        UINT_CONV(uint8_t, field_ptr, value, UINT8_MAX, ipx_sem);
        break;
    case UR_TYPE_INT64:
        UINT_CONV(int64_t, field_ptr, value, INT64_MAX, ipx_sem);
        break;
    case UR_TYPE_INT32:
        UINT_CONV(int32_t, field_ptr, value, INT32_MAX, ipx_sem);
        break;
    case UR_TYPE_INT16:
        UINT_CONV(int16_t, field_ptr, value, INT16_MAX, ipx_sem);
        break;
    case UR_TYPE_INT8:
        UINT_CONV(int8_t, field_ptr, value, INT8_MAX, ipx_sem);
        break;
    default:
        return 1; // Unsupported data type
    }

    return 0;
}

/**
 * \def INT_CONV
 * \brief Convert IPFIX signed integer to UniRec (un)signed integer (aux. macro)
 *
 * Based on a data semantic of source integer, if the value cannot fit into the destination
 * integer, extra bits are discarded (in case of flags and identifiers) or saturated value is used.
 * \param[in] dst_type Destination type
 * \param[in] dst_ptr  Pointer to the destination memory
 * \param[in] src_val  Source value
 * \param[in] max_val  Minimum value to be stored in the destination type
 * \param[in] max_val  Maximum value to be stored in the destination type
 * \param[in] sem      Data semantic of the source integer
 */
#define INT_CONV(dst_type, dst_ptr, src_val, min_val, max_val, sem) \
    if ((src_val) > ((int64_t)(max_val))) {                         \
        if ((sem) == FDS_ES_FLAGS || (sem) == FDS_ES_IDENTIFIER) {  \
            *((dst_type *) (dst_ptr)) = (dst_type) (src_val);       \
        } else {                                                    \
            *((dst_type *) (dst_ptr)) = (max_val);                  \
        }                                                           \
    } else if ((src_val) < (min_val)) {                             \
        if ((sem) == FDS_ES_FLAGS || (sem) == FDS_ES_IDENTIFIER) {  \
            *((dst_type *) (dst_ptr)) = (dst_type) (src_val);       \
        } else {                                                    \
            *((dst_type *) (dst_ptr)) = (min_val);                  \
        }                                                           \
    } else {                                                        \
        *((dst_type *) (dst_ptr)) = (dst_type) (src_val);           \
    }

/**
 * \brief Convert IPFIX signed integer to UniRec (un)signed integer
 * \copydetails translate_uint()
 */
static int
translate_int(translator_t *trans, const struct translator_rec *rec,
    const struct fds_drec_field *field)
{
    int64_t value;
    if (fds_get_int_be(field->data, field->size, &value) != FDS_OK) {
        return 1; // Conversion failed
    }

    ur_field_id_t ur_id = rec->unirec.id;
    void *field_ptr = ur_get_ptr_by_id(trans->record.ur_tmplt, trans->record.data, ur_id);
    const enum fds_iemgr_element_semantic ipx_sem = rec->ipfix.sem;

    switch (rec->unirec.type) {
    case UR_TYPE_INT64:
        *((int64_t *) field_ptr) = value;
        break;
    case UR_TYPE_INT32:
        INT_CONV(int32_t, field_ptr, value, INT32_MIN, INT32_MAX, ipx_sem);
        break;
    case UR_TYPE_INT16:
        INT_CONV(int16_t, field_ptr, value, INT16_MIN, INT16_MAX, ipx_sem);
        break;
    case UR_TYPE_INT8:
    case UR_TYPE_CHAR:
        INT_CONV(int8_t, field_ptr, value, INT8_MIN, INT8_MAX, ipx_sem);
        break;
    case UR_TYPE_UINT64:
        INT_CONV(uint64_t, field_ptr, value, 0, INT64_MAX, ipx_sem); // Must be int64!
        break;
    case UR_TYPE_UINT32:
        INT_CONV(uint32_t, field_ptr, value, 0, UINT32_MAX, ipx_sem);
        break;
    case UR_TYPE_UINT16:
        INT_CONV(uint16_t, field_ptr, value, 0, UINT16_MAX, ipx_sem);
        break;
    case UR_TYPE_UINT8:
        INT_CONV(uint8_t, field_ptr, value, 0, UINT8_MAX, ipx_sem);
        break;
    default:
        return 1; // Unsupported data type
    }

    return 0;
}

/**
 * \brief Convert IPFIX bytes/string to UniRec bytes/string
 * \copydetails translate_uint()
 */
static int
translate_bytes(translator_t *trans, const struct translator_rec *rec,
    const struct fds_drec_field *field)
{
    ur_field_id_t ur_id = rec->unirec.id;
    ur_set_var(trans->record.ur_tmplt, trans->record.data, ur_id, field->data, field->size);
    return 0;
}

/**
 * \brief Convert IPFIX boolean to UniRec char/(un)signed integer
 * \copydetails translate_uint()
 */
static int
translate_bool(translator_t *trans, const struct translator_rec *rec,
    const struct fds_drec_field *field)
{
    bool value;
    if (fds_get_bool(field->data, field->size, &value) != FDS_OK) {
        return 1;
    }

    const ur_field_id_t ur_id = rec->unirec.id;
    void *field_ptr = ur_get_ptr_by_id(trans->record.ur_tmplt, trans->record.data, ur_id);

    const uint8_t res = value ? 1U : 0U;
    switch (rec->unirec.size) {
    case 1:
        *((uint8_t  *) field_ptr) = res;
        break;
    case 2:
        *((uint16_t *) field_ptr) = res;
        break;
    case 4:
        *((uint32_t *) field_ptr) = res;
        break;
    case 8:
        *((uint64_t *) field_ptr) = res;
        break;
    default:
        // Invalid size of the field
        return 1;
    }

    return 0;
}

/**
 * \brief Convert IPFIX float to UniRec float
 * \copydetails translate_uint()
 */
static int
translate_float(translator_t *trans, const struct translator_rec *rec,
    const struct fds_drec_field *field)
{
    double value;
    if (fds_get_float_be(field->data, field->size, &value) != FDS_OK) {
        return 1; // Conversion failed
    }

    const ur_field_id_t ur_id = rec->unirec.id;
    void *field_ptr = ur_get_ptr_by_id(trans->record.ur_tmplt, trans->record.data, ur_id);

    switch (rec->unirec.type) {
    case UR_TYPE_FLOAT:
        if (value < -FLT_MAX && isnormal(value)) {
            *((float *) field_ptr) = -FLT_MAX;
        } else if (value > FLT_MAX && isnormal(value)) {
            *((float *) field_ptr) = FLT_MAX;
        } else {
            *((float *) field_ptr) = (float) value;
        }
        break;
    case UR_TYPE_DOUBLE:
        *((double *) field_ptr) = value;
        break;
    default:
        // Invalid type of the field
        return 1;
    }

    return 0;
}

/**
 * \brief Convert IPFIX IPv4/IPv6 address to UniRec IPv4/IPv6 address
 * \copydetails translate_uint()
 */
static int
translate_ip(translator_t *trans, const struct translator_rec *rec,
    const struct fds_drec_field *field)
{
    const ur_field_id_t ur_id = rec->unirec.id;
    void *field_ptr = ur_get_ptr_by_id(trans->record.ur_tmplt, trans->record.data, ur_id);

    switch (field->size) {
    case 4: // IPv4
        *((ip_addr_t *) field_ptr) = ip_from_4_bytes_be((char *) field->data);
        break;
    case 16: // IPv6
        *((ip_addr_t *) field_ptr) = ip_from_16_bytes_be((char *) field->data);
        break;
    default:
        // Invalid size of the field
        return 1;
    }

    return 0;
}

/**
 * \brief Convert IPFIX MAC address to UniRec MAC address
 * \copydetails translate_uint()
 */
static int
translate_mac(translator_t *trans, const struct translator_rec *rec,
    const struct fds_drec_field *field)
{
    if (field->size != 6U) {
        return 1;
    }

    const ur_field_id_t ur_id = rec->unirec.id;
    ur_time_t *field_ptr = ur_get_ptr_by_id(trans->record.ur_tmplt, trans->record.data, ur_id);
    memcpy(field_ptr, field->data, 6U);
    return 0;
}

/**
 * \brief Convert IPFIX timestamp to UniRec timestamp
 * \copydetails translate_uint()
 */
static int
translate_time(translator_t *trans, const struct translator_rec *rec,
    const struct fds_drec_field *field)
{
    // Get the value
    const enum fds_iemgr_element_type type_ipx = rec->ipfix.type;
    const ur_field_id_t ur_id = rec->unirec.id;
    ur_time_t *field_ptr = ur_get_ptr_by_id(trans->record.ur_tmplt, trans->record.data, ur_id);

    if (type_ipx == FDS_ET_DATE_TIME_MILLISECONDS || type_ipx == FDS_ET_DATE_TIME_SECONDS) {
        // Low precision timestamp
        uint64_t ts;
        if (fds_get_datetime_lp_be(field->data, field->size, type_ipx, &ts) != FDS_OK) {
            return 1;
        }

        *field_ptr = ur_time_from_sec_msec(ts / 1000, ts % 1000);
        return 0;
    } else if (type_ipx == FDS_ET_DATE_TIME_MICROSECONDS || type_ipx == FDS_ET_DATE_TIME_NANOSECONDS) {
        // High precision timestamp
        struct timespec ts;
        if (fds_get_datetime_hp_be(field->data, field->size, type_ipx, &ts) != FDS_OK) {
            return 1;
        }

        *field_ptr = ur_time_from_sec_msec(ts.tv_sec, ts.tv_nsec / 1000000);
        return 0;
    }

    return 1;
}

/**
 * \brief Get size of UniRec numeric data types
 * \param type UniRec data types
 * \return If the type is not numeric, returns 0. Otherwise returns the size.
 */
static uint16_t
translator_size_ur_int(ur_field_type_t type)
{
    switch (type) {
    case UR_TYPE_CHAR:
    case UR_TYPE_UINT8:
    case UR_TYPE_INT8:
        return 1U;
    case UR_TYPE_UINT16:
    case UR_TYPE_INT16:
        return 2U;
    case UR_TYPE_UINT32:
    case UR_TYPE_INT32:
    case UR_TYPE_FLOAT:
        return 4U;
    case UR_TYPE_UINT64:
    case UR_TYPE_INT64:
    case UR_TYPE_DOUBLE:
        return 8U;
    default:
        return 0U;
    }
}

/**
 * \brief Get size of IPFIX numeric data types
 * \param type IPFIX data types
 * \return If the type is not numeric, returns 0. Otherwise returns the size.
 */
static uint16_t
translator_size_ipx_int(enum fds_iemgr_element_type type)
{
    switch (type) {
    case FDS_ET_BOOLEAN:
    case FDS_ET_SIGNED_8:
    case FDS_ET_UNSIGNED_8:
        return 1U;
    case FDS_ET_SIGNED_16:
    case FDS_ET_UNSIGNED_16:
        return 2U;
    case FDS_ET_SIGNED_32:
    case FDS_ET_UNSIGNED_32:
    case FDS_ET_FLOAT_32:
        return 4U;
    case FDS_ET_SIGNED_64:
    case FDS_ET_UNSIGNED_64:
    case FDS_ET_FLOAT_64:
        return 8U;
    default:
        return 0U;
    }
}

/**
 * \brief Get a conversion function from (un)signed IPFIX type to (un)signed UniRec type
 * \param[in] ctx Instance context (only for log!)
 * \param[in] rec Mapping record
 * \return Pointer to the function or NULL (i.e. conversion not supported)
 */
static translator_func
translator_get_numeric_func(ipx_ctx_t *ctx, const struct map_rec *rec)
{
    ur_field_type_t type_ur = rec->unirec.type;
    enum fds_iemgr_element_type type_ipx = rec->ipfix.def->data_type;

    uint16_t size_ur = translator_size_ur_int(type_ur);
    uint16_t size_ipx = translator_size_ipx_int(type_ipx);
    if (size_ur == 0 || size_ipx == 0) {
        return NULL;
    }

    translator_func fn;
    bool warn_msg;

    if (fds_iemgr_is_type_unsigned(type_ipx)) {
        fn = translate_uint;
        // Check possible conversion errors
        bool is_signed = (type_ur == UR_TYPE_INT8 || type_ur == UR_TYPE_INT16
            || type_ur == UR_TYPE_INT32  || type_ur == UR_TYPE_INT64);
        warn_msg = ((is_signed && size_ur <= size_ipx) || (!is_signed && size_ur < size_ipx));
    } else if (fds_iemgr_is_type_signed(type_ipx)) {
        fn = translate_int;
        // Check possible conversion errors
        bool is_unsigned = (type_ur == UR_TYPE_UINT8 || type_ur == UR_TYPE_UINT16
            || type_ur == UR_TYPE_UINT32 || type_ur == UR_TYPE_UINT64);
        warn_msg = (is_unsigned || size_ur < size_ipx);
    } else {
        return NULL;
    }

    if (warn_msg) {
        const struct fds_iemgr_elem *el = rec->ipfix.def;
        IPX_CTX_WARNING(ctx, "Conversion from IPFIX IE '%s:%s' (%s) to UniRec '%s' (%s) may alter "
            "its value!", el->scope->name, el->name, fds_iemgr_type2str(type_ipx),
            rec->unirec.name, rec->unirec.type_str);
    }

    return fn;
}

/**
 * \brief Get a conversion function from float IPFIX type to float UniRec type
 * \param[in] ctx Instance context (only for log!)
 * \param[in] rec Mapping record
 * \return Pointer to the function or NULL (i.e. conversion not supported)
 */
static translator_func
translator_get_float_func(ipx_ctx_t *ctx, const struct map_rec *rec)
{
    ur_field_type_t type_ur = rec->unirec.type;
    enum fds_iemgr_element_type type_ipx = rec->ipfix.def->data_type;

    uint16_t size_ur = translator_size_ur_int(type_ur);
    uint16_t size_ipx = translator_size_ipx_int(type_ipx);
    if (size_ur == 0 || size_ipx == 0) {
        return NULL;
    }

    if (size_ur < size_ipx) {
        const struct fds_iemgr_elem *el = rec->ipfix.def;
        IPX_CTX_WARNING(ctx, "Conversion from IPFIX IE '%s:%s' (%s) to UniRec '%s' (%s) may alter "
            "its value!", el->scope->name, el->name, fds_iemgr_type2str(type_ipx),
            rec->unirec.name, rec->unirec.type_str);
    }

    return translate_float;
}

/**
 * \brief Find a conversion function for a mapping record
 * \param[in] ctx Instance context (only for log!)
 * \param[in] rec Mapping record
 * \return Pointer to the function or NULL (conversion is not supported)
 */
static translator_func
translator_get_func(ipx_ctx_t *ctx, const struct map_rec *rec)
{
    // Types
    ur_field_type_t type_ur = rec->unirec.type;
    enum fds_iemgr_element_type type_ipx = rec->ipfix.def->data_type;

    switch (type_ur) {
    case UR_TYPE_STRING:
    case UR_TYPE_BYTES:
        // String/byte array
        if (type_ipx == FDS_ET_STRING || type_ipx == FDS_ET_OCTET_ARRAY) {
            return translate_bytes;
        }
        break;
    case UR_TYPE_CHAR:
    case UR_TYPE_INT8:
    case UR_TYPE_INT16:
    case UR_TYPE_INT32:
    case UR_TYPE_INT64:
    case UR_TYPE_UINT8:
    case UR_TYPE_UINT16:
    case UR_TYPE_UINT32:
    case UR_TYPE_UINT64:
        // Char and (un)signed integer
        if (fds_iemgr_is_type_unsigned(type_ipx) || fds_iemgr_is_type_signed(type_ipx)) {
            return translator_get_numeric_func(ctx, rec);
        } else if (type_ipx == FDS_ET_BOOLEAN) {
            return translate_bool;
        }
        break;
    case UR_TYPE_FLOAT:
    case UR_TYPE_DOUBLE:
        // Floating-point number
        if (fds_iemgr_is_type_float(type_ipx)) {
            return translator_get_float_func(ctx, rec);
        }
        break;
    case UR_TYPE_IP:
        // IP addresses
        if (fds_iemgr_is_type_ip(type_ipx)) {
            return translate_ip;
        }
        break;
    case UR_TYPE_MAC:
        // MAC address
        if (type_ipx == FDS_ET_MAC_ADDRESS) {
            return translate_mac;
        }
        break;
    case UR_TYPE_TIME:
        // Timestamp
        if (fds_iemgr_is_type_time(type_ipx)) {
            return translate_time;
        }
        break;
    default:
        break;
    }

    return NULL;
}

/**
 * \brief Get the index of an UniRec field in an UniRec template
 * \param[in] tmplt UniRec template
 * \param[in] name  UniRec field ID
 * \return On success return the index (equal or greater than zero).
 * \return #IPX_ERR_ARG if the ID is not valid
 * \return #IPX_ERR_NOTFOUND if the field is not present in the template
 */
static int
translator_idx_by_id(const ur_template_t *tmplt, int ur_id)
{
    assert(IPX_ERR_ARG < 0 && IPX_ERR_NOTFOUND < 0);
    if (ur_id == UR_E_INVALID_NAME) {
        return IPX_ERR_ARG;
    }

    int idx = 0, field_id;
    while ((field_id = ur_iter_fields_record_order(tmplt, idx)) != UR_ITER_END) {
        if (field_id == ur_id) {
            return idx;
        }
        ++idx;
    }

    return IPX_ERR_NOTFOUND;
}

/**
 * \brief Get the index of an UniRec field in an UniRec template
 * \param[in] tmplt UniRec template
 * \param[in] name  UniRec field name
 * \return On success return the index (equal or greater than zero).
 * \return #IPX_ERR_ARG if the name is not valid
 * \return #IPX_ERR_NOTFOUND if the field is not present in the template
 */
static int
translator_idx_by_name(const ur_template_t *tmplt, const char *name)
{
    int ur_id = ur_get_id_by_name(name);
    return translator_idx_by_id(tmplt, ur_id);
}

/**
 * \brief Initialize components necessary for storing converted fields
 *
 * The function initialize will UniRec records and determine whether fields of the UniRec record
 * are required or optional.
 *
 * \warning
 *   The function expects that the \p tmplt_spec doesn't contain whitespace characters!
 * \param[in] trans      Translator internal structure
 * \param[in] tmplt      Parsed UniRec template
 * \param[in] tmplt_spec Raw UniRec specifier (with "question marks")
 * \return #IPX_OK on success
 * \return #IPX_ERR_DENIED or IPX_ERR_NOMEM on failure
 */
static int
translator_init_record(translator_t *trans, const ur_template_t *tmplt, const char *tmplt_spec)
{
    // Prepare UniRec record
    void *ur_record = ur_create_record(tmplt, UR_MAX_SIZE);
    if (!ur_record) {
        IPX_CTX_ERROR(trans->ctx, "Unable to create a UniRec record (%s:%d)", __FILE__, __LINE__);
        return IPX_ERR_DENIED;
    }

    // Prepare progress fields
    const size_t tmplt_cnt = tmplt->count;
    uint8_t *req_fields = malloc(tmplt_cnt * sizeof(*req_fields));
    uint8_t *req_tmplt = malloc(tmplt_cnt * sizeof(*req_tmplt));
    char **req_names = calloc(tmplt_cnt, sizeof(*req_names));
    char *spec_cpy = strdup(tmplt_spec);

    if (!req_fields || !req_tmplt || !req_names || !spec_cpy) {
        IPX_CTX_ERROR(trans->ctx, "Unable to allocate memory (%s:%d)", __FILE__, __LINE__);
        free(req_fields); free(req_tmplt); free(req_names); free(spec_cpy);
        ur_free_record(ur_record);
        return IPX_ERR_NOMEM;
    }

    // Determine which fields are required and which are optional
    int ret_code = IPX_OK;
    char *save_ptr = NULL;

    for (char *str = spec_cpy; ; str = NULL) {
        char *token = strtok_r(str, ",", &save_ptr);
        if (!token) {
            break; // All tokens have been processed
        }

        bool opt = false; // The UniRec field is optional
        if (token[0] == '?') {
            opt = true;
            token += 1;
        }

        int field_idx = translator_idx_by_name(tmplt, token);
        if (field_idx < 0) {
            IPX_CTX_ERROR(trans->ctx, "Unable to locale UniRec field '%s' in the template "
                "(internal error, %s:%d)", token, __FILE__, __LINE__);
            ret_code = IPX_ERR_DENIED;
            break;
        }

        assert(field_idx >= 0 && ((size_t) field_idx) < tmplt_cnt);
        req_tmplt[field_idx] = opt ? 0U : 1U; // The field is optional/required
        req_names[field_idx] = strdup(token);
        if (!req_names[field_idx]) {
            IPX_CTX_ERROR(trans->ctx, "Unable to allocate memory (%s:%d)", __FILE__, __LINE__);
            ret_code = IPX_ERR_NOMEM;
            break;
        }
    }

    free(spec_cpy);
    if (ret_code != IPX_OK) {
        // Clean up
        for (size_t i = 0; i < tmplt_cnt; ++i) {
            free(req_names[i]);
        }
        free(req_fields); free(req_tmplt); free(req_names);
        ur_free_record(ur_record);
        return ret_code;
    }

    trans->record.data = ur_record;
    trans->record.ur_tmplt = tmplt;
    trans->progress.req_fields = req_fields;
    trans->progress.req_names = req_names;
    trans->progress.req_tmplt = req_tmplt;
    trans->progress.size = tmplt_cnt;
    return IPX_OK;
}

/**
 * \brief Destroy components necessary for storing converted fields
 *
 * The function is counterpart to the initialization function translator_init_record()
 * \param[in] trans Translator internal structure
 */
static void
translator_destroy_record(translator_t *trans)
{
    for (size_t i = 0; i < trans->progress.size; ++i) {
        free(trans->progress.req_names[i]);
    }
    free(trans->progress.req_names);
    free(trans->progress.req_tmplt);
    free(trans->progress.req_fields);
    ur_free_record(trans->record.data);
}

/**
 * \brief Compare conversion definitions
 * \note Only definitions of IPFIX Information Elements are used for comparison
 *   i.e. other values can be undefined.
 * \param[in] p1 Left definition
 * \param[in] p2 Right definition
 * \return The same as memcmp i.e. < 0, == 0, > 0
 */
static int
translator_cmp(const void *p1, const void *p2)
{
    const struct translator_rec *elem1, *elem2;
    elem1 = (const struct translator_rec *) p1;
    elem2 = (const struct translator_rec *) p2;

    uint64_t elem1_val = ((uint64_t) elem1->ipfix.pen) << 16 | elem1->ipfix.id;
    uint64_t elem2_val = ((uint64_t) elem2->ipfix.pen) << 16 | elem2->ipfix.id;

    if (elem1_val == elem2_val) {
        return 0;
    } else {
        return (elem1_val < elem2_val) ? (-1) : 1;
    }
}

/**
 * \brief Initialize a conversion table
 *
 * The function will initialize a conversion table of fields from IPFIX to UniRec format.
 * The table will only consist of records that represent conversion to UniRec fields present in
 * a corresponding UniRec template. Each record includes identification of a source IPFIX IE and
 * destination UniRec ID, type and size.
 * \param[in] trans Translator internal function
 * \param[in] map   Mapping
 * \param[in] tmplt UniRec template
 * \return #IPX_OK on success
 * \return #IPX_ERR_NOMEM or #IPX_ERR_DENIED on failure
 */
static int
translator_init_table(translator_t *trans, const map_t *map, const ur_template_t *tmplt)
{
    // Prepare the table
    const size_t rec_max = map_size(map);
    struct translator_rec *table = malloc(rec_max * sizeof(*table));
    if (!table) {
        IPX_CTX_ERROR(trans->ctx, "Unable to allocate memory (%s:%d)", __FILE__, __LINE__);
        return IPX_ERR_NOMEM;
    }

    // Fill the table
    size_t rec_cnt = 0;
    int ret_val = IPX_OK;

    for (size_t i = 0; i < rec_max; ++i) {
        // Get the mapping record
        const struct map_rec *mapping = map_get(map, i);
        int ur_id = ur_get_id_by_name(mapping->unirec.name);
        int req_idx = translator_idx_by_id(tmplt, ur_id);
        if (req_idx == IPX_ERR_ARG) {
            IPX_CTX_ERROR(trans->ctx, "Unable to get ID of UniRec field '%s' "
                "(internal error, %s:%d)", mapping->unirec.name, __FILE__, __LINE__);
            ret_val = IPX_ERR_DENIED;
            break;
        }

        // Add only mapping of UniRec fields present in the UniRec template
        if (req_idx == IPX_ERR_NOTFOUND) {
            assert(!ur_is_present(tmplt, ur_id));
            continue;
        }

        // Try to find conversion function
        struct translator_rec *rec_ptr = &table[rec_cnt];
        rec_ptr->func = translator_get_func(trans->ctx, mapping);
        if (!rec_ptr->func) {
            const struct fds_iemgr_elem *el = mapping->ipfix.def;
            IPX_CTX_ERROR(trans->ctx, "Conversion from IPFIX IE '%s:%s' (PEN: %" PRIu32", IE: "
                "%" PRIu16 ", type: %s) to UniRec field '%s' (type: %s) is not supported!",
                el->scope->name, el->name, el->scope->pen, el->id, fds_iemgr_type2str(el->data_type),
                mapping->unirec.name, mapping->unirec.type_str)
            ret_val = IPX_ERR_DENIED;
            break;
        }

        // Fill the rest
        rec_ptr->unirec.id = (ur_field_id_t) ur_id;
        rec_ptr->unirec.size = ur_get_size(ur_id);
        rec_ptr->unirec.type = ur_get_type(ur_id);
        rec_ptr->unirec.req_idx = req_idx;
        rec_ptr->ipfix.pen = mapping->ipfix.en;
        rec_ptr->ipfix.id = mapping->ipfix.id;
        rec_ptr->ipfix.type = mapping->ipfix.def->data_type;
        rec_ptr->ipfix.sem = mapping->ipfix.def->data_semantic;

        // Success
        IPX_CTX_DEBUG(trans->ctx, "Added conversion from IPFIX IE '%s:%s' to UniRec '%s'",
            mapping->ipfix.def->scope->name, mapping->ipfix.def->name, mapping->unirec.name);
        rec_cnt++;
    }

    if (ret_val != IPX_OK) { // Failed
        free(table);
        return ret_val;
    }

    // Create the conversion table
    if (rec_cnt == 0) {
        IPX_CTX_WARNING(trans->ctx, "Conversion table is empty!");
    }

    qsort(table, rec_cnt, sizeof(*table), translator_cmp);
    trans->table.recs = table;
    trans->table.size = rec_cnt;
    return IPX_OK;
}

/**
 * \brief Destroy the conversion table
 *
 * The function is counterpart to the initialization function translator_init_table()
 * \param trans Translator internal structure
 */
static void
translator_destroy_table(translator_t *trans)
{
    free(trans->table.recs);
}

translator_t *
translator_init(ipx_ctx_t *ctx, const map_t *map, const ur_template_t *tmplt, const char *tmplt_spec)
{
    struct translator_s *trans = calloc(1, sizeof(*trans));
    if (!trans) {
        return NULL;
    }
    trans->ctx = ctx;

    // Initialize components for storing converted fields
    if (translator_init_record(trans, tmplt, tmplt_spec) != IPX_OK) {
        free(trans);
        return NULL;
    }

    if (translator_init_table(trans, map, tmplt) != IPX_OK) {
        translator_destroy_record(trans);
        free(trans);
        return NULL;
    }

    return trans;
}

void
translator_destroy(translator_t *trans)
{
    translator_destroy_table(trans);
    translator_destroy_record(trans);
    free(trans);
}

const void *
translator_translate(translator_t *trans, struct fds_drec *ipfix_rec, uint16_t flags, uint16_t *size)
{
    // Reset UniRec record and required field "flags"
    void *ur_record = trans->record.data;
    const ur_template_t *ur_tmplt = trans->record.ur_tmplt;
    memset(ur_record, 0, ur_rec_fixlen_size(ur_tmplt));
    ur_clear_varlen(trans->record.ur_tmplt, ur_record);

    const size_t req_fields_size = trans->progress.size * sizeof(*trans->progress.req_fields);
    memcpy(trans->progress.req_fields, trans->progress.req_tmplt, req_fields_size);

    // Initialize a record iterator
    struct fds_drec_iter it;
    fds_drec_iter_init(&it, ipfix_rec, flags);

    struct translator_rec key;
    const struct translator_rec *def;
    const size_t table_rec_cnt = trans->table.size;
    const size_t table_rec_size = sizeof(*trans->table.recs);
    int converted_fields = 0;

    // Try to convert all IPFIX fields
    while (fds_drec_iter_next(&it) != FDS_EOC) {
        // Find the conversion function
        const struct fds_tfield *info = it.field.info;
        key.ipfix.id = info->id;
        key.ipfix.pen = info->en;

        def = bsearch(&key, trans->table.recs, table_rec_cnt, table_rec_size, translator_cmp);
        if (!def) {
            // Conversion definition not found
            continue;
        }

        int field_idx = def->unirec.req_idx;
        if (def->func(trans, def, &it.field) != 0) {
            IPX_CTX_WARNING(trans->ctx, "Failed to convert an IPFIX IE (PEN: %" PRIu32 ", "
                "ID: %" PRIu16 ") to UniRec field '%s'",
                info->en, info->id, trans->progress.req_names[field_idx]);
            continue;
        }

        trans->progress.req_fields[field_idx] = 0; // Clear the "flag"
        converted_fields++;
    }

    if (converted_fields == 0) {
        IPX_CTX_INFO(trans->ctx, "Record conversion failed: no fields have been converted!", '\0');
        return NULL;
    }

    // Check if conversion filled are required fields
    size_t idx;
    for (idx = 0; idx < trans->progress.size && trans->progress.req_fields[idx] == 0; ++idx);
    if (idx < trans->progress.size) {
        assert(trans->progress.req_fields[idx] != 0);
        IPX_CTX_INFO(trans->ctx, "Record conversion failed: required UniRec field '%s' was not "
            "filled!", trans->progress.req_names[idx]);
        return NULL;
    }

    // Success
    IPX_CTX_INFO(trans->ctx, "Record conversion successful: %d fields converted", converted_fields);
    *size = ur_rec_size(ur_tmplt, ur_record);
    return ur_record;
}


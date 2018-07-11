/**
 * \file src/plugins/output/json/Storage.cpp
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief JSON converter and output manager (source file)
 * \date 2018
 */

/* Copyright (C) 2018 CESNET, z.s.p.o.
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

#include <stdexcept>
#include <cstring>
#include <limits>
#include <cmath>
#include <inttypes.h>

using namespace std;
#include "Storage.hpp"
#include "protocols.hpp"

/** Base size of the conversion buffer           */
#define BUFFER_BASE   4096
/** IANA enterprise number (forward fields)      */
#define IANA_EN_FWD   0
/** IANA enterprise number (reverse fields)      */
#define IANA_EN_REV   29305
/** IANA identificator of TCP flags              */
#define IANA_ID_FLAGS 6
/** IANA identificator of protocols              */
#define IANA_ID_PROTO 4

Storage::Storage(const struct cfg_format &fmt) : format(fmt)
{
    raw_name[0] = '\0';

    // Prepare the buffer
    record.buffer_begin = new char[BUFFER_BASE];
    record.buffer_end = record.buffer_begin + BUFFER_BASE;
    record.write_begin = record.buffer_begin;
}

Storage::~Storage()
{
    // Destroy all outputs
    for (Output *output : outputs) {
        delete output;
    }

    delete[](record.buffer_begin);
}

/**
 * \brief Reserve memory of the conversion buffer
 *
 * Requests that the string capacity be adapted to a planned change in size to a length of up
 * to n characters.
 * \param[in] n Minimal size of the buffer
 */
void Storage::buffer_reserve(size_t n)
{
    if (n <= buffer_alloc()) {
        // Nothing to do
        return;
    }

    // Prepare a new buffer and copy the content
    const size_t new_size = ((n / BUFFER_BASE) + 1) * BUFFER_BASE;
    char *new_buffer = new char[new_size];

    size_t used = buffer_used();
    std::memcpy(new_buffer, record.buffer_begin, used);
    delete[](record.buffer_begin);

    record.buffer_begin = new_buffer;
    record.buffer_end   = new_buffer + new_size;
    record.write_begin  = new_buffer + used;
}

/**
 * \brief Append the conversion buffer
 * \note
 *   If the buffer length is not sufficient enough, it is automatically reallocated to fit
 *   the string.
 * \param[in] str String to add
 */
void
Storage::buffer_append(const char *str)
{
    const size_t len = std::strlen(str) + 1; // "\0"
    buffer_reserve(buffer_used() + len);
    memcpy(record.write_begin, str, len);
    record.write_begin += len - 1;
}

void
Storage::output_add(Output *output)
{
    outputs.push_back(output);
}

int
Storage::records_store(ipx_msg_ipfix_t *msg)
{
    // Process all data records
    const uint32_t rec_cnt = ipx_msg_ipfix_get_drec_cnt(msg);
    for (uint32_t i = 0; i < rec_cnt; ++i) {
        ipx_ipfix_record *ipfix_rec = ipx_msg_ipfix_get_drec(msg, i);

        // Convert the record
        if (format.ignore_options && ipfix_rec->rec.tmplt->type == FDS_TYPE_TEMPLATE_OPTS) {
            // Skip records based on Options Template
            continue;
        }

        // Convert the record
        convert(ipfix_rec->rec);

        // Store it
        const size_t rec_size = buffer_used();
        for (Output *output : outputs) {
            if (output->process(record.buffer_begin, rec_size) != IPX_OK) {
                return IPX_ERR_DENIED;
            }
        }
    }

    return IPX_OK;
}

/**
 * \brief Find a conversion function for an IPFIX field
 * \param[in] field An IPFIX field to convert
 * \return Conversion function
 */
Storage::converter_fn
Storage::get_converter(const struct fds_drec_field &field)
{
    // Conversion table, based on types defined by enum fds_iemgr_element_type
    const static converter_fn table[] = {
        &Storage::to_octet,    // FDS_ET_OCTET_ARRAY
        &Storage::to_uint,     // FDS_ET_UNSIGNED_8
        &Storage::to_uint,     // FDS_ET_UNSIGNED_16
        &Storage::to_uint,     // FDS_ET_UNSIGNED_32
        &Storage::to_uint,     // FDS_ET_UNSIGNED_64
        &Storage::to_int,      // FDS_ET_SIGNED_8
        &Storage::to_int,      // FDS_ET_SIGNED_16
        &Storage::to_int,      // FDS_ET_SIGNED_32
        &Storage::to_int,      // FDS_ET_SIGNED_64
        &Storage::to_float,    // FDS_ET_FLOAT_32
        &Storage::to_float,    // FDS_ET_FLOAT_64
        &Storage::to_bool,     // FDS_ET_BOOLEAN
        &Storage::to_mac,      // FDS_ET_MAC_ADDRESS
        &Storage::to_string,   // FDS_ET_STRING
        &Storage::to_datetime, // FDS_ET_DATE_TIME_SECONDS
        &Storage::to_datetime, // FDS_ET_DATE_TIME_MILLISECONDS
        &Storage::to_datetime, // FDS_ET_DATE_TIME_MICROSECONDS
        &Storage::to_datetime, // FDS_ET_DATE_TIME_NANOSECONDS
        &Storage::to_ip,       // FDS_ET_IPV4_ADDRESS
        &Storage::to_ip        // FDS_ET_IPV6_ADDRESS
        // Other types are not supported yet
    };

    constexpr size_t table_size = sizeof(table) / sizeof(table[0]);
    const enum fds_iemgr_element_type type = (field.info->def != nullptr)
        ? (field.info->def->data_type) : FDS_ET_OCTET_ARRAY;

    if (type >= table_size) {
        return &Storage::to_octet;
    } else {
        return table[type];
    }
}

/**
 * \brief Convert an octet array to JSON string
 *
 * \note
 *   Because the JSON doesn't directly support the octet array, the result string is wrapped in
 *   double quotes.
 * \param[in] field Field to convert
 * \throw invalid_argument if the field is not a valid field of this type
 */
void
Storage::to_octet(const struct fds_drec_field &field)
{
    if (field.size <= 8) {
        // Print as unsigned integer
        to_uint(field);
        return;
    }

    const size_t mem_req = (2 * field.size) + 5U; // "0x" + 2 chars per byte + 2x "\"" + "\0"
    buffer_reserve(buffer_used() + mem_req);

    record.write_begin[0] = '"';
    record.write_begin[1] = '0';
    record.write_begin[2] = 'x';
    record.write_begin += 3;
    int res = fds_octet_array2str(field.data, field.size, record.write_begin, buffer_remain());
    if (res >= 0) {
        record.write_begin += res;
        *(record.write_begin++) = '"';
        return;
    }

    // Restore position and throw an exception
    throw std::invalid_argument("Failed to convert an octet array! (error: "
        + std::to_string(res) + ")");
}

/**
 * \brief Convert an integer to JSON string
 * \param[in] field Field to convert
 * \throw invalid_argument if the field is not a valid field of this type
 */
void
Storage::to_int(const struct fds_drec_field &field)
{
    // Print as signed integer
    int res = fds_int2str_be(field.data, field.size, record.write_begin, buffer_remain());
    if (res > 0) {
        record.write_begin += res;
        return;
    }

    if (res != FDS_ERR_BUFFER) {
        throw std::invalid_argument("Failed to convert a signed integer to string! (error: "
            + std::to_string(res) + ")");
    }

    // Reallocate and try again
    buffer_reserve(buffer_used() + FDS_CONVERT_STRLEN_INT);
    to_int(field);
}

/**
 * \brief Convert an unsigned integer to JSON string
 * \param[in] field Field to convert
 * \throw invalid_argument if the field is not a valid field of this type
 */
void
Storage::to_uint(const struct fds_drec_field &field)
{
    // Print as unsigned integer
    int res = fds_uint2str_be(field.data, field.size, record.write_begin, buffer_remain());
    if (res > 0) {
        record.write_begin += res;
        return;
    }

    if (res != FDS_ERR_BUFFER) {
        throw std::invalid_argument("Failed to convert an unsigned integer to string! (error: "
            + std::to_string(res) + ")");
    }

    // Reallocate and try again
    buffer_reserve(buffer_used() + FDS_CONVERT_STRLEN_INT);
    to_uint(field);
}

/**
 * \brief Convert a float to JSON string
 *
 * \note
 *   If the value represent infinite or NaN, instead of number a corresponding string
 *   is stored.
 * \param[in] field Field to convert
 * \throw invalid_argument if the field is not a valid field of this type
 */
void
Storage::to_float(const struct fds_drec_field &field)
{
    // We cannot use default function because "nan" and "infinity" values
    double value;
    if (fds_get_float_be(field.data, field.size, &value) != FDS_OK) {
        throw std::invalid_argument("Failed to convert a float number!");
    }

    if (std::isfinite(value)) {
        // Normal value
        const char *fmt = (field.size == sizeof(float))
            ? ("%." FDS_CONVERT_STRX(FLT_DIG) "g")  // float precision
            : ("%." FDS_CONVERT_STRX(DBL_DIG) "g"); // double precision
        int str_real_len = snprintf(record.write_begin, buffer_remain(), fmt, value);
        if (str_real_len < 0) {
            throw std::invalid_argument("Failed to convert a float number. snprintf() failed!");
        }

        if (size_t(str_real_len) >= buffer_remain()) {
            // Reallocate the buffer and try again
            buffer_reserve(2 * buffer_alloc()); // Just double it
            to_float(field);
            return;
        }

        record.write_begin += str_real_len;
        return;
    }

    // +/-Nan or +/-infinite
    const char *str;
    // Size 8 (double)
    if (value == std::numeric_limits<double>::infinity()) {
        str = "\"inf\"";
    } else if (value == -std::numeric_limits<double>::infinity()) {
        str = "\"-inf\"";
    } else if (value == std::numeric_limits<double>::quiet_NaN()) {
        str = "\"nan\"";
    } else if (value == -std::numeric_limits<double>::quiet_NaN()) {
        str = "\"-nan\"";
    } else {
        str = "null";
    }

    size_t size = std::strlen(str) + 1; // + '\0'
    buffer_reserve(buffer_used() + size);
    strcpy(record.write_begin, str);
    record.write_begin += size;
}

/**
 * \brief Convert a boolean to JSON string
 *
 * \note If the stored boolean value is invalid, an exception is thrown!
 * \param[in] field Field to convert
 * \throw invalid_argument if the field is not a valid field of this type
 */
void
Storage::to_bool(const struct fds_drec_field &field)
{
    if (field.size != 1) {
        throw std::invalid_argument("Failed to convert a boolean to string!");
    }

    int res = fds_bool2str(field.data, record.write_begin, buffer_remain());
    if (res > 0) {
        record.write_begin += res;
        return;
    }

    if (res != FDS_ERR_BUFFER) {
        throw std::invalid_argument("Failed to convert a boolean to string!");
    }

    // Reallocate and try again
    buffer_reserve(buffer_used() + FDS_CONVERT_STRLEN_FALSE); // false is longer
    to_bool(field);
}

/**
 * \brief Convert a datetime to JSON string
 *
 * Based on the configuration, the output string is formatted or represent UNIX timestamp
 * (in milliseconds). Formatted string is based on ISO 8601 and use only millisecond precision
 * because JSON parsers typically doesn't support anything else.
 * \param[in] field Field to convert
 * \throw invalid_argument if the field is not a valid field of this type
 */
void
Storage::to_datetime(const struct fds_drec_field &field)
{
    const fds_iemgr_element_type type = field.info->def->data_type;

    if (format.timestamp) {
        // Convert to formatted string
        fds_convert_time_fmt fmt = FDS_CONVERT_TF_MSEC_UTC; // Only supported by JSON parser
        buffer_reserve(buffer_used() + FDS_CONVERT_STRLEN_DATE + 2); // 2x '\"'

        *(record.write_begin++) = '"';
        int res = fds_datetime2str_be(field.data, field.size, type, record.write_begin,
            buffer_remain(), fmt);
        if (res > 0) {
            // Success
            record.write_begin += res;
            *(record.write_begin++) = '"';
            return;
        }

        throw std::invalid_argument("Failed to convert a timestamp to string! (error: " +
            std::to_string(res) + ")");
    }

    // Convert to UNIX timestamp (in milliseconds)
    uint64_t time;
    if (fds_get_datetime_lp_be(field.data, field.size, type, &time) != FDS_OK) {
        throw std::invalid_argument("Failed to convert a timestamp to string!");
    }

    time = htobe64(time); // Convert to network byte order and use fast libfds converter
    int res = fds_uint2str_be(&time, sizeof(time), record.write_begin, buffer_remain());
    if (res > 0) {
        record.write_begin += res;
        return;
    }

    if (res != FDS_ERR_BUFFER) {
        throw std::invalid_argument("Failed to convert a timestamp to string!");
    }

    buffer_reserve(buffer_used() + FDS_CONVERT_STRLEN_INT);
    to_datetime(field);
}

/**
 * \brief Convert a MAC address to JSON string
 *
 * \note
 *   Because the JSON doesn't directly support the MAC address, the result string is wrapped in
 *   double quotes.
 * \param[in] field Field to convert
 * \throw invalid_argument if the field is not a valid field of this type
 */
void
Storage::to_mac(const struct fds_drec_field &field)
{
    buffer_reserve(buffer_used() + FDS_CONVERT_STRLEN_MAC + 2); // MAC address + 2x '\"'

    *(record.write_begin++) = '"';
    int res = fds_mac2str(field.data, field.size, record.write_begin, buffer_remain());
    if (res > 0) {
        record.write_begin += res;
        *(record.write_begin++) = '"';
        return;
    }

    throw std::invalid_argument("Failed to convert a MAC address to string! (error: "
        + std::to_string(res) + ")");
}

/**
 * \brief Convert an IPv4/IPv6 address to JSON string
 *
 * \note
 *   Because the JSON doesn't directly support IP addresses, the result string is wrapped in double
 *   quotes.
 * \param[in] field Field to convert
 * \throw invalid_argument if the field is not a valid field of this type
 */
void
Storage::to_ip(const struct fds_drec_field &field)
{
    // Make sure that we have enough memory
    buffer_reserve(buffer_used() + FDS_CONVERT_STRLEN_IP + 2); // IPv4/IPv6 address + 2x '\"'

    *(record.write_begin++) = '"';
    int res = fds_ip2str(field.data, field.size, record.write_begin, buffer_remain());
    if (res > 0) {
        record.write_begin += res;
        *(record.write_begin++) = '"';
        return;
    }

    throw std::invalid_argument("Failed to convert an IP address to string! (error: "
        + std::to_string(res) + ")");
}

/**
 * \def ESCAPE_CHAR
 * \brief Auxiliary function for escaping characters
 * \param[in] ch Character
 */
#define ESCAPE_CHAR(ch) { \
    out_buffer[idx_output++] = '\\'; \
    out_buffer[idx_output++] = (ch); \
}

/**
 * \brief Convert IPFIX string to JSON string
 *
 * Non-ASCII characters are automatically converted to the special escaped sequence i.e. '\uXXXX'.
 * Quote and backslash are always escaped and while space (and control) characters are converted
 * based on active configuration.
 * \param[in] field Field to convert
 */
void
Storage::to_string(const struct fds_drec_field &field)
{
    /* Make sure that we have enough memory for the worst possible case (escaping everything)
     * This case contains only non-printable characters that will be replaced with string
     * "\uXXXX" (6 characters) each.
     */
    const size_t max_size = (6 * field.size) + 3U; // '\uXXXX' + 2x "\"" + 1x '\0'
    buffer_reserve(buffer_used() + max_size);

    const uint8_t *in_buffer = reinterpret_cast<const uint8_t *>(field.data);
    char *out_buffer = record.write_begin;
    uint32_t idx_output = 0;



    // Beginning of the string
    out_buffer[idx_output++] = '"';

    for (uint32_t i = 0; i < field.size; ++i) {
        // All characters from the extended part of ASCII must be escaped
        if (in_buffer[i] > 0x7F) {
            snprintf(&out_buffer[idx_output], 7, "\\u00%02x", in_buffer[i]);
            idx_output += 6;
            continue;
        }

        /*
         * Based on RFC 4627 (Section: 2.5. Strings):
         * Control characters (i.e. 0x00 - 0x1F), '"' and  '\' must be escaped
         * using "\"", "\\" or "\uXXXX" where "XXXX" is a hexa value.
         */
        if (in_buffer[i] > 0x1F && in_buffer[i] != '"' && in_buffer[i] != '\\') {
            // Copy to the output buffer
            out_buffer[idx_output++] = in_buffer[i];
            continue;
        }

        // Copy as escaped character
        switch(in_buffer[i]) {
        case '\\': // Reverse solidus
        ESCAPE_CHAR('\\');
            continue;
        case '\"': // Quotation
        ESCAPE_CHAR('\"');
            continue;
        default:
            break;
        }

        if (!format.white_spaces) {
            // Skip white space characters
            continue;
        }

        switch(in_buffer[i]) {
        case '\t': // Tabulator
        ESCAPE_CHAR('t');
            break;
        case '\n': // New line
        ESCAPE_CHAR('n');
            break;
        case '\b': // Backspace
        ESCAPE_CHAR('b');
            break;
        case '\f': // Form feed
        ESCAPE_CHAR('f');
            break;
        case '\r': // Return
        ESCAPE_CHAR('r');
            break;
        default: // "\uXXXX"
            snprintf(&out_buffer[idx_output], 7, "\\u00%02x", in_buffer[i]);
            idx_output += 6;
            break;
        }
    }

    // End of the string
    out_buffer[idx_output++] = '"';
    record.write_begin += idx_output;
}

#undef ESCAPE_CHAR

/**
 * \brief Convert TCP flags to JSON string
 *
 * \note The result string is wrapped in double quotes.
 * \param[in] field Field to convert
 * \throw invalid_argument if the field is not a valid field of this type
 */
void
Storage::to_flags(const struct fds_drec_field &field)
{
    if (field.size != 1 && field.size != 2) {
        throw std::invalid_argument("Failed to convert TCP flags to string!");
    }

    uint8_t flags;
    if (field.size == 1) {
        flags = *field.data;
    } else {
        flags = uint8_t(ntohs(*reinterpret_cast<uint16_t *>(field.data)));
    }

    const size_t size = 8; // 2x '"' + 6x flags
    buffer_reserve(buffer_used() + size + 1); // '\0'

    char *buffer = record.write_begin;
    buffer[0] = '"';
    buffer[1] = (flags & 0x20) ? 'U' : '.';
    buffer[2] = (flags & 0x10) ? 'A' : '.';
    buffer[3] = (flags & 0x08) ? 'P' : '.';
    buffer[4] = (flags & 0x04) ? 'R' : '.';
    buffer[5] = (flags & 0x02) ? 'S' : '.';
    buffer[6] = (flags & 0x01) ? 'F' : '.';
    buffer[7] = '"';
    buffer[8] = '\0';

    record.write_begin += size;
}

/**
 * \brief Convert a protocol to JSON string
 *
 * \note The result string is wrapped in double quotes.
 * \param[in] field Field to convert
 * \throw invalid_argument if the field is not a valid field of this type
 */
void
Storage::to_proto(const struct fds_drec_field &field)
{
    if (field.size != 1) {
        throw std::invalid_argument("Failed to convert a protocol to string!");
    }

    const char *proto_str = protocols[*field.data];
    const size_t proto_len = strlen(proto_str);
    buffer_reserve(buffer_used() + proto_len + 3); // 2x '"' + '\0'

    *(record.write_begin++) = '"';
    memcpy(record.write_begin, proto_str, proto_len);
    record.write_begin += proto_len;
    *(record.write_begin++) = '"';
}


/**
 * \brief Append the buffer with a name of an Information Element
 *
 * If the definition of a field is unknown, numeric identification is added.
 * \param[in] field Field identification to add
 */
void
Storage::add_field_name(const struct fds_drec_field &field)
{
    const struct fds_iemgr_elem *def = field.info->def;

    if (def == nullptr) {
        // Unknown field - max length is "\"en" + 10x <en> + ":id" + 5x <id> + '\"\0'
        snprintf(raw_name, raw_size, "\"en%" PRIu32 ":id%" PRIu16 "\":", field.info->en,
            field.info->id);
        buffer_append(raw_name);
        return;
    }

    // Output format: "<pen_name>:<element_name>:"
    const size_t scope_size = strlen(def->scope->name);
    const size_t elem_size = strlen(def->name);

    size_t size = scope_size + elem_size + 5; // 2x '"' + 2x ':' + '\0'
    buffer_reserve(buffer_used() + size);

    *(record.write_begin++) = '"';
    memcpy(record.write_begin, def->scope->name, scope_size);
    record.write_begin += scope_size;

    *(record.write_begin++) = ':';
    memcpy(record.write_begin, def->name, elem_size);
    record.write_begin += elem_size;
    *(record.write_begin++) = '"';
    *(record.write_begin++) = ':';
}

/**
 * \brief Convert an IPFIX record to JSON string
 *
 * For each field in the record, try to convert it into JSON format. The record is stored
 * into the local buffer.
 * \param[in] rec IPFIX record to convert
 */
void
Storage::convert(struct fds_drec &rec)
{
    converter_fn fn;
    unsigned int added = 0;
    record.write_begin = record.buffer_begin;
    buffer_append("{\"@type\":\"ipfix.entry\",");

    // Try to convert each field in the record
    uint16_t flags = (format.ignore_unknown) ? FDS_DREC_UNKNOWN_SKIP : 0;
    struct fds_drec_iter iter;
    fds_drec_iter_init(&iter, &rec, flags);

    while (fds_drec_iter_next(&iter) != FDS_ERR_NOTFOUND) {
        // Separate fields
        if (added != 0) {
            // Add comma
            buffer_append(",");
        }

        // Add field name "<pen>:<field_name>"
        add_field_name(iter.field);

        // Find a converter
        const struct fds_tfield *def = iter.field.info;
        if (format.tcp_flags && def->id == IANA_ID_FLAGS
                && (def->en == IANA_EN_FWD || def->en == IANA_EN_REV)) {
            // Convert to formatted flags
            fn = &Storage::to_flags;
        } else if (format.proto && def->id == IANA_ID_PROTO
                && (def->en == IANA_EN_FWD || def->en == IANA_EN_REV)) {
            // Convert to formatted protocol type
            fn = &Storage::to_proto;
        } else {
            // Convert to field based on internal type
            fn = get_converter(iter.field);
        }

        // Convert the field
        char *writer_pos = record.write_begin;

        try {
            (this->*fn)(iter.field);
        } catch (std::invalid_argument &ex) {
            // Recover from a conversion error
            record.write_begin = writer_pos;
            buffer_append("null");
        }

        added++;
    }

    buffer_append("}\n"); // This also adds '\0'
}
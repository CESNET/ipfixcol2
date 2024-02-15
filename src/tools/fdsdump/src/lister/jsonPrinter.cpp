/**
 * @file
 * @author Lukas Hutak <hutak@cesnet.cz>
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Lister JSON printer
 *
 * Copyright: (C) 2024 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <stdexcept>
#include <string>

#include <common/common.hpp>
#include <common/ieMgr.hpp>

#include <lister/jsonPrinter.hpp>

namespace fdsdump {
namespace lister {

JsonPrinter::JsonPrinter(const std::string &args)
{
    std::string args_fields;
    std::string args_opts;
    size_t delim_pos;

    delim_pos = args.find(';');
    args_fields = args.substr(0, delim_pos);
    args_opts = (delim_pos != std::string::npos)
        ? args.substr(delim_pos + 1, std::string::npos)
        : "";

    parse_fields(args_fields);
    parse_opts(args_opts);

    m_buffer.reserve(1024);
}

JsonPrinter::~JsonPrinter()
{
}

void
JsonPrinter::parse_fields(const std::string &str)
{
    std::vector<std::string> field_names;

    if (str.empty()) {
        throw std::invalid_argument("JSON output: no output fields defined");
    }

    field_names = string_split(str, ",");

    for (const std::string &name : field_names) {
        m_fields.emplace_back(name);
    }
}

void
JsonPrinter::parse_opts(const std::string &str)
{
    std::vector<std::string> opts;

    if (str.empty()) {
        return;
    }

    opts = string_split(str, ",");

    for (const std::string &opt_raw : opts) {
        std::string opt = string_trim_copy(opt_raw);
        std::vector<std::string> opt_parts = string_split(opt, "=", 2);
        const std::string &opt_name = (opt_parts.size() >= 1) ? opt_parts[0] : "";
        const std::string &opt_value = (opt_parts.size() >= 2) ? opt_parts[1] : "";

        if (strcasecmp(opt.c_str(), "no-biflow-split") == 0) {
            m_biflow_split = false;

        } else if (strcasecmp(opt_name.c_str(), "timestamp") == 0) {
            if (opt_value.empty()) {
                throw std::invalid_argument(
                    "JSON output: timestamp option is missing value"
                    ", use 'timestamp=unix' or 'timestamp=formatted'");
            }

            if (opt_value == "unix") {
                m_format_timestamp = false;
            } else if (opt_value == "formatted") {
                m_format_timestamp = true;
            } else {
                throw std::invalid_argument(
                    "JSON output: invalid timestamp option '" + opt_value + "'"
                    ", use 'timestamp=unix' or 'timestamp=formatted'");
            }

        } else {
            throw std::invalid_argument("JSON output: unknown option '" + opt + "'");
        }
    }
}

void
JsonPrinter::print_prologue()
{
    std::cout << "[";
}

void
JsonPrinter::print_record(struct fds_drec *rec, bool reverse)
{
    unsigned int field_cnt = 0;

    m_buffer.clear();
    m_buffer.push_back('{');

    for (auto &field : m_fields) {
        if (field_cnt++ > 0) {
            m_buffer.push_back(',');
        }

        m_buffer.push_back('"');
        m_buffer.append(field.get_name());
        m_buffer.push_back('"');

        m_buffer.push_back(':');

        append_value(rec, field, reverse);
    }

    m_buffer.push_back('}');

    std::cout << ((m_rec_printed++ > 0) ? ",\n " : "\n ");
    std::cout << m_buffer;
}

unsigned int
JsonPrinter::print_record(Flow *flow)
{
    switch (flow->dir) {
    case DIRECTION_NONE:
        return 0;
    case DIRECTION_FWD:
        print_record(&flow->rec, false);
        return 1;
    case DIRECTION_REV:
        print_record(&flow->rec, true);
        return 1;
    case DIRECTION_BOTH:
        if (m_biflow_split) {
            print_record(&flow->rec, false);
            print_record(&flow->rec, true);
            return 2;
        } else {
            print_record(&flow->rec, false);
            return 1;
        }
    }

    return 0;
}

void
JsonPrinter::print_epilogue()
{
    std::cout << "\n]\n";
}

void
JsonPrinter::append_value(struct fds_drec *rec, Field &field, bool reverse)
{
    const size_t start_pos = m_buffer.size();
    unsigned int count = 0;

    auto cb = [this, &count](const struct fds_drec_field &field) -> void {
        if (count++) {
            m_buffer.push_back(',');
        }

        append_value(field);
    };

    field.for_each(rec, cb, reverse);

    if (count == 0) {
        // Not found
        append_null();
    } else if (count > 1) {
        // Multiple values
        m_buffer.insert(start_pos, 1, '[');
        m_buffer.push_back(']');
    }
}

void
JsonPrinter::append_value(const struct fds_drec_field &field)
{
    const struct fds_iemgr_elem *elem = field.info->def;
    const enum fds_iemgr_element_type type = (elem != nullptr)
        ? elem->data_type
        : FDS_ET_OCTET_ARRAY;

    switch (type) {
    case FDS_ET_OCTET_ARRAY:
        append_octet_array(field);
        break;
    case FDS_ET_UNSIGNED_8:
    case FDS_ET_UNSIGNED_16:
    case FDS_ET_UNSIGNED_32:
    case FDS_ET_UNSIGNED_64:
        append_uint(field);
        break;
    case FDS_ET_SIGNED_8:
    case FDS_ET_SIGNED_16:
    case FDS_ET_SIGNED_32:
    case FDS_ET_SIGNED_64:
        append_int(field);
        break;
    case FDS_ET_FLOAT_32:
    case FDS_ET_FLOAT_64:
        append_float(field);
        break;
    case FDS_ET_BOOLEAN:
        append_boolean(field);
        break;
    case FDS_ET_MAC_ADDRESS:
        append_mac(field);
        break;
    case FDS_ET_STRING:
        append_string(field);
        break;
    case FDS_ET_DATE_TIME_SECONDS:
    case FDS_ET_DATE_TIME_MILLISECONDS:
    case FDS_ET_DATE_TIME_MICROSECONDS:
    case FDS_ET_DATE_TIME_NANOSECONDS:
        append_timestamp(field);
        break;
    case FDS_ET_IPV4_ADDRESS:
    case FDS_ET_IPV6_ADDRESS:
        append_ip(field);
        break;
    default:
        // Structured data types are currently not supported
        append_unsupported();
        break;
    }
}

void
JsonPrinter::append_octet_array(const fds_drec_field &field)
{
    if (field.size == 0) {
        append_null();
        return;
    }

    m_buffer.append("\"0x");

    for (size_t i = 0; i < field.size; ++i) {
        const uint8_t byte = field.data[i];
        const uint8_t byte_high = (byte & 0xF0) >> 4;
        const uint8_t byte_low = byte & 0x0F;

        m_buffer.push_back(byte_high < 10 ? ('0' + byte_high) : ('A' - 10 + byte_high));
        m_buffer.push_back(byte_low < 10 ? ('0' + byte_low) : ('A' - 10 + byte_low));
    }

    m_buffer.push_back('"');
}

void
JsonPrinter::append_uint(const fds_drec_field &field)
{
    char buffer[32];
    int ret;

    ret = fds_uint2str_be(field.data, field.size, buffer, sizeof(buffer));
    if (ret < 0) {
        append_invalid();
        return;
    }

    m_buffer.append(buffer);
}

void
JsonPrinter::append_int(const fds_drec_field &field)
{
    char buffer[32];
    int ret;

    ret = fds_int2str_be(field.data, field.size, buffer, sizeof(buffer));
    if (ret < 0) {
        append_invalid();
        return;
    }

    m_buffer.append(buffer);
}

void
JsonPrinter::append_float(const fds_drec_field &field)
{
    char buffer[32];
    int ret;

    ret = fds_float2str_be(field.data, field.size, buffer, sizeof(buffer));
    if (ret < 0) {
        append_invalid();
        return;
    }

    m_buffer.append(buffer);
}

void
JsonPrinter::append_boolean(const fds_drec_field &field)
{
    bool value = 0;
    int ret;

    ret = fds_get_bool(field.data, field.size, &value);
    if (ret != FDS_OK) {
        append_invalid();
        return;
    }

    if (value) {
        m_buffer.append("true");
    } else {
        m_buffer.append("false");
    }
}

void
JsonPrinter::append_timestamp(const fds_drec_field &field)
{
    if (m_format_timestamp) {
        // Convert to formatted string
        char buffer[FDS_CONVERT_STRLEN_DATE];
        int ret;

        ret = fds_datetime2str_be(
            field.data,
            field.size,
            field.info->def->data_type,
            buffer,
            sizeof(buffer),
            FDS_CONVERT_TF_MSEC_UTC);
        if (ret < 0) {
            append_invalid();
            return;
        }

        m_buffer.push_back('"');
        m_buffer.append(buffer);
        m_buffer.push_back('"');


    } else {
        // Convert to UNIX timestamp (in milliseconds)
        uint64_t time;
        if (fds_get_datetime_lp_be(field.data, field.size, field.info->def->data_type, &time) != FDS_OK) {
            append_invalid();
            return;
        }

        time = htobe64(time); // Convert to network byte order and use fast libfds converter
        char buffer[32];
        if (fds_uint2str_be(&time, sizeof(time), buffer, sizeof(buffer)) < 0) {
            append_invalid();
            return;
        }

        m_buffer.append(buffer);
    }
}

void
JsonPrinter::append_string(const fds_drec_field &field)
{
    m_buffer.push_back('"');

    for (unsigned int i = 0; i < field.size; ++i) {
        const char c = field.data[i];
        switch (c) {
        case '"':
            m_buffer.append("\\\"");
            break;
        case '\\':
            m_buffer.append("\\\\");
            break;
        case '\b':
            m_buffer.append("\\b");
            break;
        case '\f':
            m_buffer.append("\\f");
            break;
        case '\n':
            m_buffer.append("\\n");
            break;
        case '\r':
            m_buffer.append("\\r");
            break;
        case '\t':
            m_buffer.append("\\t");
            break;
        default:
            if (c >= '\x00' && c <= '\x1f') {
                char buffer[8]; // print control characters as "\uXXXX"
                snprintf(buffer, sizeof(buffer), "\\u%04X", int(c));
                m_buffer.append(buffer);
            } else {
                m_buffer.push_back(c);
            }
            break;
        }
    }

    m_buffer.push_back('"');
}

void
JsonPrinter::append_mac(const fds_drec_field &field)
{
    char buffer[FDS_CONVERT_STRLEN_MAC];
    int ret;

    ret = fds_mac2str(field.data, field.size, buffer, sizeof(buffer));
    if (ret < 0) {
        append_invalid();
        return;
    }

    m_buffer.push_back('"');
    m_buffer.append(buffer);
    m_buffer.push_back('"');
}

void
JsonPrinter::append_ip(const fds_drec_field &field)
{
    char buffer[FDS_CONVERT_STRLEN_IP];
    int ret;

    ret = fds_ip2str(field.data, field.size, buffer, sizeof(buffer));
    if (ret < 0) {
        append_invalid();
        return;
    }

    m_buffer.push_back('"');
    m_buffer.append(buffer);
    m_buffer.push_back('"');
}

void
JsonPrinter::append_null()
{
    m_buffer.append("null");
}

void
JsonPrinter::append_invalid()
{
    m_buffer.append("\"<invalid>\"");
}

void
JsonPrinter::append_unsupported()
{
    m_buffer.append("\"<unsupported>\"");
}

} // lister
} // fdsdump

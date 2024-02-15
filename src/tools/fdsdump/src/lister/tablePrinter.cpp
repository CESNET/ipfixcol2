/**
 * @file
 * @author Lukas Hutak <hutak@cesnet.cz>
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Lister table printer
 *
 * Copyright: (C) 2024 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

#include <common/common.hpp>

#include <lister/tablePrinter.hpp>

namespace fdsdump {
namespace lister {

TablePrinter::TablePrinter(const std::string &args)
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

void
TablePrinter::parse_fields(const std::string &str)
{
    std::vector<std::string> field_names;

    if (str.empty()) {
        throw std::invalid_argument("no output fields defined");
    }

    field_names = string_split(str, ",");

    for (const std::string &name : field_names) {
        Field field {name};

        if (field.is_alias()) {
            const struct fds_iemgr_alias *alias = field.get_alias();
            size_t width = field.get_name().size();

            for (size_t i = 0; i < alias->sources_cnt; i++) {
                const struct fds_iemgr_elem *elem = alias->sources[i];
                width = std::max(width, data_length(elem->data_type));
            }

            m_fields.emplace_back(field, field.get_name(), width);
            continue;
        }

        if (field.is_element()) {
            const struct fds_iemgr_elem *elem = field.get_element();
            size_t width = std::max(
                strlen(elem->name),
                data_length(elem->data_type));

            m_fields.emplace_back(field, elem->name, width);
            continue;
        }

        throw std::runtime_error("Failed to process output field '" + name + "'");
    }
}

void
TablePrinter::parse_opts(const std::string &str)
{
    std::vector<std::string> opt_names;

    if (str.empty()) {
        return;
    }

    opt_names = string_split(str, ",");

    for (const std::string &opt_raw : opt_names) {
        std::string opt = string_trim_copy(opt_raw);

        if (strcasecmp(opt.c_str(), "no-biflow-split") == 0) {
            m_biflow_split = false;
        } else {
            throw std::invalid_argument("Table output: unknown option '" + opt + "'");
        }
    }
}

size_t
TablePrinter::data_length(fds_iemgr_element_type type)
{
    switch (type) {
    case FDS_ET_UNSIGNED_8:
        return std::numeric_limits<uint8_t>::digits10;
    case FDS_ET_UNSIGNED_16:
        return std::numeric_limits<uint16_t>::digits10;
    case FDS_ET_UNSIGNED_32:
        return std::numeric_limits<uint32_t>::digits10;
    case FDS_ET_UNSIGNED_64:
        return std::numeric_limits<uint64_t>::digits10;
    case FDS_ET_SIGNED_8:
        return std::numeric_limits<int8_t>::digits10;
    case FDS_ET_SIGNED_16:
        return std::numeric_limits<int16_t>::digits10;
    case FDS_ET_SIGNED_32:
        return std::numeric_limits<int32_t>::digits10;
    case FDS_ET_SIGNED_64:
        return std::numeric_limits<int64_t>::digits10;
    case FDS_ET_FLOAT_32:
        return std::numeric_limits<float>::digits10;
    case FDS_ET_FLOAT_64:
        return std::numeric_limits<double>::digits10;
    case FDS_ET_BOOLEAN:
        return 5; // "false"
    case FDS_ET_MAC_ADDRESS:
        return strlen("XX:XX:XX:XX:XX:XX");
    case FDS_ET_DATE_TIME_SECONDS:
        return strlen("YYYY-MM-DD HH-MM-SS");
    case FDS_ET_DATE_TIME_MILLISECONDS:
        return strlen("YYYY-MM-DD HH-MM-SS.SSS");
    case FDS_ET_DATE_TIME_MICROSECONDS:
        return strlen("YYYY-MM-DD HH-MM-SS.SSSSSS");
    case FDS_ET_DATE_TIME_NANOSECONDS:
        return strlen("YYYY-MM-DD HH-MM-SS.SSSSSSSSS");
    case FDS_ET_IPV4_ADDRESS:
        return strlen("XXX.XXX.XXX.XXX");
    case FDS_ET_IPV6_ADDRESS:
        return strlen("XXXX:XXXXX:XXXX:XXXX:XXXX:XXXX:XXXX:XXXX");
    default:
        return 16U;
    }
}

void
TablePrinter::print_prologue()
{
    bool is_first = true;

    for (const auto &field : m_fields) {
        if (!is_first) {
            std::cout << ' ';
        }

        is_first = false;
        std::cout << std::setw(field.m_width) << std::left << field.m_name;
    }

    std::cout << "\n";
    is_first = true;

    for (const auto &field : m_fields) {
        if (!is_first) {
            std::cout << ' ';
        }

        is_first = false;
        std::cout << std::string(field.m_width, '-');
    }

    std::cout << "\n";
}

void
TablePrinter::print_record(struct fds_drec *rec, bool reverse)
{
    bool is_first = true;

    for (auto &field : m_fields) {
        if (!is_first) {
            std::cout << ' ';
        }

        is_first = false;
        std::cout << std::setw(field.m_width) << std::right;
        print_value(rec, field, reverse);
    }

    std::cout << '\n';
}

unsigned int
TablePrinter::print_record(Flow *flow)
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
TablePrinter::print_value(struct fds_drec *rec, FieldInfo &field, bool reverse)
{
    Field &desc = field.m_field;
    unsigned int count;

    auto cb = [this](const struct fds_drec_field &field) -> void {
        if (!m_buffer.empty()) {
            m_buffer.push_back(',');
        }

        buffer_append(field);
    };

    m_buffer.clear();
    count = desc.for_each(rec, cb, reverse);

    if (count == 0) {
        // Not found
        std::cout << "N/A";
    } else if (count > 1) {
        // Multiple values
        m_buffer.insert(0, 1, '[');
        m_buffer.push_back(']');
        std::cout << m_buffer;
    } else {
        // Single value
        std::cout << m_buffer;
    }
}

void
TablePrinter::buffer_append(const struct fds_drec_field &field)
{
    const struct fds_iemgr_elem *elem = field.info->def;
    const enum fds_iemgr_element_type type = (elem != nullptr)
        ? elem->data_type
        : FDS_ET_OCTET_ARRAY;
    char buffer[512];
    int ret;

    ret = fds_field2str_be(field.data, field.size, type, buffer, sizeof(buffer));
    if (ret >= 0) {
        m_buffer.append(buffer);
        return;
    }

    switch (ret) {
    case FDS_ERR_BUFFER:
        m_buffer.append("<too long>");
        break;
    case FDS_ERR_FORMAT:
        m_buffer.append("<unsupported>");
        break;
    case FDS_ERR_ARG:
        m_buffer.append("<invalid>");
        break;
    default:
        m_buffer.append("<error>");
        break;
    }
}

} // lister
} // fdsdump

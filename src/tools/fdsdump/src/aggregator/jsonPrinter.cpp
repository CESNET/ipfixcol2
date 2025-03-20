/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief JSON printer for aggregated records
 *
 * Copyright: (C) 2024 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <iostream>

#include <aggregator/jsonPrinter.hpp>
#include <aggregator/print.hpp>

namespace fdsdump {
namespace aggregator {

JSONPrinter::JSONPrinter(const View &view)
    : m_view(view)
{
    m_buffer.reserve(1024);
}

JSONPrinter::~JSONPrinter()
{
}

void
JSONPrinter::print_prologue()
{
    std::cout << "[";
}

void
JSONPrinter::print_record(uint8_t *record)
{
    m_buffer.clear();
    m_buffer.push_back('{');

    size_t field_cnt = 0;

    for (const auto &pair : m_view.iter_fields(record)) {
        if (field_cnt++ > 0) {
            m_buffer.push_back(',');
        }
        append_field(pair.field, &pair.value);
    }

    m_buffer.push_back('}');
    std::cout << ((m_rec_printed++ > 0) ? ",\n " : "\n ");
    std::cout << m_buffer;
}

void
JSONPrinter::print_epilogue()
{
    std::cout << "\n]\n";
}

void
JSONPrinter::append_field(const Field &field, Value *value)
{
    m_buffer.push_back('"');
    m_buffer.append(field.name());
    m_buffer.push_back('"');

    m_buffer.push_back(':');

    append_value(field, value);
}

void
JSONPrinter::append_value(const Field &field, Value *value)
{
    switch (field.data_type()) {
    case DataType::IPAddress:
    case DataType::IPv4Address:
    case DataType::IPv6Address:
    case DataType::MacAddress:
    case DataType::DateTime:
        m_buffer.push_back('"');
        print_value(field, *value, m_buffer);
        m_buffer.push_back('"');
        return;
    case DataType::Unsigned8:
    case DataType::Signed8:
    case DataType::Unsigned16:
    case DataType::Signed16:
    case DataType::Unsigned32:
    case DataType::Signed32:
    case DataType::Unsigned64:
    case DataType::Signed64:
        print_value(field, *value, m_buffer);
        return;
    case DataType::String128B:
        m_buffer.push_back('"');
        append_string_value(value);
        m_buffer.push_back('"');
        return;
    case DataType::Octets128B:
        m_buffer.push_back('"');
        append_octet_value(value);
        m_buffer.push_back('"');
        return;
    case DataType::VarString:
        m_buffer.push_back('"');
        append_varstring_value(value);
        m_buffer.push_back('"');
        return;
    case DataType::Unassigned:
        m_buffer.append("null");
        return;
    }
}

void
JSONPrinter::append_string_value(const Value *value)
{
    size_t last_byte;

    for (last_byte = sizeof(value->str); last_byte > 0; --last_byte) {
        if (value->str[last_byte - 1] != 0) {
            break;
        }
    }

    for (size_t i = 0; i < last_byte; ++i) {
        const char byte = value->str[i];

        // Escape characters
        switch (byte) {
        case '"':
            m_buffer.append("\\\"");
            continue;
        case '\'':
            m_buffer.append("\\\\");
            continue;
        case '/':
            m_buffer.append("\\/");
            continue;;
        case '\b':
            m_buffer.append("\\b");
            continue;
        case '\f':
            m_buffer.append("\\f");
            continue;
        case '\n':
            m_buffer.append("\\n");
            continue;
        case '\r':
            m_buffer.append("\\r");
            continue;
        case '\t':
            m_buffer.append("\\t");
            continue;
        default:
            break;
        }

        if (byte >= '\x00' && byte <= '\x1F') {
            m_buffer.append(char2hex(byte));
        } else {
            m_buffer.append(1, byte);
        }
    }
}

void
JSONPrinter::append_varstring_value(const Value *value)
{
    for (uint32_t i = 0; i < value->varstr.len; ++i) {
        const char byte = value->varstr.text[i];

        // Escape characters
        switch (byte) {
        case '"':
            m_buffer.append("\\\"");
            continue;
        case '\'':
            m_buffer.append("\\\\");
            continue;
        case '/':
            m_buffer.append("\\/");
            continue;;
        case '\b':
            m_buffer.append("\\b");
            continue;
        case '\f':
            m_buffer.append("\\f");
            continue;
        case '\n':
            m_buffer.append("\\n");
            continue;
        case '\r':
            m_buffer.append("\\r");
            continue;
        case '\t':
            m_buffer.append("\\t");
            continue;
        default:
            break;
        }

        if (byte >= '\x00' && byte <= '\x1F') {
            m_buffer.append(char2hex(byte));
        } else {
            m_buffer.append(1, byte);
        }
    }
}

void
JSONPrinter::append_octet_value(const Value *value)
{
    size_t last_byte;

    m_buffer.append("0x");

    // Find first non-zero byte from the end (at least one byte must be printed)
    for (last_byte = sizeof(value->str); last_byte > 1; --last_byte) {
        if (value->str[last_byte - 1] != 0) {
            break;
        }
    }

    for (size_t i = 0; i < last_byte; ++i) {
        const char byte = value->str[i];
        m_buffer.append(char2hex(byte));
    }
}

} // aggregator
} // fdsdump

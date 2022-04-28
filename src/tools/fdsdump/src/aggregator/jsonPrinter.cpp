#include <iostream>

#include "jsonPrinter.hpp"
#include "print.hpp"

JSONPrinter::JSONPrinter(ViewDefinition view_def)
    : m_view_def(view_def)
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
    ViewValue *value = (ViewValue *) record;
    size_t field_cnt = 0;

    m_buffer.clear();
    m_buffer.push_back('{');

    for (const auto &field : m_view_def.key_fields) {
        if (field_cnt++ > 0) {
            m_buffer.push_back(',');
        }

        append_field(field, value);
        advance_value_ptr(value, field.size);
    }

    for (const auto &field : m_view_def.value_fields) {
        if (field_cnt++ > 0) {
            m_buffer.push_back(',');
        }

        append_field(field, value);
        advance_value_ptr(value, field.size);
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
JSONPrinter::append_field(const ViewField &field, ViewValue *value)
{
    m_buffer.push_back('"');
    m_buffer.append(field.name);
    m_buffer.push_back('"');

    m_buffer.push_back(':');

    append_value(field, value);
}

void
JSONPrinter::append_value(const ViewField &field, ViewValue *value)
{
    char tmp[256];
    print_value(field, *value, tmp, false);

    switch (field.data_type) {
    case DataType::Unassigned:
    case DataType::IPAddress:
    case DataType::IPv4Address:
    case DataType::IPv6Address:
    case DataType::MacAddress:
    case DataType::DateTime:
    case DataType::String128B:
        m_buffer.push_back('"');
        m_buffer.append(tmp);
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
        m_buffer.append(tmp);
        return;
    }
}

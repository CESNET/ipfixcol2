#include "table_printer.hpp"
#include <cstdio>
#include <arpa/inet.h>

static int
get_width(const ViewField &field)
{
    switch (field.data_type) {
    case DataType::Unsigned8:
    case DataType::Signed8:
        return 5;
    case DataType::Unsigned16:
    case DataType::Signed16:
        return 5;
    case DataType::Unsigned32:
    case DataType::Signed32:
        return 8;
    case DataType::Unsigned64:
    case DataType::Signed64:
        return 12;
    case DataType::IPAddress:
    case DataType::IPv6Address:
        return 39;
    case DataType::IPv4Address:
        return 15;
    }
}

static void
print_value(const ViewField &field, ViewValue &value, char *buffer)
{
    switch (field.data_type) {
    case DataType::Unsigned8:
        sprintf(buffer, "%hhu", value.u8);
        break;
    case DataType::Unsigned16:
        sprintf(buffer, "%hu", value.u16);
        break;
    case DataType::Unsigned32:
        sprintf(buffer, "%u", value.u32);
        break;
    case DataType::Unsigned64:
        sprintf(buffer, "%lu", value.u64);
        break;
    case DataType::Signed8:
        sprintf(buffer, "%hhd", value.i8);
        break;
    case DataType::Signed16:
        sprintf(buffer, "%hd", value.i16);
        break;
    case DataType::Signed32:
        sprintf(buffer, "%d", value.i32);
        break;
    case DataType::Signed64:
        sprintf(buffer, "%ld", value.i64);
        break;
    case DataType::IPAddress:
        inet_ntop(value.ip.length == 4 ? AF_INET : AF_INET6, value.ip.address, buffer, 64);
        break;
    case DataType::IPv4Address:
        inet_ntop(AF_INET, value.ipv4, buffer, 64);
        break;
    case DataType::IPv6Address:
        inet_ntop(AF_INET6, value.ipv6, buffer, 64);
        break;
    default: assert(0);
    }
}

TablePrinter::TablePrinter(ViewDefinition view_def)
    : m_view_def(view_def)
{
}

TablePrinter::~TablePrinter()
{
}

void
TablePrinter::print_prologue()
{
    for (const auto &field : m_view_def.key_fields) {
        printf("%*s ", get_width(field), field.name.c_str());
    }

    for (const auto &field : m_view_def.value_fields) {
        printf("%*s ", get_width(field), field.name.c_str());
    }

    printf("\n");
}

void
TablePrinter::print_record(AggregateRecord &record)
{
    char buffer[1024] = {0};
    ViewValue *value = reinterpret_cast<ViewValue *>(record.data);

    for (const auto &field : m_view_def.key_fields) {
        print_value(field, *value, buffer);
        advance_value_ptr(value, field.size);
        printf("%*s ", get_width(field), buffer);
    }

    for (const auto &field : m_view_def.value_fields) {
        print_value(field, *value, buffer);
        advance_value_ptr(value, field.size);
        printf("%*s ", get_width(field), buffer);
    }

    printf("\n");
}

void
TablePrinter::print_epilogue()
{
}
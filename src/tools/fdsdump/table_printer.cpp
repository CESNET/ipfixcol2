#include "table_printer.hpp"
#include <cstdio>
#include <arpa/inet.h>

static void
print_value(const ViewField &field, ViewValue &value)
{
    char buffer[64];
    switch (field.data_type) {
    case DataType::Unsigned8:
        printf("%hhu", value.u8);
        break;
    case DataType::Unsigned16:
        printf("%hu", value.u16);
        break;
    case DataType::Unsigned32:
        printf("%u", value.u32);
        break;
    case DataType::Unsigned64:
        printf("%lu", value.u64);
        break;
    case DataType::Signed8:
        printf("%hhd", value.i8);
        break;
    case DataType::Signed16:
        printf("%hd", value.i16);
        break;
    case DataType::Signed32:
        printf("%d", value.i32);
        break;
    case DataType::Signed64:
        printf("%ld", value.i64);
        break;
    case DataType::IPAddress:
        inet_ntop(value.ip.length == 4 ? AF_INET : AF_INET6, value.ip.address, buffer, 64);
        printf("%s", buffer);
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
        printf("%s ", field.name.c_str());
    }

    for (const auto &field : m_view_def.value_fields) {
        printf("%s ", field.name.c_str());
    }

    printf("\n");
}

void
TablePrinter::print_record(AggregateRecord &record)
{
    ViewValue *value = reinterpret_cast<ViewValue *>(record.data);

    for (const auto &field : m_view_def.key_fields) {
        print_value(field, *value);
        advance_value_ptr(value, field.size);
        printf(" ");
    }

    for (const auto &field : m_view_def.value_fields) {
        print_value(field, *value);
        advance_value_ptr(value, field.size);
        printf(" ");
    }

    printf("\n");
}

void
TablePrinter::print_epilogue()
{
}
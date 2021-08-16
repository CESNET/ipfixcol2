#include "view/tableprinter.hpp"

#include <cstdio>

#include "ipfix/informationelements.hpp"
#include "view/print.hpp"

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
TablePrinter::print_record(uint8_t *record)
{
    char buffer[1024] = {0}; //TODO: We might want to do this some other way
    ViewValue *value = (ViewValue *) record;

    for (const auto &field : m_view_def.key_fields) {
        print_value(field, *value, buffer, m_translate_ip_addrs);
        advance_value_ptr(value, field.size);
        printf("%*s ", get_width(field), buffer);
    }

    for (const auto &field : m_view_def.value_fields) {
        print_value(field, *value, buffer, m_translate_ip_addrs);
        advance_value_ptr(value, field.size);
        printf("%*s ", get_width(field), buffer);
    }

    printf("\n");
}

void
TablePrinter::print_epilogue()
{
}

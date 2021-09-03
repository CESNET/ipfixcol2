#include "jsonprinter.hpp"

JSONPrinter::JSONPrinter(ViewDefinition view_def)
    : m_view_def(view_def)
{
}

JSONPrinter::~JSONPrinter()
{
}

void
JSONPrinter::print_prologue()
{
    printf("{");
}

void
JSONPrinter::print_record(uint8_t *record)
{
    char buffer[1024] = {0};
    ViewValue *value = (ViewValue *) record;

    size_t fields_count = m_view_def.key_fields.size() + m_view_def.value_fields.size();
    size_t i = 0;

    if (m_first_record) {
        printf("\n  {\n");
        m_first_record = false;
    } else {
        printf(",\n  {\n");
    }

    for (const auto &field : m_view_def.key_fields) {
        print_value(field, *value, buffer, false);
        advance_value_ptr(value, field.size);
        printf("    \"%s\": \"%s\"%s\n", field.name.c_str(), buffer, i == fields_count - 1 ? "" : ",");

        i++;
    }

    for (const auto &field : m_view_def.value_fields) {
        print_value(field, *value, buffer, false);
        advance_value_ptr(value, field.size);
        printf("    \"%s\": \"%s\"%s\n", field.name.c_str(), buffer, i == fields_count - 1 ? "" : ",");

        i++;
    }

    printf("  }");
}

void
JSONPrinter::print_epilogue()
{
    printf("\n}\n");
}

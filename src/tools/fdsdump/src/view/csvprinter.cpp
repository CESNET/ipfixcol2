#include "csvprinter.hpp"

CSVPrinter::CSVPrinter(ViewDefinition view_def)
    : m_view_def(view_def)
{
}

CSVPrinter::~CSVPrinter()
{
}

void
CSVPrinter::print_prologue()
{
    size_t fields_count = m_view_def.key_fields.size() + m_view_def.value_fields.size();
    size_t i = 0;

    for (const auto &field : m_view_def.key_fields) {
        printf("%s%s", field.name.c_str(), i == fields_count - 1 ? "" : ",");
        i++;
    }

    for (const auto &field : m_view_def.value_fields) {
        printf("%s%s", field.name.c_str(), i == fields_count - 1 ? "" : ",");
        i++;
    }

    printf("\n");
}

void
CSVPrinter::print_record(uint8_t *record)
{
    char buffer[1024] = {0};
    ViewValue *value = (ViewValue *) record;

    size_t fields_count = m_view_def.key_fields.size() + m_view_def.value_fields.size();
    size_t i = 0;


    for (const auto &field : m_view_def.key_fields) {
        print_value(field, *value, buffer, false);
        advance_value_ptr(value, field.size);
        printf("%s%s", buffer, i == fields_count - 1 ? "" : ",");

        i++;
    }

    for (const auto &field : m_view_def.value_fields) {
        print_value(field, *value, buffer, false);
        advance_value_ptr(value, field.size);
        printf("%s%s", buffer, i == fields_count - 1 ? "" : ",");

        i++;
    }

    printf("\n");
}

void
CSVPrinter::print_epilogue()
{
}

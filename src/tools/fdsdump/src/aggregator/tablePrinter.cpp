/**
 * @file
 * @author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * @brief Table printer implementation
 */

#include <iomanip>
#include <iostream>

#include "tablePrinter.hpp"
#include "informationElements.hpp"
#include "print.hpp"

namespace fdsdump {
namespace aggregator {

TABLEPrinter::TABLEPrinter(ViewDefinition view_def)
    : m_view_def(view_def)
{
    m_buffer.reserve(1024);
}

TABLEPrinter::~TABLEPrinter()
{
}

void
TABLEPrinter::print_prologue()
{
    bool is_first = true;

    for (const auto &field : m_view_def.key_fields) {
        if (!is_first) {
            std::cout << ' ';
        }

        is_first = false;
        std::cout << std::setw(get_width(field)) << field.name;
    }

    for (const auto &field : m_view_def.value_fields) {
        if (!is_first) {
            std::cout << ' ';
        }

        is_first = false;
        std::cout << std::setw(get_width(field)) << field.name;
    }

    std::cout << "\n";
    is_first = true;

    for (const auto &field : m_view_def.key_fields) {
        if (!is_first) {
            std::cout << ' ';
        }

        is_first = false;
        std::cout << std::string(get_width(field), '-');
    }

    for (const auto &field : m_view_def.value_fields) {
        if (!is_first) {
            std::cout << ' ';
        }

        is_first = false;
        std::cout << std::string(get_width(field), '-');
    }

    std::cout << "\n";
}

void
TABLEPrinter::print_record(uint8_t *record)
{
    ViewValue *value = (ViewValue *) record;
    std::string buffer;
    bool is_first = true;

    for (const auto &field : m_view_def.key_fields) {
        if (!is_first) {
            std::cout << ' ';
        }

        is_first = false;
        buffer.clear();
        print_value(field, *value, buffer);
        advance_value_ptr(value, field.size);

        std::cout << std::setw(get_width(field)) << std::right;
        std::cout << buffer;
    }

    for (const auto &field : m_view_def.value_fields) {
        if (!is_first) {
            std::cout << ' ';
        }

        is_first = false;
        buffer.clear();
        print_value(field, *value, buffer);
        advance_value_ptr(value, field.size);

        std::cout << std::setw(get_width(field)) << std::right;
        std::cout << buffer;
    }

    std::cout << '\n';
}

void
TABLEPrinter::print_epilogue()
{
}

} // aggregator
} // fdsdump

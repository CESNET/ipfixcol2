/**
 * @file
 * @author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * @brief Table printer implementation
 *
 * Copyright: (C) 2024 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <iomanip>
#include <iostream>

#include <aggregator/print.hpp>
#include <aggregator/tablePrinter.hpp>

namespace fdsdump {
namespace aggregator {

TABLEPrinter::TABLEPrinter(const View &view)
    : m_view(view)
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

    for (const auto &field : m_view.fields()) {
        if (!is_first) {
            std::cout << ' ';
        }

        is_first = false;
        std::cout << std::setw(get_width(*field.get())) << field->name();
    }

    std::cout << "\n";
    is_first = true;

    for (const auto &field : m_view.fields()) {
        if (!is_first) {
            std::cout << ' ';
        }

        is_first = false;
        std::cout << std::string(get_width(*field.get()), '-');
    }

    std::cout << "\n";
}

void
TABLEPrinter::print_record(uint8_t *record)
{
    std::string buffer;
    bool is_first = true;

    for (const auto &pair : m_view.iter_fields(record)) {
        if (!is_first) {
            std::cout << ' ';
        }

        is_first = false;
        buffer.clear();
        print_value(pair.field, pair.value, buffer);

        std::cout << std::setw(get_width(pair.field)) << std::right;
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

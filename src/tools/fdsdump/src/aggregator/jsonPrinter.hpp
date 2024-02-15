/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief JSON printer for aggregated records
 *
 * Copyright: (C) 2024 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <string>

#include <aggregator/field.hpp>
#include <aggregator/printer.hpp>
#include <aggregator/value.hpp>
#include <aggregator/view.hpp>

namespace fdsdump {
namespace aggregator {

class JSONPrinter : public Printer
{
public:
    JSONPrinter(const View &view);

    ~JSONPrinter() override;

    void
    print_prologue() override;

    void
    print_record(uint8_t *record) override;

    void
    print_epilogue() override;

private:
    void append_field(const Field &field, Value *value);
    void append_value(const Field &field, Value *value);
    void append_string_value(const Value *value);
    void append_octet_value(const Value *value);
    void append_varstring_value(const Value *value);

    const View &m_view;
    std::string m_buffer;
    size_t m_rec_printed = 0;
};

} // aggregator
} // fdsdump

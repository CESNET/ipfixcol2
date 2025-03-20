/**
 * @file
 * @author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * @brief Table printer implementation
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

class TABLEPrinter : public Printer
{
public:
    bool m_translate_ip_addrs = false;

    TABLEPrinter(const View &view);

    ~TABLEPrinter() override;

    void
    print_prologue() override;

    void
    print_record(uint8_t *record) override;

    void
    print_epilogue() override;

private:
    const View &m_view;
    std::string m_buffer;
};

} // aggregator
} // fdsdump

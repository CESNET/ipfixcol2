/**
 * @file
 * @author Lukas Hutak <hutak@cesnet.cz>
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Table statistics printer
 *
 * Copyright: (C) 2024 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <string>
#include <statistics/printer.hpp>

namespace fdsdump {
namespace statistics {

class TablePrinter : public Printer {
public:
    TablePrinter(const std::string &args);

    virtual
    ~TablePrinter();

    virtual void
    print_prologue() override;

    virtual void
    print_stats(const fds_file_stats &stats) override;

    virtual void
    print_epilogue() override;
};

} // statistics
} // fdsdump

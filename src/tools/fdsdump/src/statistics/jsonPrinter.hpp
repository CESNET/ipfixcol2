/**
 * @file
 * @author Lukas Hutak <hutak@cesnet.cz>
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief JSON statistics printer
 *
 * Copyright: (C) 2024 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <string>
#include <statistics/printer.hpp>

namespace fdsdump {
namespace statistics {

class JsonPrinter : public Printer {
public:
    JsonPrinter(const std::string &args);

    virtual
    ~JsonPrinter();

    virtual void
    print_prologue() override;

    virtual void
    print_stats(const fds_file_stats &stats) override;

    virtual void
    print_epilogue() override;
};

} // statistics
} // fdsdump

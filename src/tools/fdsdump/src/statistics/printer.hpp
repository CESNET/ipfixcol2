/**
 * @file
 * @author Lukas Hutak <hutak@cesnet.cz>
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Statistics printer
 *
 * Copyright: (C) 2024 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <string>
#include <libfds.h>

#include <common/common.hpp>

namespace fdsdump {
namespace statistics {

/**
 * @brief Interface of an output printer for statistics.
 */
class Printer {
public:
    virtual
    ~Printer() {};

    virtual void
    print_prologue() = 0;

    virtual void
    print_stats(const fds_file_stats &stats) = 0;

    virtual void
    print_epilogue() = 0;
};

std::unique_ptr<Printer>
printer_factory(const std::string &manual);

} // statistics
} // fdsdump

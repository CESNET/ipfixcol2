/**
 * @file
 * @author Lukas Hutak <hutak@cesnet.cz>
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Statistics mode entrypoint
 *
 * Copyright: (C) 2024 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <common/common.hpp>
#include <options.hpp>

namespace fdsdump {
namespace statistics {

void
mode_statistics(const Options &opts);

} // statistics
} // fdsdump

/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Aggregator mode
 *
 * Copyright: (C) 2024 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <common/common.hpp>
#include <options.hpp>

namespace fdsdump {
namespace aggregator {

void
mode_aggregate(const Options &opts);

} // aggregator
} // fdsdump

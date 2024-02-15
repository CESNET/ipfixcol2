/**
 * @file
 * @author Lukas Hutak <hutak@cesnet.cz>
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Lister entrypoint
 *
 * Copyright: (C) 2024 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <options.hpp>
#include <common/flowProvider.hpp>

namespace fdsdump {
namespace lister {

void
mode_list(const Options &opts);

} // lister
} // fdsdump

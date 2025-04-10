/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Block representation
 * @date 2025
 *
 * Copyright(c) 2025 CESNET z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <vector>
#include "clickhouse.h"

struct Block {
    std::vector<std::shared_ptr<clickhouse::Column>> columns;
    clickhouse::Block block;
    unsigned int rows = 0;
};

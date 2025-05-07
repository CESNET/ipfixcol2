/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Column representation
 * @date 2025
 *
 * Copyright(c) 2025 CESNET z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "clickhouse.h"
#include "config.h"
#include "datatype.h"
#include <libfds.h>
#include <string>

/**
 * @class Column
 * @brief Information about a column
 */
struct Column {
    std::string name;
    DataType datatype;
    bool nullable;

    const fds_iemgr_elem *elem = nullptr;
    const fds_iemgr_alias *alias = nullptr;
    SpecialField special = SpecialField::NONE;
};

/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Common functionality and general utilities
 * @date 2025
 *
 * Copyright(c) 2025 CESNET z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "common.h"

#include <cctype>

std::string to_lower(std::string str)
{
    for (auto &c : str) {
        c = std::tolower(c);
    }
    return str;
}

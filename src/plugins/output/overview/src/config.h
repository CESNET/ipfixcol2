/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Configuration parsing and representation
 * @date 2025
 *
 * Copyright(c) 2025 CESNET z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <libfds.h>

/**
 * @class Config
 * @brief A struct containing all the configurable plugin parameters
 */
struct Config {
    bool skip_option_templates = false;
};

/**
 * @brief Parse a XML config into a structured form
 *
 * @param xml The config as a XML string
 * @param iemgr The iemgr instance
 * @return The parsed config
 */
Config parse_config(const char *xml);

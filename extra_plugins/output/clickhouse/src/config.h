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

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

enum class SpecialField {
    NONE = 0,
    ODID,
};

/**
 * @class Config
 * @brief A struct containing all the configurable plugin parameters
 */
struct Config {
    struct Column {
        std::string name;
        bool nullable = false;
        std::variant<const fds_iemgr_elem *, const fds_iemgr_alias *, SpecialField> source;
    };

    struct Endpoint {
        std::string host;
        uint16_t port = 9000;
    };

    struct Connection {
        std::vector<Endpoint> endpoints;
        std::string user;
        std::string password;
        std::string database;
        std::string table;
    };

    Connection connection;
    std::vector<Config::Column> columns;
    uint64_t inserter_threads = 8;
    uint64_t blocks = 64;
    uint64_t block_insert_threshold = 100000;
    uint64_t block_insert_max_delay_secs = 10;
    bool split_biflow = true;
    bool biflow_empty_autoignore = true;
    bool nonblocking = true;
};

/**
 * @brief Parse a XML config into a structured form
 *
 * @param xml The config as a XML string
 * @param iemgr The iemgr instance
 * @return The parsed config
 */
Config parse_config(const char *xml, const fds_iemgr_t *iemgr);

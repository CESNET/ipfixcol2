/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief
 * @date 2024
 *
 * Copyright(c) 2024 CESNET z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <libfds.h>

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

using VectorOfElemOrAlias = std::vector<std::variant<const fds_iemgr_elem *, const fds_iemgr_alias *>>;

struct Config {
    std::string host;
    uint16_t port;
    std::string user;
    std::string password;
    std::string database;
    std::string table;
    VectorOfElemOrAlias fields;
    unsigned int num_inserter_threads = 32;
    unsigned int num_blocks = 256;
    unsigned int block_insert_threshold = 100000;
};

Config parse_config(const char *xml, const fds_iemgr_t *iemgr);

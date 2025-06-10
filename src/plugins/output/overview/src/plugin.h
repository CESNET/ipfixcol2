/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Main plugin class header file
 * @date 2025
 *
 * Copyright(c) 2025 CESNET z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "config.h"
#include <ipfixcol2.h>
#include <map>
#include <vector>

class Plugin {
public:
    /**
     * @brief Instantiate the plugin instance
     *
     * @param ctx The collector context
     * @param xml_config The config in form of a XML string
     */
    Plugin(ipx_ctx_t *ctx, const char *xml_config);

    // Disable copy and move
    Plugin(Plugin &&) = delete;
    Plugin(const Plugin &) = delete;
    Plugin &operator=(Plugin &&) = delete;
    Plugin &operator=(const Plugin &) = delete;

    /**
     * @brief Process a collector message
     *
     * @param msg The collector message
     */
    void process(ipx_msg_t *msg);

    /**
     * @brief Stop the plugin and wait till it is stopped (blocking)
     */
    void stop();

private:
    ipx_ctx_t *m_ctx;
    Config m_config;
    uint64_t m_total_rec_count;
    std::map<uint64_t, uint64_t> m_field_counts; // (pen, id) -> count
    std::vector<uint64_t> m_fields; // fields in the order we first saw them
};

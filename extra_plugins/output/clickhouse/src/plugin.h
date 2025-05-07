/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Main plugin class
 * @date 2025
 *
 * Copyright(c) 2025 CESNET z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "block.h"
#include "clickhouse.h"
#include "column.h"
#include "common.h"
#include "config.h"
#include "inserter.h"
#include "recparser.h"
#include "stats.h"

#include <ipfixcol2.h>
#include <libfds.h>
#include <memory>


/**
 * @class Plugin
 * @brief Class representing the ClickHouse output plugin
 */
class Plugin : Nonmoveable, Noncopyable {
public:
    /**
     * @brief Instantiate the plugin instance
     *
     * @param ctx The collector context for logging
     * @param xml_config The config in form of a XML string
     */
    Plugin(ipx_ctx_t *ctx, const char *xml_config);

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
    friend class Stats;

    Logger m_logger;
    Config m_config;
    std::vector<Column> m_columns;

    std::vector<std::unique_ptr<Block>> m_blocks;

    std::vector<std::unique_ptr<Inserter>> m_inserters;

    SyncQueue<Block *> m_avail_blocks;
    SyncQueue<Block *> m_filled_blocks;

    Block *m_current_block = nullptr;

    Stats m_stats;

    std::time_t m_last_insert_time = 0;

    std::unique_ptr<RecParserManager> m_rec_parsers;

    void
    process_ipfix_msg(ipx_msg_ipfix_t *msg);

    void
    process_session_msg(ipx_msg_session_t *msg);

    int
    process_record(ipx_msg_ipfix_t *msg, fds_drec &rec, Block &block);

    void
    extract_values(ipx_msg_ipfix_t *msg, RecParser &parser, Block &block, bool rev);
};

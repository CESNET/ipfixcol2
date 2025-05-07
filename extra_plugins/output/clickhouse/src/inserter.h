/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Inserter class for inserting data into ClickHouse
 * @date 2025
 *
 * Copyright(c) 2025 CESNET z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "worker.h"
#include "block.h"
#include "syncqueue.h"
#include "column.h"

/**
 * @class Inserter
 * @brief A worker class responsible for inserting data into a ClickHouse table.
 */
class Inserter : public Worker {
public:
    /**
     * @brief Constructor for the Inserter class.
     * @param id Unique identifier for the inserter.
     * @param logger Logger instance for logging operations.
     * @param client_opts Options for configuring the ClickHouse client.
     * @param table_name Name of the ClickHouse table to insert data into.
     * @param columns Reference to the vector of columns defining the table schema.
     * @param input_blocks Reference to the queue of input blocks to be inserted.
     * @param avail_blocks Reference to the queue of available blocks for reuse.
     */
    Inserter(
        int id,
        Logger logger,
        clickhouse::ClientOptions client_opts,
        std::string table_name,
        const std::vector<Column> &columns,
        SyncQueue<Block *> &input_blocks,
        SyncQueue<Block *> &avail_blocks);

private:
    int m_id; ///< Unique identifier for the inserter.
    Logger m_logger; ///< Logger instance for logging operations.
    clickhouse::ClientOptions m_client_opts; ///< Options for configuring the ClickHouse client.
    std::string m_table_name; ///< Name of the ClickHouse table to insert data into.
    const std::vector<Column> &m_columns; ///< Reference to the vector of columns defining the table schema.
    SyncQueue<Block *> &m_input_blocks; ///< Reference to the queue of input blocks to be inserted.
    SyncQueue<Block *> &m_avail_blocks; ///< Reference to the queue of available blocks for reuse.
    std::unique_ptr<clickhouse::Client> m_client; ///< Pointer to the ClickHouse client instance.

    void run() override;

    bool insert(clickhouse::Block &block);
};

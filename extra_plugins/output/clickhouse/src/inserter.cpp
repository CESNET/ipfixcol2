/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Inserter class for inserting data into ClickHouse
 * @date 2025
 *
 * Copyright(c) 2025 CESNET z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "inserter.h"
#include <sstream>
#include <string>
#include <utility>
#include <vector>

static constexpr int ERR_TABLE_NOT_FOUND = 60;
static constexpr int STOP_TIMEOUT_SECS = 10;

static std::vector<std::pair<std::string, std::string>> describe_table(clickhouse::Client &client, const std::string &table)
{
    std::vector<std::pair<std::string, std::string>> name_and_type;
    try {
        client.Select("DESCRIBE TABLE " + table, [&](const clickhouse::Block &block) {
            if (block.GetColumnCount() > 0 && block.GetRowCount() > 0) {
                const auto &name = block[0]->As<clickhouse::ColumnString>();
                const auto &type = block[1]->As<clickhouse::ColumnString>();
                for (size_t i = 0; i < block.GetRowCount(); i++) {
                    name_and_type.emplace_back(name->At(i), type->At(i));
                }
            }
        });
    } catch (const clickhouse::ServerException &exc) {
        if (exc.GetCode() == ERR_TABLE_NOT_FOUND) {
            throw Error("table \"{}\" does not exist", table);
        } else {
            throw;
        }
    }
    return name_and_type;
}

static void ensure_schema(clickhouse::Client &client, const std::string &table, const std::vector<Column> &columns)
{
    // Check that the database has the necessary columns
    auto db_columns = describe_table(client, table);

    auto schema_hint = [&](){
        std::stringstream ss;
        ss << "hint:\n";
        ss << "CREATE TABLE " << table << "(\n";
        size_t i = 0;
        for (const auto& column : columns) {
            const auto &clickhouse_type = type_to_clickhouse(columns[i].datatype, columns[i].nullable);
            ss << "    \"" << column.name << "\" " << clickhouse_type << (i < columns.size()-1 ? "," : "") << '\n';
            i++;
        }
        ss << ");";
        return ss.str();
    };

    if (columns.size() != db_columns.size()) {
        throw Error("config has {} columns but table \"{}\" has {}\n{}", columns.size(), table, db_columns.size(), schema_hint());
    }

    for (size_t i = 0; i < db_columns.size(); i++) {
        const auto &expected_name = columns[i].name;
        const auto &expected_type = type_to_clickhouse(columns[i].datatype, columns[i].nullable);
        const auto &[actual_name, actual_type] = db_columns[i];

        if (expected_name != actual_name) {
            throw Error("expected column #{} in table \"{}\" to be named \"{}\" but it is \"{}\"\n{}",
                        i, table, expected_name, actual_name, schema_hint());
        }

        if (expected_type != actual_type) {
            throw Error("expected column #{} in table \"{}\" to be of type \"{}\" but it is \"{}\"\n{}",
                        i, table, expected_type, actual_type, schema_hint());
        }
    }
}

Inserter::Inserter(
        int id,
        Logger logger,
        clickhouse::ClientOptions client_opts,
        std::string table_name,
        const std::vector<Column> &columns,
        SyncQueue<Block *> &input_blocks,
        SyncQueue<Block *> &avail_blocks)
    : m_id(id)
    , m_logger(logger)
    , m_client_opts(client_opts)
    , m_table_name(table_name)
    , m_columns(columns)
    , m_input_blocks(input_blocks)
    , m_avail_blocks(avail_blocks)
{}

bool Inserter::insert(clickhouse::Block &block)
{
    bool needs_reconnect = false;
    while (true) {
        if (stop_requested() && secs_since_stop_requested() > STOP_TIMEOUT_SECS) {
            return false;
        }

        try {
            if (needs_reconnect) {
                m_client->ResetConnectionEndpoint();
                ensure_schema(*m_client.get(), m_table_name, m_columns);
                m_logger.warning("[Worker %d] Connected to %s:%d due to error with previous endpoint", m_id,
                              m_client->GetCurrentEndpoint()->host.c_str(), m_client->GetCurrentEndpoint()->port);
            }
            m_logger.debug("[Worker %d] Inserting %d rows", m_id, block.GetRowCount());
            m_client->Insert(m_table_name, block);
            break;

        } catch (const std::exception &ex) {
            m_logger.error("[Worker %d] Insert failed: %s - retrying in 1 second", m_id, ex.what());
            needs_reconnect = true;
        }

        if (stop_requested() && secs_since_stop_requested() > STOP_TIMEOUT_SECS) {
            return false;
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return true;
}

void Inserter::run()
{
    m_client = std::make_unique<clickhouse::Client>(m_client_opts);
    ensure_schema(*m_client.get(), m_table_name, m_columns);
    m_logger.info("[Worker %d] Connected to %s:%d", m_id,
                  m_client->GetCurrentEndpoint()->host.c_str(), m_client->GetCurrentEndpoint()->port);

    while (true) {
        Block *block = m_input_blocks.get();
        if (!block) {
            // We might get null as a way to get unblocked and process stop signal.
            // Nulls are inserted only after all the valid blocks are queued.
            break;
        }

        if (stop_requested() && secs_since_stop_requested() > STOP_TIMEOUT_SECS) {
            break;
        }

        block->block.RefreshRowCount();
        bool ok = insert(block->block);
        if (!ok) {
            // Do not clear the block as it could not have been inserted.
            // It will be used to count the number of dropped records.
            break;
        }

        for (auto &column : block->columns) {
            column->Clear();
        }
        block->rows = 0;
        m_avail_blocks.put(block);
    }
}

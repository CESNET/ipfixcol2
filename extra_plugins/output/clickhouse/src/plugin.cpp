/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Main plugin class
 * @date 2024
 *
 * Copyright(c) 2024 CESNET z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "plugin.h"
#include "datatype.h"

#include <ipfixcol2.h>
#include <libfds.h>

static constexpr int ERR_TABLE_NOT_FOUND = 60;
static constexpr int STATS_PRINT_INTERVAL_SECS = 1;

static ExtractorFn make_extractor(const fds_iemgr_elem &elem)
{
    return [=](fds_drec &drec, uint16_t iter_flags) -> std::optional<fds_drec_field> {
        uint32_t pen = elem.scope->pen;
        uint16_t id = elem.id;
        fds_drec_iter iter;
        fds_drec_iter_init(&iter, &drec, iter_flags);
        int ret = fds_drec_iter_find(&iter, pen, id);
        if (ret == FDS_EOC) {
            return {};
        }
        return iter.field;
    };
}

static ExtractorFn make_extractor(const fds_iemgr_alias &alias)
{
    return [=](fds_drec &drec, uint16_t iter_flags) -> std::optional<fds_drec_field> {
        fds_drec_iter iter;
        for (size_t i = 0; i < alias.sources_cnt; i++) {
            uint32_t pen = alias.sources[i]->scope->pen;
            uint16_t id = alias.sources[i]->id;
            fds_drec_iter_init(&iter, &drec, iter_flags);
            int ret = fds_drec_iter_find(&iter, pen, id);
            if (ret != FDS_EOC) {
                return iter.field;
            }
        }
        return {};
    };
}

static std::vector<ColumnCtx> prepare_columns(std::vector<Config::Column> &columns_cfg)
{
    std::vector<ColumnCtx> columns;

    for (const auto &column_cfg : columns_cfg) {
        ColumnCtx column{};
        DataType type;

        if (std::holds_alternative<const fds_iemgr_elem *>(column_cfg.source)) {
            const fds_iemgr_elem *elem = std::get<const fds_iemgr_elem *>(column_cfg.source);
            type = type_from_ipfix(elem);
            column.extractor = make_extractor(*elem);

        } else if (std::holds_alternative<const fds_iemgr_alias *>(column_cfg.source)) {
            const fds_iemgr_alias *alias = std::get<const fds_iemgr_alias *>(column_cfg.source);
            type = find_common_type(*alias);
            column.extractor = make_extractor(*alias);

        } else /* if (std::holds_alternative<SpecialField>(column_cfg.source)) */ {
            type = DataType::UInt32;
            column.special = std::get<SpecialField>(column_cfg.source);

        }

        column.name = column_cfg.name;
        column.type = type_to_clickhouse(type, column_cfg.nullable ? DataTypeNullable::Nullable : DataTypeNullable::Nonnullable);

        column.getter = make_getter(type);
        column.column_writer = make_columnwriter(type, column_cfg.nullable ? DataTypeNullable::Nullable : DataTypeNullable::Nonnullable);
        column.column_factory = [=]() {
            return make_column(type, column_cfg.nullable ? DataTypeNullable::Nullable : DataTypeNullable::Nonnullable);
        };

        columns.emplace_back(std::move(column));
    }

    return columns;
}

static std::vector<std::pair<std::string, std::string>> describe_table(clickhouse::Client &client, const std::string &table)
{
    std::vector<std::pair<std::string, std::string>> name_and_type;
    try {
        client.Select("DESCRIBE TABLE " + table, [&](const clickhouse::Block &block) {
            if (block.GetColumnCount() > 0 && block.GetRowCount() > 0) {
                // debug_print_block(block);
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

static void ensure_schema(clickhouse::Client &client, const std::string &table, const std::vector<ColumnCtx> &columns)
{
    // Check that the database has the necessary columns
    auto db_columns = describe_table(client, table);
    if (columns.size() != db_columns.size()) {
        throw Error("config has {} columns but table \"{}\" has {}", columns.size(), table, db_columns.size());
    }

    for (size_t i = 0; i < db_columns.size(); i++) {
        const auto &expected_name = columns[i].name;
        const auto &expected_type = columns[i].type;
        const auto &[actual_name, actual_type] = db_columns[i];

        if (expected_name != actual_name) {
            throw Error("expected column #{} in table \"{}\" to be named \"{}\" but it is \"{}\"",
                        i, table, expected_name, actual_name);
        }

        if (expected_type != actual_type) {
            throw Error("expected column #{} in table \"{}\" to be of type \"{}\" but it is \"{}\"",
                        i, table, expected_type, actual_type);
        }
    }
}

Inserter::Inserter(int id,
                   Logger logger,
                   clickhouse::ClientOptions client_opts,
                   const std::vector<ColumnCtx> &columns,
                   const std::string &table,
                   SyncQueue<BlockCtx *> &filled_blocks,
                   SyncStack<BlockCtx *> &empty_blocks)
    : m_id(id)
    , m_logger(logger)
    , m_client_opts(client_opts)
    , m_columns(columns)
    , m_table(table)
    , m_filled_blocks(filled_blocks)
    , m_empty_blocks(empty_blocks)
{
}

void Inserter::start()
{
    m_thread = std::thread([this]() {
        try {
            run();
        } catch (...) {
            m_exception = std::current_exception();
            m_errored = true;
        }
    });
}

void Inserter::insert(clickhouse::Block &block) {
    bool needs_reconnect = false;
    while (!m_stop_signal) {
        try {
            if (needs_reconnect) {
                m_client->ResetConnectionEndpoint();
                ensure_schema(*m_client.get(), m_table, m_columns);
                m_logger.warning("[Worker %d] Connected to %s:%d due to error with previous endpoint", m_id,
                              m_client->GetCurrentEndpoint()->host.c_str(), m_client->GetCurrentEndpoint()->port);
            }
            m_client->Insert(m_table, block);
            break;

        } catch (const std::exception &ex) {
            m_logger.error("[Worker %d] Insert failed: %s - retrying in 1 second", m_id, ex.what());
            needs_reconnect = true;
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void Inserter::run() {
    m_client = std::make_unique<clickhouse::Client>(m_client_opts);
    ensure_schema(*m_client.get(), m_table, m_columns);
    m_logger.info("[Worker %d] Connected to %s:%d", m_id,
                  m_client->GetCurrentEndpoint()->host.c_str(), m_client->GetCurrentEndpoint()->port);

    while (!m_stop_signal) {
        BlockCtx *block = m_filled_blocks.get();
        if (!block) {
            // we might get null as a way to get unblocked and process stop signal
            continue;
        }

        block->block.RefreshRowCount();
        insert(block->block);

        for (auto &column : block->columns) {
            column->Clear();
        }
        block->rows = 0;
        m_empty_blocks.put(block);
    }
}

void Inserter::stop()
{
    m_stop_signal = true;
}

void Inserter::join()
{
    m_thread.join();
}

void Inserter::check_error()
{
    if (m_errored) {
        std::rethrow_exception(m_exception);
    }
}

Plugin::Plugin(ipx_ctx_t *ctx, const char *xml_config)
    : m_logger(ctx)
{
    m_config = parse_config(xml_config, ipx_ctx_iemgr_get(ctx));

    m_columns = prepare_columns(m_config.columns);

    std::vector<clickhouse::Endpoint> endpoints;
    for (const Config::Endpoint &endpoint_cfg : m_config.connection.endpoints) {
        endpoints.push_back(clickhouse::Endpoint{endpoint_cfg.host, endpoint_cfg.port});
    }

    // Prepare blocks
    m_logger.info("Preparing %d blocks", m_config.blocks);
    for (unsigned int i = 0; i < m_config.blocks; i++) {
        m_blocks.emplace_back(std::make_unique<BlockCtx>());
        BlockCtx &block = *m_blocks.back().get();
        for (const auto &column : m_columns) {
            block.columns.emplace_back(column.column_factory());
            block.block.AppendColumn(column.name, block.columns.back());
        }
        m_empty_blocks.put(&block);
    }

    // Prepare inserters
    m_logger.info("Preparing %d inserter threads", m_config.inserter_threads);
    for (unsigned int i = 0; i < m_config.inserter_threads; i++) {
        auto client_opts = clickhouse::ClientOptions()
            .SetEndpoints(endpoints)
            .SetUser(m_config.connection.user)
            .SetPassword(m_config.connection.password)
            .SetDefaultDatabase(m_config.connection.database);

        m_inserters.emplace_back(std::make_unique<Inserter>(
            m_inserters.size() + 1, m_logger, client_opts, m_columns, m_config.connection.table, m_filled_blocks, m_empty_blocks));
    }

    // Start inserter threads
    m_logger.info("Starting inserter threads");
    for (auto &inserter : m_inserters) {
        inserter->start();
    }

    m_logger.info("Clickhouse plugin is ready");
}

bool
Plugin::should_skip_record(fds_drec &rec, uint16_t iter_flags)
{
    // Should we skip the record?
    if (!m_config.biflow_empty_autoignore || (rec.tmplt->flags & FDS_TEMPLATE_BIFLOW) == 0) {
        return false;
    }

    static constexpr uint32_t IANA_EN = 0;
    static constexpr uint16_t IANA_OCTET_DELTA_COUNT_ID = 1;
    static constexpr uint16_t IANA_PACKET_DELTA_COUNT_ID = 2;

    fds_drec_iter iter;
    fds_drec_iter_init(&iter, &rec, iter_flags);
    while (fds_drec_iter_next(&iter) != FDS_EOC) {
        if (iter.field.info->en == IANA_EN &&
                (iter.field.info->id == IANA_OCTET_DELTA_COUNT_ID || iter.field.info->id == IANA_PACKET_DELTA_COUNT_ID)) {
            uint64_t value;
            if (fds_get_uint_be(iter.field.data, iter.field.size, &value) != FDS_OK) {
                throw Error("fds_get_uint_be failed");
            }
            if (value == 0) {
                return true; // Record skipped
            }
        }
    }

    return false;
}

bool
Plugin::process_record(ipx_msg_ipfix_t *msg, fds_drec &rec, uint16_t iter_flags)
{
    if (should_skip_record(rec, iter_flags)) {
        return false;
    }

    // Get values
    for (ColumnCtx &ctx : m_columns) {
        if (ctx.special == SpecialField::ODID) {
            ctx.has_value = true;
            ctx.value_buffer = ipx_msg_ipfix_get_ctx(msg)->odid;
            continue;
        }

        std::optional<fds_drec_field> field = ctx.extractor(rec, iter_flags);
        if (!field) {
            ctx.has_value = false;
            continue;
        }

        ctx.getter(*field, ctx.value_buffer);
        ctx.has_value = true;
    }

    // Store values
    for (size_t i = 0; i < m_columns.size(); i++) {
        m_columns[i].column_writer(
            m_columns[i].has_value ? &m_columns[i].value_buffer : nullptr,
            *m_current_block->columns[i].get());
    }

    return true;
}

void
Plugin::process(ipx_msg_t *msg)
{
    if (ipx_msg_get_type(msg) != IPX_MSG_IPFIX) {
        return;
    }
    ipx_msg_ipfix_t *ipfix_msg = ipx_msg_base2ipfix(msg);

    // Get new empty block if there is no current block
    if (!m_current_block) {
        m_current_block = m_empty_blocks.get();
    }

    // Go through all the records and store them
    uint32_t drec_cnt = ipx_msg_ipfix_get_drec_cnt(ipfix_msg);
    uint32_t rows_count = 0;

    for (uint32_t idx = 0; idx < drec_cnt; idx++) {
        ipx_ipfix_record *rec = ipx_msg_ipfix_get_drec(ipfix_msg, idx);
        if ((rec->rec.tmplt->flags & FDS_TEMPLATE_BIFLOW) && m_config.split_biflow) {
            // In case of biflow if splitBiflow option is set, go through the record twice.
            // Once in forward direction, and the second time in reverse direction.
            if (process_record(ipfix_msg, rec->rec, FDS_DREC_BIFLOW_FWD)) {
                rows_count += 1;
            }
            if (process_record(ipfix_msg, rec->rec, FDS_DREC_BIFLOW_REV)) {
                rows_count += 1;
            }
        } else {
            process_record(ipfix_msg, rec->rec, 0);
            rows_count += 1;
        }
    }
    m_current_block->rows += rows_count;

    // Record current time for updating timers
    std::time_t now = std::time(nullptr);
    if (m_start_time == 0) {
        m_start_time = now;
        m_last_insert_time = now;
        m_last_stats_print_time = now;
    }

    // Send the block for insertion if it is sufficiently full or a block hasn't been sent in a long enough time
    if (m_current_block->rows >= m_config.block_insert_threshold
            || (uint64_t(now - m_last_insert_time) >= m_config.block_insert_max_delay_secs && m_current_block->rows > 0)) {
        m_filled_blocks.put(m_current_block);
        m_current_block = nullptr;
        m_last_insert_time = now;
    }

    // Update stats
    m_rows_written_total += rows_count;
    m_recs_processed_total += drec_cnt;
    m_recs_processed_since_last += drec_cnt;
    if ((now - m_last_stats_print_time) > STATS_PRINT_INTERVAL_SECS) {
        double total_rps = m_recs_processed_total / std::max<double>(1, now - m_start_time);
        double immediate_rps = m_recs_processed_since_last / std::max<double>(1, now - m_last_stats_print_time);
        std::size_t empty_blocks = m_empty_blocks.size();
        std::size_t filled_blocks = m_filled_blocks.size();
        m_logger.info("STATS - RECS: %lu, ROWS: %lu, AVG: %.2f recs/sec, AVG_IMMEDIATE: %.2f recs/sec, EMPTY_BLOCKS: %lu, FILLED_BLOCKS: %lu",
                      m_recs_processed_total,
                      m_rows_written_total,
                      total_rps,
                      immediate_rps,
                      empty_blocks,
                      filled_blocks);
        m_recs_processed_since_last = 0;
        m_last_stats_print_time = now;
    }

    // Check for any exceptions was thrown by the inserter threads
    for (auto &inserter : m_inserters) {
        inserter->check_error();
    }
}

void Plugin::stop()
{
    // Export what's left in the last block
    if (m_current_block && m_current_block->rows > 0) {
        m_filled_blocks.put(m_current_block);
        m_current_block = nullptr;
    }

    // Stop all the threads and wait for them to finish
    m_logger.info("Sending stop signal to inserter threads...");
    for (auto &inserter : m_inserters) {
        inserter->stop();
    }
    for (const auto &inserter : m_inserters) {
        (void) inserter;
        // Wake up the inserter threads in case they are waiting on a .get()
        m_filled_blocks.put(nullptr);
    }

    m_logger.info("Waiting for inserter threads to finish...");
    for (auto &inserter : m_inserters) {
        inserter->join();
    }
}

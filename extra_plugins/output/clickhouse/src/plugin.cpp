/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Main plugin class
 * @date 2025
 *
 * Copyright(c) 2025 CESNET z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "datatype.h"
#include "plugin.h"

#include <cassert>
#include <ipfixcol2.h>
#include <libfds.h>

static std::vector<Column> prepare_columns(std::vector<Config::Column> &columns_cfg)
{
    std::vector<Column> columns;

    for (const auto &column_cfg : columns_cfg) {
        Column column{};
        DataType type;

        if (std::holds_alternative<const fds_iemgr_elem *>(column_cfg.source)) {
            const fds_iemgr_elem *elem = std::get<const fds_iemgr_elem *>(column_cfg.source);
            type = type_from_ipfix(elem->data_type);
            column.elem = elem;

        } else if (std::holds_alternative<const fds_iemgr_alias *>(column_cfg.source)) {
            const fds_iemgr_alias *alias = std::get<const fds_iemgr_alias *>(column_cfg.source);
            type = find_common_type(*alias);
            column.alias = alias;

        } else /* if (std::holds_alternative<SpecialField>(column_cfg.source)) */ {
            type = DataType::UInt32;
            column.special = std::get<SpecialField>(column_cfg.source);

        }

        column.name = column_cfg.name;
        column.datatype = type;
        column.nullable = column_cfg.nullable;

        columns.emplace_back(std::move(column));
    }

    return columns;
}


Plugin::Plugin(ipx_ctx_t *ctx, const char *xml_config)
    : m_logger(ctx)
    , m_stats(m_logger, *this)
{
    // Subscribe to periodic messages aswell to ensure data export even when no data is coming
    ipx_msg_mask_t new_mask = IPX_MSG_IPFIX | IPX_MSG_PERIODIC | IPX_MSG_SESSION;
    int rc = ipx_ctx_subscribe(ctx, &new_mask, nullptr);
    if (rc != IPX_OK) {
        throw Error("ipx_ctx_subscribe() failed with error code {}", rc);
    }

    // Parse config
    m_config = parse_config(xml_config, ipx_ctx_iemgr_get(ctx));

    std::vector<clickhouse::Endpoint> endpoints;
    for (const Config::Endpoint &endpoint_cfg : m_config.connection.endpoints) {
        endpoints.push_back(clickhouse::Endpoint{endpoint_cfg.host, endpoint_cfg.port});
    }

    m_columns = prepare_columns(m_config.columns);
    m_rec_parsers = std::make_unique<RecParserManager>(m_columns, m_config.biflow_empty_autoignore);

    // Prepare blocks
    for (unsigned int i = 0; i < m_config.blocks; i++) {
        std::unique_ptr<Block> blk = std::make_unique<Block>();
        for (const auto &column : m_columns) {
            blk->columns.emplace_back(make_column(column.datatype, column.nullable));
            blk->block.AppendColumn(column.name, blk->columns.back());
        }
        m_blocks.emplace_back(std::move(blk));
        m_avail_blocks.put(m_blocks.back().get());
    }

    // Prepare inserters
    for (unsigned int i = 0; i < m_config.inserter_threads; i++) {
        clickhouse::ClientOptions client_opts = clickhouse::ClientOptions()
            .SetEndpoints(endpoints)
            .SetUser(m_config.connection.user)
            .SetPassword(m_config.connection.password)
            .SetDefaultDatabase(m_config.connection.database);
        std::unique_ptr<Inserter> ins = std::make_unique<Inserter>(
            i,
            m_logger,
            client_opts,
            m_config.connection.table,
            m_columns,
            m_filled_blocks,
            m_avail_blocks);

        m_inserters.emplace_back(std::move(ins));
    }

    m_logger.info("Starting inserters");
    for (auto &ins : m_inserters) {
        ins->start();
    }


    m_logger.info("ClickHouse plugin is ready");
}

void Plugin::extract_values(ipx_msg_ipfix_t *msg, RecParser &parser, Block &block, bool rev)
{
    std::size_t n_columns = m_columns.size();
    bool has_value;
    ValueVariant value;

    for (std::size_t i = 0; i < n_columns; i++) {
        has_value = false;

        if (m_columns[i].special == SpecialField::ODID) {
            value = ipx_msg_ipfix_get_ctx(msg)->odid;
            has_value = true;

        } else {
            fds_drec_field &field = parser.get_column(i, rev);
            if (field.data != nullptr) {
                try {
                    value = get_value(m_columns[i].datatype, field);
                    has_value = true;
                } catch (const ConversionError& err) {
                    m_logger.error("Field conversion failed (field #%d, \"%s\"): %s", i, m_columns[i].name, err.what());
                }
            }
        }

        write_to_column(m_columns[i].datatype, m_columns[i].nullable, *block.columns[i].get(), has_value ? &value : nullptr);
    }
    block.rows++;
}

int
Plugin::process_record(ipx_msg_ipfix_t *msg, fds_drec &rec, Block &block)
{
    if (rec.tmplt->type == FDS_TYPE_TEMPLATE_OPTS) {
        // skip the data record if the template used is a options template
        // currently we only want data records using "normal" templates
        // this is to prevent empty records in the database and might be improved upon in the future
        return 0;
    }

    int ret = 0;
    RecParser &parser = m_rec_parsers->get_parser(rec.tmplt);
    parser.parse_record(rec);

    if (!parser.skip_fwd()) {
        extract_values(msg, parser, block, false);
        ret++;
    }

    if (!parser.skip_rev()) {
        extract_values(msg, parser, block, true);
        ret++;
    }

    return ret;
}

void
Plugin::process_session_msg(ipx_msg_session_t *msg)
{
    if (ipx_msg_session_get_event(msg) == IPX_MSG_SESSION_CLOSE) {
        const ipx_session *sess = ipx_msg_session_get_session(msg);
        m_rec_parsers->delete_session(sess);
    }
}

void
Plugin::process_ipfix_msg(ipx_msg_ipfix_t *msg)
{
    // get new block if we don't have one
    if (m_current_block == nullptr) {
        if (m_config.nonblocking) {
            std::optional<Block *> maybe_block = m_avail_blocks.try_get();
            if (maybe_block.has_value()) {
                m_current_block = maybe_block.value();
            } else {
                // no available blocks and we are in a non-blocking mode, drop the message
                uint32_t drec_cnt = ipx_msg_ipfix_get_drec_cnt(msg);
                m_stats.add_dropped(drec_cnt);
                return;
            }
        } else {
            m_current_block = m_avail_blocks.get();
        }
    }

    // setup rec parser
    const ipx_msg_ctx *msg_ctx = ipx_msg_ipfix_get_ctx(msg);
    if (msg_ctx->session->type == FDS_SESSION_SCTP) {
        throw std::runtime_error("SCTP is not supported at this time");
    }
    m_rec_parsers->select_session(msg_ctx->session);
    m_rec_parsers->select_odid(msg_ctx->odid);

    // go through all the records
    uint32_t drec_cnt = ipx_msg_ipfix_get_drec_cnt(msg);
    uint32_t rows_count = 0;
    for (uint32_t idx = 0; idx < drec_cnt; idx++) {
        ipx_ipfix_record *rec = ipx_msg_ipfix_get_drec(msg, idx);
        uint32_t rows_inserted = process_record(msg, rec->rec, *m_current_block);
        rows_count += rows_inserted;
    }

    m_stats.add_recs(drec_cnt);
    m_stats.add_rows(rows_count);
}

void
Plugin::process(ipx_msg_t *msg)
{
    if (ipx_msg_get_type(msg) == IPX_MSG_SESSION) {
        process_session_msg(ipx_msg_base2session(msg));

    } else if (ipx_msg_get_type(msg) == IPX_MSG_IPFIX) {
        ipx_msg_ipfix_t *ipfix_msg = ipx_msg_base2ipfix(msg);
        process_ipfix_msg(ipfix_msg);
    }

    time_t now = std::time(nullptr);

    // Send the block for insertion if it is sufficiently full or a block hasn't been sent in a long enough time
    if (m_current_block) {
        bool nonempty = m_current_block->rows > 0;
        bool thresh_reached = m_current_block->rows >= m_config.block_insert_threshold;
        bool timeout_reached = uint64_t(now - m_last_insert_time) >= m_config.block_insert_max_delay_secs;

        if (nonempty && (thresh_reached || timeout_reached)) {
            m_filled_blocks.put(m_current_block);
            m_current_block = nullptr;
            m_last_insert_time = now;
        }
    }

    // Print stats
    m_stats.print_stats_throttled(now);

    // Check for any exceptions thrown by workers
    for (auto &ins : m_inserters) {
        ins->check_error();
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
    for (auto &ins : m_inserters) {
        ins->request_stop();
    }
    for (const auto &ins : m_inserters) {
        (void) ins;
        // Wake up the inserter threads in case they are waiting on a .get()
        m_filled_blocks.put(nullptr);
    }

    m_logger.info("Waiting for inserter threads to finish...");
    for (auto &ins : m_inserters) {
        ins->join();
    }

    std::size_t drop_count = 0;
    for (const auto& block : m_blocks) {
        drop_count += block->rows;
    }
    m_logger.warning("%zu rows could not have been inserted and have been dropped due to termination timeout",
                     drop_count);
}

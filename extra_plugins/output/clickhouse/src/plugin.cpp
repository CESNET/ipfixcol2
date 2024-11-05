/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief
 * @date 2024
 *
 * Copyright(c) 2024 CESNET z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "plugin.h"
#include "datatype.h"

#include <ipfixcol2.h>
#include <libfds.h>
#include <clickhouse/client.h>

static constexpr int ERR_TABLE_NOT_FOUND = 60;

using ExtractorFn = std::function<std::optional<fds_drec_field> (fds_drec &drec)>;

static ExtractorFn make_extractor(const fds_iemgr_elem &elem)
{
    return [=](fds_drec &drec) -> std::optional<fds_drec_field> {
        uint32_t pen = elem.scope->pen;
        uint16_t id = elem.id;
        fds_drec_iter iter;
        fds_drec_iter_init(&iter, &drec, 0);
        int ret = fds_drec_iter_find(&iter, pen, id);
        if (ret == FDS_EOC) {
            return {};
        }
        return iter.field;
    };
}

static ExtractorFn make_extractor(const fds_iemgr_alias &alias)
{
    return [=](fds_drec &drec) -> std::optional<fds_drec_field> {
        fds_drec_iter iter;
        for (size_t i = 0; i < alias.sources_cnt; i++) {
            uint32_t pen = alias.sources[i]->scope->pen;
            uint16_t id = alias.sources[i]->id;
            fds_drec_iter_init(&iter, &drec, 0);
            int ret = fds_drec_iter_find(&iter, pen, id);
            if (ret != FDS_EOC) {
                return iter.field;
            }
        }
        return {};
    };
}
static std::vector<FieldCtx> prepare_fields(VectorOfElemOrAlias &fields_cfg)
{
    std::vector<FieldCtx> fields;

    for (const auto &field_cfg : fields_cfg) {
        DataType type;
        ExtractorFn extractor;
        std::string name;

        if (std::holds_alternative<const fds_iemgr_elem *>(field_cfg)) {
            const fds_iemgr_elem *elem = std::get<const fds_iemgr_elem *>(field_cfg);
            name = elem->name;
            type = type_from_ipfix(elem->data_type);
            extractor = make_extractor(*elem);

        } else /* if (std::holds_alternative<const fds_iemgr_alias *>(field_cfg)) */ {
            const fds_iemgr_alias *alias = std::get<const fds_iemgr_alias *>(field_cfg);
            name = alias->name;
            type = find_common_type(*alias);
            extractor = make_extractor(*alias);
        }

        WriterFn writer = make_writer(type);

        FieldCtx field;
        field.name = name;
        field.type = type_to_clickhouse(type);
        field.handler = [=](fds_drec &drec, clickhouse::Column &column) {
            std::optional<fds_drec_field> field = extractor(drec);
            writer(field, column);
        };
        field.column_factory = [=]() {
            return make_column(type);
        };
        fields.emplace_back(std::move(field));
    }

    return fields;
}

void check_table_exists_and_matches_fields(clickhouse::Client &client, const std::string &table, const std::vector<FieldCtx> &expected_table_fields)
{
    std::vector<std::pair<std::string, std::string>> field_name_and_type;
    try {
        client.Select("DESCRIBE TABLE " + table, [&](const clickhouse::Block &block) {
            if (block.GetColumnCount() > 0 && block.GetRowCount() > 0) {
                print_block(block);
                const auto &name = block[0]->As<clickhouse::ColumnString>();
                const auto &type = block[1]->As<clickhouse::ColumnString>();
                for (size_t i = 0; i < block.GetRowCount(); i++) {
                    field_name_and_type.emplace_back(name->At(i), type->At(i));
                }
            }
        });
    } catch (const clickhouse::ServerException &exc) {
        if (exc.GetCode() == ERR_TABLE_NOT_FOUND) {
            throw Error("Table \"{}\" does not exist", table);
        } else {
            throw;
        }
    }

    if (field_name_and_type.size() != expected_table_fields.size()) {
        throw Error("Config has {} fields but table \"{}\" has {}", expected_table_fields.size(), table, field_name_and_type.size());
    }

    for (size_t i = 0; i < expected_table_fields.size(); i++) {
        const auto &expected_name = expected_table_fields[i].name;
        const auto &expected_type = expected_table_fields[i].type;
        const auto &[actual_name, actual_type] = field_name_and_type[i];

        if (expected_name != actual_name) {
            throw Error("Expected field #{} in table \"{}\" to be named \"{}\" but it is \"{}\"", i, table, expected_name, actual_name);
        }
        if (expected_type != actual_type) {
            throw Error("Expected field #{} in table \"{}\" to be of type \"{}\" but it is \"{}\"", i, table, expected_type, actual_type);
        }
    }
}

Inserter::Inserter(std::unique_ptr<clickhouse::Client> client, const std::string &table, SyncQueue<BlockCtx *> &filled_blocks, SyncQueue<BlockCtx *> &empty_blocks)
    : m_client(std::move(client))
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

void Inserter::run() {
    while (!m_stop_signal) {
        BlockCtx *block = m_filled_blocks.get();
        if (!block) {
            continue;
        }

        block->block.RefreshRowCount();
        m_client->Insert(m_table, block->block);

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
    : m_ctx(ctx)
{
    m_config = parse_config(xml_config, ipx_ctx_iemgr_get(ctx));

    m_client = std::make_unique<clickhouse::Client>(
        clickhouse::ClientOptions()
        .SetHost(m_config.host)
        .SetPort(m_config.port)
        .SetUser(m_config.user)
        .SetPassword(m_config.password)
        .SetDefaultDatabase(m_config.database)
    );

    m_fields = prepare_fields(m_config.fields);
    check_table_exists_and_matches_fields(*m_client.get(), m_config.table, m_fields);

    // Prepare blocks
    IPX_CTX_INFO(m_ctx, "Preparing %d blocks", m_config.num_blocks);
    for (unsigned int i = 0; i < m_config.num_blocks; i++) {
        m_blocks.emplace_back(std::make_unique<BlockCtx>());
        BlockCtx &block = *m_blocks.back().get();
        for (const auto &field : m_fields) {
            block.columns.emplace_back(field.column_factory());
            auto &column = block.columns.back();
            block.block.AppendColumn(field.name, column);
        }
        m_empty_blocks.put(&block);
    }

    // Prepare inserters
    IPX_CTX_INFO(m_ctx, "Preparing %d inserter threads", m_config.num_inserter_threads);
    for (unsigned int i = 0; i < m_config.num_inserter_threads; i++) {
        auto client = std::make_unique<clickhouse::Client>(
            clickhouse::ClientOptions()
            .SetHost(m_config.host)
            .SetPort(m_config.port)
            .SetUser(m_config.user)
            .SetPassword(m_config.password)
            .SetDefaultDatabase(m_config.database)
        );

        auto inserter = std::make_unique<Inserter>(std::move(client), m_config.table, m_filled_blocks, m_empty_blocks);
        m_inserters.emplace_back(std::move(inserter));
    }

    // Start inserter threads
    IPX_CTX_INFO(m_ctx, "Starting inserter threads", 0);
    for (auto &inserter : m_inserters) {
        inserter->start();
    }

    IPX_CTX_INFO(m_ctx, "Clickhouse plugin is ready", 0);
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
    for (uint32_t idx = 0; idx < drec_cnt; idx++) {
        ipx_ipfix_record *rec = ipx_msg_ipfix_get_drec(ipfix_msg, idx);
        for (size_t i = 0; i < m_fields.size(); i++) {
            m_fields[i].handler(rec->rec, *m_current_block->columns[i]);
        }
    }
    m_current_block->rows += drec_cnt;

    // Send the block for insertion if it is sufficiently full
    if (m_current_block->rows >= m_config.block_insert_threshold) {
        m_filled_blocks.put(m_current_block);
        m_current_block = nullptr;
    }

    // Update stats
    m_stats.add_records(drec_cnt);
    m_stats.print();

    // Check for any exceptions was thrown by the inserter threads
    for (auto &inserter : m_inserters) {
        inserter->check_error();
    }
}

void Plugin::stop()
{
    IPX_CTX_INFO(m_ctx, "Sending stop signal to inserter threads...");
    for (auto &inserter : m_inserters) {
        inserter->stop();
    }
    for (auto &_ : m_inserters) {
        // Wake up the inserter threads in case they are waiting on a .get()
        m_filled_blocks.put(nullptr);
    }

    IPX_CTX_INFO(m_ctx, "Waiting for inserter threads to finish...");
    for (auto &inserter : m_inserters) {
        inserter->join();
    }

    IPX_CTX_INFO(m_ctx, "All inserter threads stopped");
}

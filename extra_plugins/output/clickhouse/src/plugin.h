/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief
 * @date 2024
 *
 * Copyright(c) 2024 CESNET z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "common.h"
#include "config.h"

#include <ipfixcol2.h>
#include <clickhouse/client.h>
#include <memory>
#include <thread>

using ColumnFactoryFn = std::function<std::shared_ptr<clickhouse::Column>()>;
using HandlerFn = std::function<void(fds_drec &drec, clickhouse::Column &column)>;

struct FieldCtx {
    std::string name;
    std::string type;
    ColumnFactoryFn column_factory;
    HandlerFn handler;
};

struct BlockCtx {
    std::vector<std::shared_ptr<clickhouse::Column>> columns;
    clickhouse::Block block;
    unsigned int rows;
};

class Inserter : Nonmoveable, Noncopyable {
public:
    Inserter(std::unique_ptr<clickhouse::Client> client, const std::string &table, SyncQueue<BlockCtx *> &filled_blocks, SyncQueue<BlockCtx *> &empty_blocks);

    void start();

    void stop();

    void join();

    void check_error();

private:
    std::thread m_thread;
    std::atomic_bool m_stop_signal = false;
    std::atomic_bool m_errored = false;
    std::exception_ptr m_exception = nullptr;

    std::unique_ptr<clickhouse::Client> m_client;
    const std::string &m_table;
    SyncQueue<BlockCtx *> &m_filled_blocks;
    SyncQueue<BlockCtx *> &m_empty_blocks;

    void run();
};

class Plugin : Nonmoveable, Noncopyable {
public:
    Plugin(ipx_ctx_t *ctx, const char *xml_config);

    void process(ipx_msg_t *msg);

    void stop();

private:
    ipx_ctx_t *m_ctx;
    Config m_config;
    std::unique_ptr<clickhouse::Client> m_client;
    std::vector<FieldCtx> m_fields;
    Stats m_stats;
    uint64_t m_recs_processed = 0;

    BlockCtx *m_current_block = nullptr;
    std::vector<std::unique_ptr<Inserter>> m_inserters;
    std::vector<std::unique_ptr<BlockCtx>> m_blocks;
    SyncQueue<BlockCtx *> m_empty_blocks;
    SyncQueue<BlockCtx *> m_filled_blocks;
};

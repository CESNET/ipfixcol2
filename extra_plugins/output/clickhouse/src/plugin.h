/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Main plugin class
 * @date 2024
 *
 * Copyright(c) 2024 CESNET z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "clickhouse.h"
#include "common.h"
#include "config.h"
#include "datatype.h"
#include "syncqueue.h"
#include "syncstack.h"

#include <ipfixcol2.h>
#include <memory>
#include <thread>

using ExtractorFn = std::function<std::optional<fds_drec_field> (fds_drec &drec, uint16_t iter_flags)>;
using ColumnFactoryFn = std::function<std::shared_ptr<clickhouse::Column>()>;

/**
 * @class FieldInfo
 * @brief Information about a field
 */
struct ColumnCtx {
    std::string name;
    std::string type;
    SpecialField special = SpecialField::NONE;

    ColumnFactoryFn column_factory = nullptr;
    ExtractorFn extractor = nullptr;
    GetterFn getter = nullptr;
    ColumnWriterFn column_writer = nullptr;

    bool has_value = false;
    ValueVariant value_buffer;
};

/**
 * @class BlockCtx
 * @brief Context of a block - a collection of columns
 */
struct BlockCtx {
    std::vector<std::shared_ptr<clickhouse::Column>> columns;
    clickhouse::Block block;
    unsigned int rows;
};

/**
 * @class Inserter
 * @brief Class representing an inserter thread
 */
class Inserter : Nonmoveable, Noncopyable {
public:
    /**
     * @brief Instantiate an inserter instance
     *
     * @param logger The logger
     * @param client_opts The Clickhouse client options
     * @param columns The column definitions
     * @param table The table to insert the data into
     * @param filled_blocks A queue of blocks ready to be sent
     * @param empty_blocks A queue of blocks that have been sent and are able to be reused
     */
    Inserter(int id,
             Logger logger,
             clickhouse::ClientOptions client_opts,
             const std::vector<ColumnCtx> &columns,
             const std::string &table,
             SyncQueue<BlockCtx *> &filled_blocks,
             SyncStack<BlockCtx *> &empty_blocks);

    /**
     * @brief Start the inserter thread
     */
    void start();

    /**
     * @brief Stop the inserter thread
     */
    void stop();

    /**
     * @brief Wait for the inserter thread to stop
     */
    void join();

    /**
     * @brief Check if the inserter thread has encountered an error, and if so, rethrow the captured exception
     */
    void check_error();

private:
    int m_id;
    Logger m_logger;
    std::thread m_thread;
    std::atomic_bool m_stop_signal = false;
    std::atomic_bool m_errored = false;
    std::exception_ptr m_exception = nullptr;

    clickhouse::ClientOptions m_client_opts;
    const std::vector<ColumnCtx> &m_columns;
    const std::string &m_table;
    SyncQueue<BlockCtx *> &m_filled_blocks;
    SyncStack<BlockCtx *> &m_empty_blocks;

    std::unique_ptr<clickhouse::Client> m_client;

    void connect();
    void run();
    void insert(clickhouse::Block &block);
};

/**
 * @class Plugin
 * @brief Class representing the Clickhouse output plugin
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
    Logger m_logger;
    Config m_config;
    std::vector<ColumnCtx> m_columns;

    BlockCtx *m_current_block = nullptr;
    std::vector<std::unique_ptr<Inserter>> m_inserters;
    std::vector<std::unique_ptr<BlockCtx>> m_blocks;
    SyncStack<BlockCtx *> m_empty_blocks;
    SyncQueue<BlockCtx *> m_filled_blocks;

    uint64_t m_rows_written_total = 0;
    uint64_t m_recs_processed_total = 0;
    uint64_t m_recs_processed_since_last = 0;
    std::time_t m_start_time = 0;
    std::time_t m_last_stats_print_time = 0;
    std::time_t m_last_insert_time = 0;

    bool should_skip_record(fds_drec &rec, uint16_t iter_flags);
    bool process_record(ipx_msg_ipfix_t *msg, fds_drec &rec, uint16_t iter_flags);
};

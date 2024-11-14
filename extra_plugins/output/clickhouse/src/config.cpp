/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Configuration parsing and representation
 * @date 2025
 *
 * Copyright(c) 2025 CESNET z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "config.h"

#include <libfds.h>

#include <limits>
#include <optional>
#include <stdexcept>
#include <memory>

namespace args {

enum {
    CONNECTION,
    ENDPOINTS,
    ENDPOINT,
    HOST,
    PORT,
    USER,
    PASSWORD,
    DATABASE,
    TABLE,
    COLUMNS,
    COLUMN,
    NAME,
    SOURCE,
    NULLABLE,
    INSERTER_THREADS,
    BLOCKS,
    BLOCK_INSERT_THRESHOLD,
    BLOCK_INSERT_MAX_DELAY_SECS,
    SPLIT_BIFLOW,
    BIFLOW_EMPTY_AUTOIGNORE,
    NONBLOCKING,
};


static const struct fds_xml_args column[] = {
    FDS_OPTS_ELEM(NAME,     "name",     FDS_OPTS_T_STRING, 0),
    FDS_OPTS_ELEM(SOURCE,   "source",   FDS_OPTS_T_STRING, FDS_OPTS_P_OPT),
    FDS_OPTS_ELEM(NULLABLE, "nullable", FDS_OPTS_T_BOOL,   FDS_OPTS_P_OPT),
    FDS_OPTS_END,
};

static const struct fds_xml_args columns[] = {
    FDS_OPTS_NESTED(COLUMN, "column", column, FDS_OPTS_P_MULTI),
    FDS_OPTS_END,
};

static const struct fds_xml_args endpoint[] = {
    FDS_OPTS_ELEM  (HOST,     "host",     FDS_OPTS_T_STRING, 0),
    FDS_OPTS_ELEM  (PORT,     "port",     FDS_OPTS_T_UINT,   FDS_OPTS_P_OPT),
    FDS_OPTS_END,
};

static const struct fds_xml_args endpoints[] = {
    FDS_OPTS_NESTED(ENDPOINT, "endpoint", endpoint, FDS_OPTS_P_MULTI),
    FDS_OPTS_END,
};

static const struct fds_xml_args connection[] = {
    FDS_OPTS_NESTED(ENDPOINTS, "endpoints", endpoints,         0),
    FDS_OPTS_ELEM  (USER,      "user",      FDS_OPTS_T_STRING, 0),
    FDS_OPTS_ELEM  (PASSWORD,  "password",  FDS_OPTS_T_STRING, 0),
    FDS_OPTS_ELEM  (DATABASE,  "database",  FDS_OPTS_T_STRING, 0),
    FDS_OPTS_ELEM  (TABLE,     "table",     FDS_OPTS_T_STRING, 0),
    FDS_OPTS_END,
};

static const struct fds_xml_args root[] = {
    FDS_OPTS_ROOT  ("params"),
    FDS_OPTS_NESTED(CONNECTION,                  "connection",              connection,        0),
    FDS_OPTS_ELEM  (INSERTER_THREADS,            "inserterThreads",         FDS_OPTS_T_UINT,   FDS_OPTS_P_OPT),
    FDS_OPTS_ELEM  (BLOCKS,                      "blocks",                  FDS_OPTS_T_UINT,   FDS_OPTS_P_OPT),
    FDS_OPTS_ELEM  (BLOCK_INSERT_THRESHOLD,      "blockInsertThreshold",    FDS_OPTS_T_UINT,   FDS_OPTS_P_OPT),
    FDS_OPTS_ELEM  (BLOCK_INSERT_MAX_DELAY_SECS, "blockInsertMaxDelaySecs", FDS_OPTS_T_UINT,   FDS_OPTS_P_OPT),
    FDS_OPTS_ELEM  (SPLIT_BIFLOW,                "splitBiflow",             FDS_OPTS_T_BOOL,   FDS_OPTS_P_OPT),
    FDS_OPTS_ELEM  (BIFLOW_EMPTY_AUTOIGNORE,     "biflowEmptyAutoignore",   FDS_OPTS_T_BOOL,   FDS_OPTS_P_OPT),
    FDS_OPTS_ELEM  (NONBLOCKING,                 "nonblocking",             FDS_OPTS_T_BOOL,   FDS_OPTS_P_OPT),
    FDS_OPTS_NESTED(COLUMNS,                     "columns",                 columns,           0),
    FDS_OPTS_END,
};

}

static std::optional<SpecialField> parse_special_field(const std::string &name)
{
    if (name == "odid") {
        return {SpecialField::ODID};
    }
    return {};
}

static Config::Column parse_column(fds_xml_ctx_t *column_ctx, const fds_iemgr_t *iemgr)
{
    const fds_xml_cont *content;
    Config::Column column;
    std::string source;

    while (fds_xml_next(column_ctx, &content) == FDS_OK) {
        if (content->id == args::NAME) {
            column.name = content->ptr_string;
        } else if (content->id == args::NULLABLE) {
            column.nullable = content->val_bool;
        } else if (content->id == args::SOURCE) {
            source = content->ptr_string;
        }
    }

    if (source.empty()) {
        source = column.name;
    }

    const fds_iemgr_elem *elem = fds_iemgr_elem_find_name(iemgr, source.c_str());
    const fds_iemgr_alias *alias = fds_iemgr_alias_find(iemgr, source.c_str());
    std::optional<SpecialField> special = parse_special_field(source);
    if (!special && !elem && !alias) {
        throw std::runtime_error("IPFIX element with name \"" + source + "\" not found");
    } else if (special) {
        column.source = *special;
    } else if (alias) {
        column.source = alias;
    } else if (elem) {
        column.source = elem;
    }

    return column;
}

static std::vector<Config::Column> parse_columns(fds_xml_ctx_t *columns_ctx, const fds_iemgr_t *iemgr)
{
    const fds_xml_cont *content;
    std::vector<Config::Column> columns;

    while (fds_xml_next(columns_ctx, &content) == FDS_OK) {
        columns.push_back(parse_column(content->ptr_ctx, iemgr));
    }

    return columns;
}

static Config::Endpoint parse_endpoint(fds_xml_ctx_t *endpoint_ctx)
{
    Config::Endpoint endpoint;
    const fds_xml_cont *content;

    while (fds_xml_next(endpoint_ctx, &content) == FDS_OK) {
        if (content->id == args::HOST) {
            endpoint.host = content->ptr_string;

        } else if (content->id == args::PORT) {
            if (content->val_uint > std::numeric_limits<uint16_t>::max()) {
                throw std::runtime_error(std::to_string(content->val_uint) + " is not a valid port number");
            }
            endpoint.port = content->val_uint;

        }
    }

    return endpoint;
}

static std::vector<Config::Endpoint> parse_endpoints(fds_xml_ctx_t *endpoints_ctx)
{
    std::vector<Config::Endpoint> endpoints;
    const fds_xml_cont *content;

    while (fds_xml_next(endpoints_ctx, &content) == FDS_OK) {
        if (content->id == args::ENDPOINT) {
            endpoints.push_back(parse_endpoint(content->ptr_ctx));
        }
    }

    return endpoints;
}

static Config::Connection parse_connection(fds_xml_ctx_t *connection_ctx)
{
    Config::Connection connection;
    const fds_xml_cont *content;

    while (fds_xml_next(connection_ctx, &content) == FDS_OK) {
        if (content->id == args::USER) {
            connection.user = content->ptr_string;

        } else if (content->id == args::PASSWORD) {
            connection.password = content->ptr_string;

        } else if (content->id == args::DATABASE) {
            connection.database = content->ptr_string;

        } else if (content->id == args::TABLE) {
            connection.table = content->ptr_string;

        } else if (content->id == args::ENDPOINTS) {
            connection.endpoints = parse_endpoints(content->ptr_ctx);
        }
    }

    return connection;
}

static void parse_root(fds_xml_ctx_t *root_ctx, const fds_iemgr_t *iemgr, Config &config)
{
    const fds_xml_cont *content;

    while (fds_xml_next(root_ctx, &content) == FDS_OK) {
        if (content->id == args::CONNECTION) {
            config.connection = parse_connection(content->ptr_ctx);

        } else if (content->id == args::COLUMNS) {
            config.columns = parse_columns(content->ptr_ctx, iemgr);

        } else if (content->id == args::BLOCKS) {
            config.blocks = content->val_uint;

        } else if (content->id == args::INSERTER_THREADS) {
            config.inserter_threads = content->val_uint;

        } else if (content->id == args::BLOCK_INSERT_THRESHOLD) {
            config.block_insert_threshold = content->val_uint;

        } else if (content->id == args::BLOCK_INSERT_MAX_DELAY_SECS) {
            config.block_insert_max_delay_secs = content->val_uint;

        } else if (content->id == args::SPLIT_BIFLOW) {
            config.split_biflow = content->val_bool;

        } else if (content->id == args::BIFLOW_EMPTY_AUTOIGNORE) {
            config.biflow_empty_autoignore = content->val_bool;

        } else if (content->id == args::NONBLOCKING) {
            config.nonblocking = content->val_bool;

        }
    }
}

Config parse_config(const char *xml_string, const fds_iemgr_t *iemgr)
{
    Config config{};

    std::unique_ptr<fds_xml_t, decltype(&fds_xml_destroy)> parser(fds_xml_create(), &fds_xml_destroy);
    if (!parser) {
        throw std::runtime_error("Failed to create an XML parser!");
    }

    if (fds_xml_set_args(parser.get(), args::root) != FDS_OK) {
        std::string err = fds_xml_last_err(parser.get());
        throw std::runtime_error("Failed to parse the description of an XML document: " + err);
    }

    fds_xml_ctx_t *root_ctx = fds_xml_parse_mem(parser.get(), xml_string, true);
    if (!root_ctx) {
        std::string err = fds_xml_last_err(parser.get());
        throw std::runtime_error("Failed to parse the configuration: " + err);
    }

    parse_root(root_ctx, iemgr, config);

    return config;
}

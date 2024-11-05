/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief
 * @date 2024
 *
 * Copyright(c) 2024 CESNET z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "config.h"

#include <limits>
#include <stdexcept>
#include <memory>

namespace args {

enum {
    HOST,
    PORT,
    USER,
    PASSWORD,
    DATABASE,
    TABLE,
    FIELDS,
    FIELD,
};


static const struct fds_xml_args fields[] = {
    FDS_OPTS_ELEM(FIELD, "field", FDS_OPTS_T_STRING, FDS_OPTS_P_MULTI),
    FDS_OPTS_END,
};

static const struct fds_xml_args root[] = {
    FDS_OPTS_ROOT  ("params"),
    FDS_OPTS_ELEM  (HOST,     "host",       FDS_OPTS_T_STRING, 0),
    FDS_OPTS_ELEM  (PORT,     "port",       FDS_OPTS_T_UINT,   0),
    FDS_OPTS_ELEM  (USER,     "user",       FDS_OPTS_T_STRING, 0),
    FDS_OPTS_ELEM  (PASSWORD, "password",   FDS_OPTS_T_STRING, 0),
    FDS_OPTS_ELEM  (DATABASE, "database",   FDS_OPTS_T_STRING, 0),
    FDS_OPTS_ELEM  (TABLE,    "table",      FDS_OPTS_T_STRING, 0),
    FDS_OPTS_NESTED(FIELDS,   "fields",     fields,            0),
    FDS_OPTS_END,
};

}

static void parse_fields(fds_xml_ctx_t *fields_ctx, const fds_iemgr_t *iemgr, Config &config)
{
    const fds_xml_cont *content;

    while (fds_xml_next(fields_ctx, &content) == FDS_OK) {
        const fds_iemgr_elem *elem = fds_iemgr_elem_find_name(iemgr, content->ptr_string);
        const fds_iemgr_alias *alias = fds_iemgr_alias_find(iemgr, content->ptr_string);
        if (!elem && !alias) {
            throw std::runtime_error("IPFIX element with name \"" + std::string(content->ptr_string) + "\" not found");
        } else if (elem) {
            config.fields.emplace_back(elem);
        } else if (alias) {
            config.fields.emplace_back(alias);
        }
    }
}

static void parse_root(fds_xml_ctx_t *root_ctx, const fds_iemgr_t *iemgr, Config &config)
{
    const fds_xml_cont *content;

    while (fds_xml_next(root_ctx, &content) == FDS_OK) {
        if (content->id == args::USER) {
            config.user = content->ptr_string;

        } else if (content->id == args::PASSWORD) {
            config.password = content->ptr_string;

        } else if (content->id == args::HOST) {
            config.host = content->ptr_string;

        } else if (content->id == args::PORT) {
            if (content->val_uint > std::numeric_limits<uint16_t>::max()) {
                throw std::runtime_error(std::to_string(content->val_uint) + " is not a valid port number");
            }
            config.port = content->val_uint;

        } else if (content->id == args::DATABASE) {
            config.database = content->ptr_string;

        } else if (content->id == args::TABLE) {
            config.table = content->ptr_string;

        } else if (content->id == args::FIELDS) {
            parse_fields(content->ptr_ctx, iemgr, config);
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

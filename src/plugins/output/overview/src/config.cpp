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

#include <memory>
#include <stdexcept>
#include <string>

namespace args {

enum {
    SKIP_OPTIONS_TEMPLATES
};

static const struct fds_xml_args root[] = {
    FDS_OPTS_ROOT  ("params"),
    FDS_OPTS_ELEM  (SKIP_OPTIONS_TEMPLATES, "skipOptionsTemplates", FDS_OPTS_T_BOOL, FDS_OPTS_P_OPT),
    FDS_OPTS_END,
};

}

static void parse_root(fds_xml_ctx_t *root_ctx, Config &config)
{
    const fds_xml_cont *content;

    while (fds_xml_next(root_ctx, &content) == FDS_OK) {
        if (content->id == args::SKIP_OPTIONS_TEMPLATES) {
            config.skip_option_templates = content->val_bool;
        }
    }
}

Config parse_config(const char *xml_string)
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

    parse_root(root_ctx, config);

    return config;
}

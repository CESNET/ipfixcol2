/**
 * \file src/plugins/output/ipfix/src/Config.cpp
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * \brief Config for IPFIX output plugin
 * \date 2019
 */

/* Copyright (C) 2019 CESNET, z.s.p.o.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of the Company nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * ALTERNATIVELY, provided that this notice is retained in full, this
 * product may be distributed under the terms of the GNU General Public
 * License (GPL) version 2 or later, in which case the provisions
 * of the GPL apply INSTEAD OF those given above.
 *
 * This software is provided ``as is'', and any express or implied
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose are disclaimed.
 * In no event shall the company or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even
 * if advised of the possibility of such damage.
 *
 */

#include "Config.hpp"

#include <stdexcept>
#include <memory>

/// XML nodes
enum params_xml_nodes {
    PARAM_FILENAME,
    PARAM_USE_LOCALTIME,
    PARAM_WINDOW_SIZE,
    PARAM_ALIGN_WINDOWS,
    PARAM_PRESERVE_ORIGINAL,
    PARAM_SPLIT_ON_EXPORT_TIME
};

/// Description of XML document
static const struct fds_xml_args args_params[] = {
    FDS_OPTS_ROOT("params"),
    FDS_OPTS_ELEM(PARAM_FILENAME,      "filename",     FDS_OPTS_T_STRING, 0),
    FDS_OPTS_ELEM(PARAM_USE_LOCALTIME, "useLocalTime", FDS_OPTS_T_BOOL, FDS_OPTS_P_OPT),
    FDS_OPTS_ELEM(PARAM_WINDOW_SIZE,   "windowSize",   FDS_OPTS_T_UINT, FDS_OPTS_P_OPT),
    FDS_OPTS_ELEM(PARAM_ALIGN_WINDOWS, "alignWindows", FDS_OPTS_T_BOOL, FDS_OPTS_P_OPT),
    FDS_OPTS_ELEM(PARAM_PRESERVE_ORIGINAL,    "preserveOriginal",   FDS_OPTS_T_BOOL, FDS_OPTS_P_OPT),
    FDS_OPTS_ELEM(PARAM_SPLIT_ON_EXPORT_TIME, "rotateOnExportTime", FDS_OPTS_T_BOOL, FDS_OPTS_P_OPT),
    FDS_OPTS_END
};

void Config::set_defaults() {
    filename = "";
    use_localtime = false;
    window_size = 0;
    align_windows = true;
    preserve_original = false;
    split_on_export_time = false;
}

void Config::parse_params(fds_xml_ctx_t *params)
{
    const struct fds_xml_cont *content;
    while (fds_xml_next(params, &content) != FDS_EOC) {
        switch (content->id) {
        case PARAM_FILENAME:
            assert(content->type == FDS_OPTS_T_STRING);
            filename = std::string(content->ptr_string);
            break;
        case PARAM_USE_LOCALTIME:
            assert(content->type == FDS_OPTS_T_BOOL);
            use_localtime = content->val_bool;
            break;
        case PARAM_WINDOW_SIZE:
            assert(content->type == FDS_OPTS_T_UINT);
            window_size = content->val_uint;
            break;
        case PARAM_ALIGN_WINDOWS:
            assert(content->type == FDS_OPTS_T_BOOL);
            align_windows = content->val_bool;
            break;
        case PARAM_PRESERVE_ORIGINAL:
            assert(content->type == FDS_OPTS_T_BOOL);
            preserve_original = content->val_bool;
            break;
        case PARAM_SPLIT_ON_EXPORT_TIME:
            assert(content->type == FDS_OPTS_T_BOOL);
            split_on_export_time = content->val_bool;
            break;
        default:
            throw std::invalid_argument("Unexpected element within <params>!");
        }
    }
}

void
Config::check_validity()
{
    if (filename.empty()) {
        throw std::invalid_argument("Filename cannot be empty!");
    }
}

Config::Config(const char *params)
{
    // Set default values
    set_defaults();

    // Create XML parser
    std::unique_ptr<fds_xml_t, decltype(&fds_xml_destroy)> xml(fds_xml_create(), &fds_xml_destroy);
    if (!xml) {
        throw std::runtime_error("Failed to create an XML parser!");
    }

    if (fds_xml_set_args(xml.get(), args_params) != FDS_OK) {
        std::string err = fds_xml_last_err(xml.get());
        throw std::runtime_error("Failed to parse the description of an XML document: " + err);
    }

    fds_xml_ctx_t *params_ctx = fds_xml_parse_mem(xml.get(), params, true);
    if (!params_ctx) {
        std::string err = fds_xml_last_err(xml.get());
        throw std::runtime_error("Failed to parse the configuration: " + err);
    }

    // Parse parameters and check configuration
    try {
        parse_params(params_ctx);
        check_validity();
    } catch (std::exception &ex) {
        throw std::runtime_error("Failed to parse the configuration: " + std::string(ex.what()));
    }
}

Config::~Config()
{
    // Nothing to do
}

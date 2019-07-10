/**
 * \file src/plugins/output/fds/src/Config.cpp
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Parser of XML configuration (source file)
 * \date 2019
 *
 * Copyright(c) 2019 CESNET z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "Config.hpp"
#include <memory>
#include <stdexcept>

/*
 * <params>
 *   <storagePath>...</storagePath>
 *   <compression>...</compression>       <!-- optional -->
 *   <dumpInterval>                       <!-- optional -->
 *     <timeWindow>...</timeWindow>       <!-- optional -->
 *     <align>...</align>                 <!-- optional -->
 *   </dumpInterval>
 *   <asyncIO>...</asyncIO>               <!-- optional -->
 * </params>
 */

/// XML nodes
enum params_xml_nodes {
    NODE_STORAGE = 1,
    NODE_COMPRESS,
    NODE_DUMP,
    NODE_ASYNCIO,

    DUMP_WINDOW,
    DUMP_ALIGN
};

/// Definition of the \<dumpInterval\> node
static const struct fds_xml_args args_dump[] = {
    FDS_OPTS_ELEM(DUMP_WINDOW,  "timeWindow",          FDS_OPTS_T_UINT, FDS_OPTS_P_OPT),
    FDS_OPTS_ELEM(DUMP_ALIGN,   "align",               FDS_OPTS_T_BOOL, FDS_OPTS_P_OPT),
    FDS_OPTS_END
};

/// Definition of the \<params\> node
static const struct fds_xml_args args_params[] = {
    FDS_OPTS_ROOT("params"),
    FDS_OPTS_ELEM(NODE_STORAGE,  "storagePath",        FDS_OPTS_T_STRING, 0),
    FDS_OPTS_ELEM(NODE_COMPRESS, "compression",        FDS_OPTS_T_STRING, FDS_OPTS_P_OPT),
    FDS_OPTS_NESTED(NODE_DUMP,   "dumpInterval",       args_dump,         FDS_OPTS_P_OPT),
    FDS_OPTS_ELEM(NODE_ASYNCIO,  "asyncIO",            FDS_OPTS_T_BOOL,   FDS_OPTS_P_OPT),
    FDS_OPTS_END
};

Config::Config(const char *params)
{
    set_default();

    // Create XML parser
    std::unique_ptr<fds_xml_t, decltype(&fds_xml_destroy)> xml(fds_xml_create(), &fds_xml_destroy);
    if (!xml) {
        throw std::runtime_error("Failed to create an XML parser!");
    }

    if (fds_xml_set_args(xml.get(), args_params) != FDS_OK) {
        throw std::runtime_error("Failed to parse the description of an XML document!");
    }

    fds_xml_ctx_t *params_ctx = fds_xml_parse_mem(xml.get(), params, true);
    if (!params_ctx) {
        std::string err = fds_xml_last_err(xml.get());
        throw std::runtime_error("Failed to parse the configuration: " + err);
    }

    // Parse parameters and check configuration
    try {
        parse_root(params_ctx);
        validate();
    } catch (std::exception &ex) {
        throw std::runtime_error("Failed to parse the configuration: " + std::string(ex.what()));
    }
}

/**
 * @brief Set default parameters
 */
void
Config::set_default()
{
    m_path.clear();
    m_calg = calg::NONE;
    m_async = true;

    m_window.align = true;
    m_window.size = WINDOW_SIZE;
}

/**
 * @brief Check if the configuration is valid
 * @throw runtime_error if the configuration breaks some rules
 */
void
Config::validate()
{
    if (m_path.empty()) {
        throw std::runtime_error("Storage path cannot be empty!");
    }

    if (m_window.size == 0) {
        throw std::runtime_error("Window size cannot be zero!");
    }
}

/**
 * @brief Process \<params\> node
 * @param[in] ctx XML context to process
 * @throw runtime_error if the parser fails
 */
void
Config::parse_root(fds_xml_ctx_t *ctx)
{
    const struct fds_xml_cont *content;
    while (fds_xml_next(ctx, &content) != FDS_EOC) {
        switch (content->id) {
        case NODE_STORAGE:
            // Storage path
            assert(content->type == FDS_OPTS_T_STRING);
            m_path = content->ptr_string;
            break;
        case NODE_COMPRESS:
            // Compression method
            assert(content->type == FDS_OPTS_T_STRING);
            if (strcasecmp(content->ptr_string, "none") == 0) {
                m_calg = calg::NONE;
            } else if (strcasecmp(content->ptr_string, "lz4") == 0) {
                m_calg = calg::LZ4;
            } else if (strcasecmp(content->ptr_string, "zstd") == 0) {
                m_calg = calg::ZSTD;
            } else {
                const std::string inv_str = content->ptr_string;
                throw std::runtime_error("Unknown compression algorithm '" + inv_str + "'");
            }
            break;
        case NODE_ASYNCIO:
            // Asynchronous I/O
            assert(content->type == FDS_OPTS_T_BOOL);
            m_async = content->val_bool;
            break;
        case NODE_DUMP:
            // Dump window
            assert(content->type == FDS_OPTS_T_CONTEXT);
            parse_dump(content->ptr_ctx);
            break;
        default:
            // Internal error
            throw std::runtime_error("Unknown XML node");
        }
    }
}

/**
 * @brief Auxiliary function for parsing \<dumpInterval\> options
 * @param[in] ctx XML context to process
 * @throw runtime_error if the parser fails
 */
void
Config::parse_dump(fds_xml_ctx_t *ctx)
{
    const struct fds_xml_cont *content;
    while(fds_xml_next(ctx, &content) != FDS_EOC) {
        switch (content->id) {
        case DUMP_WINDOW:
            // Window size
            assert(content->type == FDS_OPTS_T_UINT);
            if (content->val_uint > UINT32_MAX) {
                throw std::runtime_error("Window size is too long!");
            }
            m_window.size = static_cast<uint32_t>(content->val_uint);
            break;
        case DUMP_ALIGN:
            // Window alignment
            assert(content->type == FDS_OPTS_T_BOOL);
            m_window.align = content->val_bool;
            break;
        default:
            // Internal error
            throw std::runtime_error("Unknown XML node");
        }
    }
}
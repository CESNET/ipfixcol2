/**
 * \file
 * \author Jakub Antonín Štigler <xstigl00@stud.fit.vutbr.cz>
 * \brief ipfixcol2 API for tcp input plugin
 * \date 2024
 *
 * Copyright: (C) 2023 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdexcept> // exception

#include <ipfixcol2.h> // IPX_*, ipx_*

#include "Config.hpp" // Config
#include "Plugin.hpp" // Plugin

IPX_API struct ipx_plugin_info ipx_plugin_info = {
    // Plugin identification name
    "tcp",
    // Brief description of the plugin
    "Input plugins for IPFIX/NetFlow v5/v9 over Transmission Control Protocol.",
    // Plugin type (input plugin)
    IPX_PT_INPUT,
    // Configuration flags (unused)
    0,
    // Plugin version string
    "3.0.0",
    // Minimal IPFIXcol version string
    "2.0.0",
};

using namespace tcp_in;

int ipx_plugin_init(ipx_ctx_t *ctx, const char *params) {
    Plugin *plugin;

    try {
        Config conf(ctx, params);
        plugin = new Plugin(ctx, conf);
    } catch (std::exception &ex) {
        IPX_CTX_ERROR(ctx, "%s", ex.what());
        return IPX_ERR_DENIED;
    }

    ipx_ctx_private_set(ctx, plugin);
    return IPX_OK;
}

int ipx_plugin_get(ipx_ctx_t *ctx, void *cfg) {
    auto plugin = reinterpret_cast<Plugin *>(cfg);

    try {
        plugin->get();
    } catch (std::exception &ex) {
        IPX_CTX_ERROR(ctx, "%s", ex.what());
        return IPX_ERR_DENIED;
    }

    return IPX_OK;
}

void ipx_plugin_session_close(ipx_ctx_t *ctx, void *cfg, const struct ipx_session *session) {
    auto plugin = reinterpret_cast<Plugin *>(cfg);

    try {
        plugin->close_session(session);
    } catch (std::exception &ex) {
        IPX_CTX_ERROR(ctx, "%s", ex.what());
    }
}

void ipx_plugin_destroy(ipx_ctx_t *ctx, void *cfg) {
    (void)ctx;
    delete reinterpret_cast<Plugin *>(cfg);
}

/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Plugin interface implementation
 * @date 2025
 *
 * Copyright(c) 2025 CESNET z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "plugin.h"

#include <ipfixcol2.h>

#include <memory>

/** Plugin description */
IPX_API struct ipx_plugin_info ipx_plugin_info = {
    // Plugin identification name
    "clickhouse",
    // Brief description of plugin
    "Output plugin that stores flow records to ClickHouse database.",
    // Plugin type
    IPX_PT_OUTPUT,
    // Configuration flags (reserved for future use)
    0,
    // Plugin version string (like "1.2.3")
    "1.0.0",
    // Minimal IPFIXcol version string (like "1.2.3")
    "2.8.0"
};

int
ipx_plugin_init(ipx_ctx_t *ctx, const char *xml_config)
{
    std::unique_ptr<Plugin> plugin;
    try {
        plugin = std::make_unique<Plugin>(ctx, xml_config);
    } catch (const std::exception &ex) {
        IPX_CTX_ERROR(ctx, "An unexpected exception has occured: %s", ex.what());
        return IPX_ERR_DENIED;
    } catch (...) {
        IPX_CTX_ERROR(ctx, "An unexpected exception has occured.");
        return IPX_ERR_DENIED;
    }
    ipx_ctx_private_set(ctx, plugin.release());
    return IPX_OK;
}

void
ipx_plugin_destroy(ipx_ctx_t *ctx, void *priv)
{
    (void) ctx;
    Plugin *plugin = reinterpret_cast<Plugin *>(priv);
    try {
        plugin->stop();
    } catch (const std::exception &ex) {
        IPX_CTX_ERROR(ctx, "An unexpected exception has occured: %s", ex.what());
    } catch (...) {
        IPX_CTX_ERROR(ctx, "An unexpected exception has occured.");
    }
    delete plugin;
}

int
ipx_plugin_process(ipx_ctx_t *ctx, void *priv, ipx_msg_t *msg)
{
    Plugin *plugin = reinterpret_cast<Plugin *>(priv);
    try {
        plugin->process(msg);
    } catch (const std::exception &ex) {
        IPX_CTX_ERROR(ctx, "An unexpected exception has occured: %s", ex.what());
        return IPX_ERR_DENIED;
    } catch (...) {
        IPX_CTX_ERROR(ctx, "An unexpected exception has occured.");
        return IPX_ERR_DENIED;
    }

    return IPX_OK;
}

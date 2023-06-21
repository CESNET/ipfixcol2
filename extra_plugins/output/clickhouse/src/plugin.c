/**
 * \file
 * \author Petr Szczurek <xszczu00@stud.fit.vut.cz>
 * \brief ClickHouse output plugin for IPFIXcol2
 *
 * Copyright(c) 2023 CESNET z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <ipfixcol2.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>

#include "config.h"

/** Plugin description */
IPX_API struct ipx_plugin_info ipx_plugin_info = {
    // Plugin type
    .type = IPX_PT_OUTPUT,
    // Plugin identification name
    .name = "clickhouse",
    // Brief description of plugin
    .dsc = "Output plugin that store flow records to ClickHouse database.",
    // Configuration flags (reserved for future use)
    .flags = 0,
    // Plugin version string (like "1.2.3")
    .version = "2.0.0",
    // Minimal IPFIXcol version string (like "1.2.3")
    .ipx_min = "2.3.0"
};

/** Instance */
struct instance {
    /** Parsed configuration of the instance  */
    struct config *config;
};

int
ipx_plugin_init(ipx_ctx_t *ctx, const char *params)
{
    // Create a private data
    struct instance *data = calloc(1, sizeof(*data));
    if (!data) {
        return IPX_ERR_DENIED;
    }

    if ((data->config = config_create(ctx, params)) == NULL) {
        free(data);
        return IPX_ERR_DENIED;
    }

    ipx_ctx_private_set(ctx, data);
    return IPX_OK;
}

void
ipx_plugin_destroy(ipx_ctx_t *ctx, void *cfg)
{
    (void) ctx; // Suppress warnings
    struct instance *data = (struct instance *) cfg;

    config_destroy(data->config);
    free(data);
}

int
ipx_plugin_process(ipx_ctx_t *ctx, void *cfg, ipx_msg_t *msg)
{
    struct instance *data = (struct instance *) cfg;

    // TODO
    (void) ctx;
    (void) data;
    (void) msg;

    return IPX_OK;
}

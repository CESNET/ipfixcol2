/**
 * \file src/input_plugins/dummy/dummy.c
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Example output plugin for IPFIXcol 2
 * \date 2018
 */

/* Copyright (C) 2018 CESNET, z.s.p.o.
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

#include <ipfixcol2.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>

/** Plugin description */
IPX_API struct ipx_plugin_info ipx_plugin_info = {
    // Plugin type
    .type = IPX_PT_OUTPUT,
    // Plugin identification name
    .name = "dummy",
    // Brief description of plugin
    .dsc = "Example output plugin.",
    // Configuration flags (reserved for future use)
    .flags = 0,
    // Plugin version string (like "1.2.3")
    .version = "0.1.0",
    // Minimal IPFIXcol version string (like "1.2.3")
    .ipx_min = "2.0.0"
};

/** Instance */
struct instance_data {
    /** Sleep time between messages           */
    struct timespec sleep_time;
};

int
ipx_plugin_init(ipx_ctx_t *ctx, const char *params)
{
    // Create a private data
    struct instance_data *data = calloc(1, sizeof(data));
    if (!data) {
        return IPX_ERR_DENIED;
    }

    // TODO: Parse configuration
    (void) params;
    data->sleep_time.tv_sec = 0;
    data->sleep_time.tv_nsec = 100000000LL; // 100ms
    ipx_ctx_private_set(ctx, data);

    // Subscribe to receive IPFIX messages and Transport Session events
    uint16_t new_mask = IPX_MSG_IPFIX | IPX_MSG_SESSION;
    ipx_ctx_subscribe(ctx, &new_mask, NULL);
    return IPX_OK;
}

void
ipx_plugin_destroy(ipx_ctx_t *ctx, void *cfg)
{
    (void) ctx; // Suppress warnings

    struct instance_data *data = (struct instance_data *) cfg;
    free(data);
}

int
ipx_plugin_process(ipx_ctx_t *ctx, void *cfg, ipx_msg_t *msg)
{
    struct instance_data *data = (struct instance_data *) cfg;

    int type = ipx_msg_get_type(msg);
    if (type == IPX_MSG_IPFIX) {
        // Process IPFIX message
        ipx_msg_ipfix_t *ipfix_msg = ipx_msg_base2ipfix(msg);
        const struct ipx_msg_ctx *ipfix_ctx = ipx_msg_ipfix_get_ctx(ipfix_msg);
        IPX_CTX_INFO(ctx, "[%" PRIu32 "] Received an IPFIX message", ipfix_ctx->odid);
    }

    if (type == IPX_MSG_SESSION) {
        // Process Transport Session Event
        ipx_msg_session_t *session_msg = ipx_msg_base2session(msg);
        enum ipx_msg_session_event event = ipx_msg_session_get_event(session_msg);
        const struct ipx_session *session = ipx_msg_session_get_session(session_msg);
        const char *status_msg = (event == IPX_MSG_SESSION_OPEN) ? "opened" : "closed";
        IPX_CTX_INFO(ctx, "Transport Session '%s' %s", session->ident, status_msg);
    }

    nanosleep(&data->sleep_time, NULL);
    return IPX_OK;
}

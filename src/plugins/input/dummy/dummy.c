/**
 * \file src/plugins/input/dummy/dummy.c
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Example input plugin for IPFIXcol 2
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
#include "config.h"

/** Plugin description */
IPX_API struct ipx_plugin_info ipx_plugin_info = {
    // Plugin type
    .type = IPX_PT_INPUT,
    // Plugin identification name
    .name = "dummy",
    // Brief description of plugin
    .dsc = "Example plugin that generates messages.",
    // Configuration flags (reserved for future use)
    .flags = 0,
    // Plugin version string (like "1.2.3")
    .version = "2.0.0",
    // Minimal IPFIXcol version string (like "1.2.3")
    .ipx_min = "2.0.0"
};

/** Instance */
struct instance_data {
    /** Parsed configuration of the instance  */
    struct instance_config *config;
    /** Information about the source of flows */
    struct ipx_session     *session;
};

/**
 * \brief Sleep for specific time
 * \param delay Delay specification
 */
static void
dummy_sleep(const struct timespec *delay)
{
    if (delay->tv_sec == 0 && delay->tv_nsec == 0) {
        return;
    }

    nanosleep(delay, NULL);
}

int
ipx_plugin_init(ipx_ctx_t *ctx, const char *params)
{
    // Create a private data
    struct instance_data *data = calloc(1, sizeof(*data));
    if (!data) {
        return IPX_ERR_DENIED;
    }

    // Parse configuration of the instance
    if ((data->config = config_parse(ctx, params)) == NULL) {
        free(data);
        return IPX_ERR_DENIED;
    }

    // Store the instance data
    ipx_ctx_private_set(ctx, data);
    return IPX_OK;
}

void
ipx_plugin_destroy(ipx_ctx_t *ctx, void *cfg)
{
    struct instance_data *data = (struct instance_data *) cfg;

    if (data->session != NULL) {
        // Inform other plugins that the Transport Session is closed
        ipx_msg_session_t *close_event = ipx_msg_session_create(data->session, IPX_MSG_SESSION_CLOSE);
        ipx_ctx_msg_pass(ctx, ipx_msg_session2base(close_event));

        /* The session cannot be freed because other plugin still have access to it.
         * Send it as a garbage message after the Transport Session close event.
         */
        ipx_msg_garbage_cb cb = (ipx_msg_garbage_cb) &ipx_session_destroy;
        ipx_msg_garbage_t *garbage = ipx_msg_garbage_create(data->session, cb);
        ipx_ctx_msg_pass(ctx, ipx_msg_garbage2base(garbage));
    }

    config_destroy(data->config);
    free(data);
}

int
ipx_plugin_get(ipx_ctx_t *ctx, void *cfg)
{
    struct instance_data *data = (struct instance_data *) cfg;

    if (data->session == NULL) {
        // Create a info about a Transport Session (only once!)
        struct ipx_session_net net_cfg;
        net_cfg.l3_proto = AF_INET;
        net_cfg.port_src = 0;
        net_cfg.port_dst = 0;
        if (inet_pton(AF_INET, "127.0.0.1", &net_cfg.addr_src.ipv4) != 1
            || inet_pton(AF_INET, "127.0.0.1", &net_cfg.addr_dst.ipv4) != 1) {
            // inet_pton() failed!
            IPX_CTX_ERROR(ctx, "inet_pton() failed!", '\0');
            return IPX_ERR_DENIED;
        }

        data->session = ipx_session_new_tcp(&net_cfg);
        if (!data->session) {
            IPX_CTX_ERROR(ctx, "ipx_session_new_tcp() failed!", '\0');
            return IPX_ERR_DENIED;
        }

        // Inform other plugins about the new Transport Session
        ipx_msg_session_t *msg = ipx_msg_session_create(data->session, IPX_MSG_SESSION_OPEN);
        ipx_ctx_msg_pass(ctx, ipx_msg_session2base(msg));
    }

    // Define a source of the a new IPFIX message
    struct ipx_msg_ctx msg_ctx = {
        .session = data->session,
        .odid = data->config->odid,
        .stream = 0
    };

    // Create the empty IPFIX message
    struct fds_ipfix_msg_hdr *ipfix_hdr = malloc(sizeof(*ipfix_hdr));
    if (!ipfix_hdr) {
        // Allocation failed, but this is not a fatal error - just skip the message
        IPX_CTX_ERROR(ctx, "Memory allocation failed! (%s:%d)", __FILE__, __LINE__);
        dummy_sleep(&data->config->sleep_time);
        return IPX_OK;
    }

    ipfix_hdr->version = htons(FDS_IPFIX_VERSION);
    ipfix_hdr->length = htons(FDS_IPFIX_MSG_HDR_LEN);
    ipfix_hdr->export_time = htonl(time(NULL));
    ipfix_hdr->seq_num = htonl(0);
    ipfix_hdr->odid = htonl(data->config->odid);

    // Insert the message and info about source into a wrapper
    ipx_msg_ipfix_t *msg2send = ipx_msg_ipfix_create(ctx, &msg_ctx, (uint8_t *) ipfix_hdr,
        FDS_IPFIX_MSG_HDR_LEN);
    if (!msg2send) {
        // Allocation failed, but this is not a fatal error - just skip the message
        IPX_CTX_ERROR(ctx, "Memory allocation failed! (%s:%d)", __FILE__, __LINE__);
        dummy_sleep(&data->config->sleep_time);
        return IPX_OK;
    }

    // Pass the message
    ipx_ctx_msg_pass(ctx, ipx_msg_ipfix2base(msg2send));

    dummy_sleep(&data->config->sleep_time);
    return IPX_OK;
}
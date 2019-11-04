/**
 * \file src/plugins/output/ipfix/IPFIXOutputPlugin.cpp
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * \brief Main file for IPFIX output plugin
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
#include "IPFIXOutput.hpp"

#include <memory>
#include <ipfixcol2.h>

/// Instance of the output plugin
struct IPFIXOutputInstance {
    /// Configuration of the instance
    Config *config;
    /// Output file manager
    IPFIXOutput *ipfix_output;
};

/** Plugin description */
IPX_API struct ipx_plugin_info ipx_plugin_info = {
    // Plugin identification name
    "ipfix",
    // Brief description of plugin
    "IPFIX output plugin",
    // Plugin type
    IPX_PT_OUTPUT,
    // Configuration flags (reserved for future use)
    0,
    // Plugin version string (like "1.2.3")
    "2.0.0",
    // Minimal IPFIXcol version string (like "1.2.3")
    "2.1.0"};

int
ipx_plugin_init(ipx_ctx_t *ctx, const char *params)
{
    ipx_msg_mask_t mask = IPX_MSG_IPFIX | IPX_MSG_SESSION;
    if (ipx_ctx_subscribe(ctx, &mask, nullptr) != IPX_OK) {
        IPX_CTX_ERROR(ctx, "Error subscribing to messages", 0);
        return IPX_ERR_DENIED;
    }

    IPFIXOutputInstance *instance = nullptr;
    try {
        std::unique_ptr<IPFIXOutputInstance> ptr(new IPFIXOutputInstance());
        std::unique_ptr<Config> config(new Config(params));

        std::unique_ptr<IPFIXOutput> ipfix_output(new IPFIXOutput(config.get(), ctx));

        instance = ptr.release();
        instance->config = config.release();
        instance->ipfix_output = ipfix_output.release();
        ipx_ctx_private_set(ctx, instance);

    } catch (std::exception &ex) {
        IPX_CTX_ERROR(ctx, "%s", ex.what());
        return IPX_ERR_DENIED;

    } catch (...) {
        IPX_CTX_ERROR(ctx, "Unexpected exception has occurred!", '\0');
        return IPX_ERR_DENIED;
    }

    ipx_ctx_private_set(ctx, instance);
    return IPX_OK;
}

void
ipx_plugin_destroy(ipx_ctx_t *ctx, void *cfg)
{
    (void)ctx;

    IPFIXOutputInstance *instance = reinterpret_cast<IPFIXOutputInstance *>(cfg);

    delete instance->ipfix_output;
    delete instance->config;
    delete instance;
}

int
ipx_plugin_process(ipx_ctx_t *ctx, void *cfg, ipx_msg_t *msg)
{
    try {
        IPFIXOutputInstance *instance = reinterpret_cast<IPFIXOutputInstance *>(cfg);
        ipx_msg_type msg_type = ipx_msg_get_type(msg);
        if (msg_type == IPX_MSG_SESSION) {
            instance->ipfix_output->on_session_message(ipx_msg_base2session(msg));
        } else if (msg_type == IPX_MSG_IPFIX) {
            instance->ipfix_output->on_ipfix_message(ipx_msg_base2ipfix(msg));
        }

    } catch (std::exception &ex) {
        IPX_CTX_ERROR(ctx, "%s", ex.what());
        return IPX_ERR_DENIED;

    } catch (...) {
        IPX_CTX_ERROR(ctx, "Unexpected exception has occurred!", '\0');
        return IPX_ERR_DENIED;
    }

    return IPX_OK;
}

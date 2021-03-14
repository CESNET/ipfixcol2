/**
 * \file src/plugins/output/forwarder/src/main.cpp
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * \brief Main plugin entry point
 * \date 2021
 */

/* Copyright (C) 2021 CESNET, z.s.p.o.
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

#include "Forwarder.h"
#include "config.h"

#include <ipfixcol2.h>

#include <cstdio>
#include <memory>

/// Plugin definition
IPX_API struct ipx_plugin_info ipx_plugin_info = {
    "forwarder",
    "Forward flow records as IPFIX to one or more subcollectors.",
    IPX_PT_OUTPUT,
    0,
    "1.0.0",
    "2.2.0"
};

int
ipx_plugin_init(ipx_ctx_t *ctx, const char *xml_config)
{
    auto forwarder = std::unique_ptr<Forwarder>(new Forwarder(ctx));
    try {
        parse_and_configure(ctx, xml_config, *forwarder);
    } catch (std::string &error_message) {
        IPX_CTX_ERROR(ctx, "%s", error_message.c_str());
        return IPX_ERR_FORMAT;
    }
    forwarder->start();

    ipx_msg_mask_t mask = IPX_MSG_IPFIX | IPX_MSG_SESSION;
    ipx_ctx_subscribe(ctx, &mask, NULL);
    ipx_ctx_private_set(ctx, forwarder.release());

    return IPX_OK;
}

void
ipx_plugin_destroy(ipx_ctx_t *ctx, void *priv)
{
    (void)ctx;
    Forwarder *forwarder = (Forwarder *)(priv);
    forwarder->stop();
    delete forwarder;
}

int
ipx_plugin_process(ipx_ctx_t *ctx, void *priv, ipx_msg_t *msg)
{
    Forwarder *forwarder = (Forwarder *)(priv);
    try {
        switch (ipx_msg_get_type(msg)) {
        case IPX_MSG_IPFIX:
            forwarder->on_ipfix_message(ipx_msg_base2ipfix(msg));
            break;
        case IPX_MSG_SESSION:
            forwarder->on_session_message(ipx_msg_base2session(msg));
            break;
        default: assert(0);
        }
    } catch (std::string &error_message) {
        IPX_CTX_ERROR(ctx, "%s", error_message.c_str());
        return IPX_ERR_DENIED;
    } catch (std::bad_alloc &ex) {
        IPX_CTX_ERROR(ctx, "Memory error");
        return IPX_ERR_NOMEM;
    }
    return IPX_OK;
}

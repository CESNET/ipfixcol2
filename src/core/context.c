/**
 * \file src/core/context.c
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Plugin context (source file)
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

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdatomic.h>

#include "context.h"
#include "verbose.h"
#include "utils.h"
#include "fpipe.h"
#include "ring.h"

/** Current  */
enum ipx_ctx_permissions {
    /** Permission to pass a message              */
    IPX_CP_MSG_PASS    = (1 << 0),
    /** Permission to subscribe a message         */
    IPX_CP_MSG_SUB     = (1 << 1),
};

/**
 * \brief Context a plugin instance
 */
struct ipx_ctx {
    /** Plugin identification name                                                               */
    char *name;
    /** Plugin type                                                                              */
    enum ipx_plugin_type type;
    /** Permission flags (see #ipx_ctx_permissions)                                              */
    uint32_t permissions;

    struct {
        /** Feedback pipe (only from an IPFIX parser to particular Inputs plugin)                */
        ipx_fpipe_t *feedback;
        /**
         * Previous plugin (i.e. source of messages) - read ONLY
         * \note NULL for input plugins
         */
        ipx_ring_t *prev;
        /**
         * Next plugin (i.e. destination of messages) - write ONLY
         * \note NULL for output plugins
         */
        ipx_ring_t *next;
    } pipeline; /**< Connection to internal communication pipeline                               */

    struct {
        /** Private data of the instance                                                         */
        void *private;
        /** Update data                                                                          */
        void *update;
    } cfg_plugin; /**< Plugin configuration                                                      */

    struct {
        /**
         * Message subscription mask
         * \note The value represents bitwise OR of #ipx_msg_type flags
         */
        uint16_t msg_mask;
        /** Pointer to the current manager of Information Elements (can be NULL)                 */
        const fds_iemgr_t *ie_mgr;
        /** Current size of IPFIX record (with registered extensions)                            */
        size_t rec_size;
        /** Verbosity level of the plugin                                                        */
        uint8_t  vlevel;
    } cfg_system; /**< System configuration                                                      */
};

ipx_ctx_t *
ipx_ctx_create_dummy(const char *name, enum ipx_plugin_type type)
{
    struct ipx_ctx *ctx = calloc(1, sizeof(*ctx));
    if (!ctx) {
        return NULL;
    }

    ctx->name = strdup(name);
    if (!ctx->name) {
        free(ctx);
        return NULL;
    }

    ctx->type = type;
    ctx->cfg_system.vlevel = IPX_VERB_ERROR;
    ctx->cfg_system.rec_size = sizeof(struct ipx_ipfix_record);
    ctx->permissions = IPX_CP_MSG_PASS;
    return ctx;
}

void
ipx_ctx_destroy(ipx_ctx_t *ctx)
{
    free(ctx->name);
    free(ctx);
}

size_t
ipx_ctx_recsize_get(const ipx_ctx_t *ctx)
{
    return ctx->cfg_system.rec_size;
}

const char *
ipx_ctx_name_get(const ipx_ctx_t *ctx)
{
    return ctx->name;
}

enum ipx_verb_level
ipx_ctx_verb_get(const ipx_ctx_t *ctx)
{
    return ctx->cfg_system.vlevel;
}

int
ipx_ctx_subscribe(ipx_ctx_t *ctx, const uint16_t *mask_new, uint16_t *mask_old)
{
    if ((ctx->permissions & IPX_CP_MSG_SUB) == 0) {
        return IPX_ERR_ARG;
    }

    if (mask_old != NULL) {
        *mask_old = ctx->cfg_system.msg_mask;
    }

    if (!mask_new) {
        return IPX_OK;
    }

    // Plugin can receive only IPFIX and Transport Session Messages
    const uint16_t allowed_types = IPX_MSG_IPFIX | IPX_MSG_SESSION;
    if (((*mask_new) & ~allowed_types) != 0) {
        // Mask includes prohibited types
        return IPX_ERR_FORMAT;
    }

    ctx->cfg_system.msg_mask = (*mask_new);
    return IPX_OK;
}

int
ipx_ctx_msg_pass(ipx_ctx_t *ctx, ipx_msg_t *msg)
{
    // Check permissions and arguments
    if (msg == NULL || (ctx->permissions & IPX_CP_MSG_PASS) == 0) {
        return IPX_ERR_ARG;
    }

    if (!ctx->pipeline.next) {
        /* Plugin has permission but the successor is not connected. This can happen only if
         * the destructor is called immediately after initialization without prepared pipeline ->
         * it's safe to destroy message immediately
         */
        ipx_msg_destroy(msg);
        return IPX_OK;
    }

    ipx_ring_push(ctx->pipeline.next, msg);
    return IPX_OK;
}

void
ipx_ctx_private_set(ipx_ctx_t *ctx, void *data)
{
    ctx->cfg_plugin.private = data;
}

void
ipx_ctx_update_set(ipx_ctx_t *ctx, void *data)
{
    ctx->cfg_plugin.update = data;
}

ipx_fpipe_t *
ipx_ctx_fpipe_get(ipx_ctx_t *ctx)
{
    return ctx->pipeline.feedback;
}

const fds_iemgr_t *
ipx_ctx_iemgr_get(ipx_ctx_t *ctx)
{
    return ctx->cfg_system.ie_mgr;
}


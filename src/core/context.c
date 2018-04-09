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
#include <inttypes.h>

#include "context.h"
#include "verbose.h"
#include "utils.h"
#include "fpipe.h"
#include "ring.h"
#include "message_ipfix.h"

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
    /** Instance identification name (usually from startup configuration)                        */
    char *name;
    /** Plugin type (#IPX_PT_INPUT, #IPX_PT_INTERMEDIATE or #IPX_PT_OUTPUT)                      */
    unsigned int type;
    /** Permission flags (see #ipx_ctx_permissions)                                              */
    uint32_t permissions;
    /** Plugin description and callback function                                                 */
    const struct ipx_ctx_callbacks *plugin_cbs;

    struct {
        /**
         * Feedback pipe (connection to Input plugin)
         * \note
         *   For input plugins represents connection from a collector configurator (for injecting
         *   messages on the front of pipeline) and if the plugin supports processing request to
         *   close a Transport Session, its also feedback from IPFIX Message parser.
         * \note
         *   For IPFIX parser plugin represents pipe for passing requests to close misbehaving
         *   Transport Sessions. However, not all input plugins support this feature, therefore,
         *   the pipe is NULL if the feature is not supported.
         */
        ipx_fpipe_t *feedback;
        /**
         * Previous plugin (i.e. source of messages) - read ONLY
         * \note NULL for input plugins
         */
        ipx_ring_t *src;
        /**
         * Next plugin (i.e. destination of messages) - write ONLY
         * \note NULL for output plugins
         */
        ipx_ring_t *dst;
    } pipeline; /**< Connection to internal communication pipeline                               */

    struct {
        /** Private data of the instance                                                         */
        void *private;
        // TODO:  Update data
        // void *update;
    } cfg_plugin; /**< Plugin configuration                                                      */

    struct {
        /**
         * Message types selected for processing by instance
         * \note The value represents bitwise OR of #ipx_msg_type flags
         */
        ipx_msg_mask_t msg_mask_selected;
        /**
         * Message types that can be subscribed
         * \note The value represents bitwise OR of #ipx_msg_type flags
         */
        ipx_msg_mask_t msg_mask_allowed;
        /** Pointer to the current manager of Information Elements (can be NULL)                 */
        const fds_iemgr_t *ie_mgr;
        /** Current size of IPFIX record (with registered extensions)                            */
        size_t rec_size;
        /** Verbosity level of the plugin                                                        */
        uint8_t  vlevel;
    } cfg_system; /**< System configuration                                                      */
};

ipx_ctx_t *
ipx_ctx_create(const char *name, const struct ipx_ctx_callbacks *callbacks)
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

    ctx->type = 0; // Undefined type
    ctx->permissions = IPX_CP_MSG_PASS;
    ctx->plugin_cbs = callbacks;

    ctx->cfg_system.vlevel = ipx_verb_level_get();
    ctx->cfg_system.rec_size = IPX_MSG_IPFIX_BASE_REC_SIZE;
    ctx->cfg_system.msg_mask_selected = IPX_MSG_IPFIX;
    ctx->cfg_system.msg_mask_allowed = IPX_MSG_IPFIX | IPX_MSG_SESSION;
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

void
ipx_ctx_recsize_set(ipx_ctx_t *ctx, size_t size)
{
    /* Size allocated for an IPFIX record reference MUST be able to hold at least a record without
     * any extensions.
     */
    assert(size >= offsetof(struct ipx_ipfix_record, ext));
    ctx->cfg_system.rec_size = size;
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

void
ipx_ctx_verb_set(ipx_ctx_t *ctx, enum ipx_verb_level verb)
{
    ctx->cfg_system.vlevel = verb;
}

int
ipx_ctx_subscribe(ipx_ctx_t *ctx, const ipx_msg_mask_t *mask_new, ipx_msg_mask_t *mask_old)
{
    if ((ctx->permissions & IPX_CP_MSG_SUB) == 0) {
        return IPX_ERR_ARG;
    }

    if (mask_old != NULL) {
        *mask_old = ctx->cfg_system.msg_mask_selected;
    }

    if (!mask_new) {
        return IPX_OK;
    }

    // Plugin can receive only IPFIX and Transport Session Messages
    if (((*mask_new) & ~ctx->cfg_system.msg_mask_allowed) != 0) {
        // Mask includes prohibited types
        return IPX_ERR_FORMAT;
    }

    ctx->cfg_system.msg_mask_selected = (*mask_new);
    return IPX_OK;
}

int
ipx_ctx_msg_pass(ipx_ctx_t *ctx, ipx_msg_t *msg)
{
    // Check permissions and arguments
    if (msg == NULL || (ctx->permissions & IPX_CP_MSG_PASS) == 0) {
        return IPX_ERR_ARG;
    }

    if (!ctx->pipeline.dst) { // TODO: is it really true???
        /* Plugin has permission but the successor is not connected. This can happen only if
         * the destructor is called immediately after initialization without prepared pipeline ->
         * it's safe to destroy message immediately
         */
        ipx_msg_destroy(msg);
        return IPX_OK;
    }

    ipx_ring_push(ctx->pipeline.dst, msg);
    return IPX_OK;
}

void
ipx_ctx_private_set(ipx_ctx_t *ctx, void *data)
{
    ctx->cfg_plugin.private = data;
}

ipx_fpipe_t *
ipx_ctx_fpipe_get(ipx_ctx_t *ctx)
{
    return ctx->pipeline.feedback;
}

void
ipx_ctx_fpipe_set(ipx_ctx_t *ctx, ipx_fpipe_t *pipe)
{
    ctx->pipeline.feedback = pipe;
}

const fds_iemgr_t *
ipx_ctx_iemgr_get(ipx_ctx_t *ctx)
{
    return ctx->cfg_system.ie_mgr;
}

void
ipx_ctx_iemgr_set(ipx_ctx_t *ctx, const fds_iemgr_t *mgr)
{
    // Template manager must be always defined. Event if it is empty...
    assert(mgr != NULL);
    ctx->cfg_system.ie_mgr = mgr;
}

void
ipx_ctx_ring_src_set(ipx_ctx_t *ctx, ipx_ring_t *ring)
{
    ctx->pipeline.src = ring;
}

void
ipx_ctx_ring_dst_set(ipx_ctx_t *ctx, ipx_ring_t *ring)
{
    ctx->pipeline.dst = ring;
}

void
ipx_ctx_overwrite_msg_mask(ipx_ctx_t *ctx, ipx_msg_mask_t mask)
{
    ctx->cfg_system.msg_mask_allowed = mask;
}

// -------------------------------------------------------------------------------------------------

static int
init_check_input(ipx_ctx_t *ctx)
{
    if (ctx->pipeline.feedback == NULL) {
        IPX_CTX_ERROR(ctx, "Input feedback pipe is not defined!");
        return IPX_ERR_ARG;
    }

    if (ctx->pipeline.dst == NULL) {
        IPX_CTX_ERROR(ctx, "Output ring buffer is not defined!");
        return IPX_ERR_ARG;
    }

    if (ctx->plugin_cbs->get == NULL) {
        IPX_CTX_ERROR(ctx, "Getter callback function is not defined!");
        return IPX_ERR_ARG;
    }

    return IPX_OK;
}

static int
init_check_intermediate(ipx_ctx_t *ctx)
{
    if (ctx->pipeline.src == NULL) { // TODO: except output plugin...
        IPX_CTX_ERROR(ctx, "Input ring buffer is not defined!");
        return IPX_ERR_ARG;
    }

    /* Although the output manager is implemented as an intermediate plugin, it doesn't use
     * a standard output ring buffer.
     */
    if (ctx->pipeline.dst == NULL && ctx->plugin_cbs->info->type != IPX_PT_OUTPUT_MGR) {
        IPX_CTX_ERROR(ctx, "Output ring buffer is not defined!");
        return IPX_ERR_ARG;
    }

    if (ctx->plugin_cbs->process) {
        IPX_CTX_ERROR(ctx, "Processing callback function is not defined!");
        return IPX_ERR_ARG;
    }

    return IPX_OK;
}

static int
init_check_output(ipx_ctx_t *ctx)
{
    if (ctx->pipeline.src == NULL) {
        IPX_CTX_ERROR(ctx, "Input ring buffer is not defined!");
        return IPX_ERR_ARG;
    }

    if (ctx->plugin_cbs->process) {
        IPX_CTX_ERROR(ctx, "Processing callback function is not defined!");
        return IPX_ERR_ARG;
    }

    return IPX_OK;
}

int
ipx_ctx_init(ipx_ctx_t *ctx, const char *params)
{
    // Check plugin description
    if (ctx->plugin_cbs == NULL || ctx->plugin_cbs->info == NULL) {
        IPX_CTX_ERROR(ctx, "Plugin information or functions callbacks are undefined!");
        return IPX_ERR_ARG;
    }

    // Check plugin specific requirements
    int rc;
    uint16_t plugin_type = ctx->plugin_cbs->info->type;
    switch (plugin_type) {
    case IPX_PT_INPUT:
        rc = init_check_input(ctx);
        break;
    case IPX_PT_INTERMEDIATE:
    case IPX_PT_OUTPUT_MGR:    // Output manager is implemented as an input plugin
        rc = init_check_intermediate(ctx);
        break;
    case IPX_PT_OUTPUT:
        rc = init_check_output(ctx);
        break;
    default:
        IPX_CTX_ERROR(ctx, "Unexpected plugin type (id %" PRIu16 ") cannot be initialized!",
            plugin_type);
        return IPX_ERR_ARG;
    }

    if (rc != IPX_OK) {
        return rc;
    }

    // Check common requirements
    if (ctx->cfg_system.ie_mgr == NULL) {
        IPX_CTX_ERROR(ctx, "Reference to a manager of Information Elements is not defined!");
        return IPX_ERR_ARG;
    }

    if (ctx->plugin_cbs->init == NULL || ctx->plugin_cbs->destroy == NULL) {
        IPX_CTX_ERROR(ctx, "Plugin instance constructor and/or destructor is not defined!");
        return IPX_ERR_ARG;
    }

    // Try to initialize the plugin
    // Temporarily remove permission to pass messages
    uint32_t permissions_old = ctx->permissions;
    ctx->permissions &= ~(uint32_t) IPX_CP_MSG_PASS;
    rc = ctx->plugin_cbs->init(ctx, params);
    ctx->permissions = permissions_old;

    if (rc != IPX_OK) {
        IPX_CTX_ERROR(ctx, "Initialization function of the instance failed!");
        return IPX_ERR_ARG;
    }

    ctx->type = plugin_type;
    return IPX_OK;
}

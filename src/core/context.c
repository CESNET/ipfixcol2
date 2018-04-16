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
#include <inttypes.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>
#include <sys/prctl.h>

#include "context.h"
#include "verbose.h"
#include "utils.h"
#include "fpipe.h"
#include "ring.h"
#include "message_ipfix.h"

/** Identification of this component (for log) */
const char *comp_str = "Context";

/** List of permissions */
enum ipx_ctx_permissions {
    /** Permission to pass a message              */
    IPX_CP_MSG_PASS    = (1 << 0),
    /** Permission to subscribe a message         */
    IPX_CP_MSG_SUB     = (1 << 1),
};

/** State of context */
enum ipx_ctx_state {
    /** Context was created, but an instances hasn't been initialized yet                        */
    IPX_CS_NEW,
    /** Instance has been successfully initialized, but a thread is not running                  */
    IPX_CS_INIT,
    /** Instance initialized and a thread is running                                             */
    IPX_CS_RUNNING
};

/**
 * \brief Context a plugin instance
 */
struct ipx_ctx {
    /** Instance identification name (usually from startup configuration)                        */
    char *name;
    /** Plugin type (#IPX_PT_INPUT, #IPX_PT_INTERMEDIATE, #IPX_PT_OUTPUT or #IPX_PT_OUTPUT_MGR)  */
    uint16_t type;
    /** Permission flags (see #ipx_ctx_permissions)                                              */
    uint32_t permissions;
    /** Plugin description and callback function                                                 */
    const struct ipx_ctx_callbacks *plugin_cbs;

    /** State of the context                                                                     */
    enum ipx_ctx_state state;
    /** Thread identification (valid only if state == #IPX_CS_RUNNING)                           */
    pthread_t thread_id;

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

    ctx->type = 0;           // Undefined type
    ctx->permissions = 0;    // No permissions
    ctx->plugin_cbs = callbacks;
    ctx->state = IPX_CS_NEW;

    ctx->cfg_system.vlevel = ipx_verb_level_get();
    ctx->cfg_system.rec_size = IPX_MSG_IPFIX_BASE_REC_SIZE;
    ctx->cfg_system.msg_mask_selected = 0; // No messages to process selected
    ctx->cfg_system.msg_mask_allowed = IPX_MSG_IPFIX | IPX_MSG_SESSION;

    if (callbacks == NULL) {
        // Dummy context for testing
        ctx->permissions = IPX_CP_MSG_PASS;
    }

    return ctx;
}

void
ipx_ctx_destroy(ipx_ctx_t *ctx)
{
    if (ctx->state == IPX_CS_RUNNING) {
        // Wait for the thread to terminate
        int rc = pthread_join(ctx->thread_id, NULL);
        if (rc != 0) {
            const char *err_str;
            ipx_strerror(rc, err_str);
            IPX_CTX_WARNING(ctx, "pthread_join() failed: %s", err_str);
        }
        // The instance has been already destroyed by the thread
    }

    if (ctx->state == IPX_CS_INIT) {
        /* Thread is not running but the plugin has been initialized.
         * Because the thread hasn't been started (i.e. no messages has been received or send),
         * prevent the instance to send anything to the output pipeline -> destroy garbage
         * and other message types immediately!
         */
        ipx_ring_t *tmp = ctx->pipeline.dst;
        ctx->pipeline.dst = NULL;

        const char *plugin_name = ctx->plugin_cbs->info->name;
        IPX_CTX_DEBUG(ctx, "Calling instance destructor of the plugin '%s'", plugin_name);

        ctx->plugin_cbs->destroy(ctx, ctx->cfg_plugin.private);
        ctx->pipeline.dst = tmp;
    }

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
        IPX_CTX_DEBUG(ctx, "Called ipx_ctx_subscribe() but doesn't have permissions!");
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
        IPX_CTX_DEBUG(ctx, "Called ipx_ctx_msg_pass() but %s!",
            (!msg) ? "the message is NULL" : "doesn't have permissions");
        return IPX_ERR_ARG;
    }

    if (!ctx->pipeline.dst) {
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


// -------------------------------------------------------------------------------------------------

/**
 * \brief Check specific requirements of an input instance
 * \param[in] ctx Context
 * \return #IPX_OK or #IPX_ERR_ARG
 */
static int
init_check_input(const ipx_ctx_t *ctx)
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

/**
 * \brief Check specific requirements of an intermediate instance
 * \param[in] ctx Context
 * \return #IPX_OK or #IPX_ERR_ARG
 */
static int
init_check_intermediate(const ipx_ctx_t *ctx)
{
    if (ctx->pipeline.src == NULL) {
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

    if (ctx->plugin_cbs->process == NULL) {
        IPX_CTX_ERROR(ctx, "Processing callback function is not defined!");
        return IPX_ERR_ARG;
    }

    return IPX_OK;
}

/**
 * \brief Check specific requirements of an output instance
 * \param[in] ctx Context
 * \return #IPX_OK or #IPX_ERR_ARG
 */
static int
init_check_output(const ipx_ctx_t *ctx)
{
    if (ctx->pipeline.src == NULL) {
        IPX_CTX_ERROR(ctx, "Input ring buffer is not defined!");
        return IPX_ERR_ARG;
    }

    if (ctx->plugin_cbs->process == NULL) {
        IPX_CTX_ERROR(ctx, "Processing callback function is not defined!");
        return IPX_ERR_ARG;
    }

    return IPX_OK;
}

/**
 * \brief Check common (i.e. input/intermediate/output) requirements of an instance
 * \param[in] ctx Context
 * \return #IPX_OK or #IPX_ERR_ARG
 */
static int
init_check_common(const ipx_ctx_t *ctx)
{
    // Check common requirements
    if (ctx->cfg_system.ie_mgr == NULL) {
        IPX_CTX_ERROR(ctx, "Reference to a manager of Information Elements is not defined!");
        return IPX_ERR_ARG;
    }

    if (ctx->plugin_cbs->init == NULL || ctx->plugin_cbs->destroy == NULL) {
        IPX_CTX_ERROR(ctx, "Plugin instance constructor and/or destructor is not defined!");
        return IPX_ERR_ARG;
    }

    const char *plugin_name = ctx->plugin_cbs->info->name;
    if (plugin_name == NULL || strlen(plugin_name) == 0) {
        IPX_CTX_ERROR(ctx, "Name of the plugin is not defined or it is empty!");
        return IPX_ERR_ARG;
    }

    return IPX_OK;
}

/**
 * \brief Set the name of a thread
 * \param[in] ident New identification
 */
static inline void
thread_set_name(const char *ident)
{
    static const size_t size = 16; // i.e. 15 characters + '\0'
    char name[size];
    strncpy(name, ident, size - 1);
    name[size - 1] = '\0';

    int rc = prctl(PR_SET_NAME, name, 0, 0, 0);
    if (rc == -1) {
        const char *err_str;
        ipx_strerror(errno, err_str);
        IPX_WARNING(comp_str, "Failed to set the name of a thread. prctl() failed: %s",
            err_str);
    }
}

/**
 * \brief Get the name of a thread
 * \param[out] ident Current identification
 */
static inline void
thread_get_name(char ident[16])
{
    int rc = prctl(PR_GET_NAME, ident, 0, 0, 0);
    if (rc == -1) {
        const char *err_str;
        ipx_strerror(errno, err_str);
        IPX_WARNING(comp_str, "Failed to set the name of a thread. prctl() failed: %s",
            err_str);
        ident[0] = '\0';
    }
}

int
ipx_ctx_init(ipx_ctx_t *ctx, const char *params)
{
    if (ctx->state != IPX_CS_NEW) {
        IPX_CTX_ERROR(ctx, "Unable to initialize already initialized instance context!");
        return IPX_ERR_ARG;
    }

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
    case IPX_PT_OUTPUT_MGR:    // Output manager is implemented as an intermediate plugin
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
    if ((rc = init_check_common(ctx)) != IPX_OK) {
        return rc;
    }

    // Ok, everything seems fine, set default parameters
    switch (plugin_type) {
    case IPX_PT_INPUT:
        ctx->cfg_system.msg_mask_selected = 0;
        ctx->permissions = IPX_CP_MSG_PASS;
        break;
    case IPX_PT_INTERMEDIATE:
        ctx->cfg_system.msg_mask_selected = IPX_MSG_IPFIX;
        ctx->permissions = IPX_CP_MSG_PASS | IPX_CP_MSG_SUB;
        break;
    case IPX_PT_OUTPUT_MGR:
        /* By default, only ::IPX_MSG_IPFIX (IPFIX Message) and ::IPX_MSG_SESSION (Transport
         * Session Message) types can be passed to plugin instance for processing. However,
         * implementation of the output manager (as an intermediate plugin) requires processing of
         * almost all types of messages.
         */
        ctx->cfg_system.msg_mask_selected = IPX_MSG_MASK_ALL;
        ctx->cfg_system.msg_mask_allowed = IPX_MSG_MASK_ALL; // overwrite
        ctx->permissions = IPX_CP_MSG_SUB;
        break;
    case IPX_PT_OUTPUT:
        ctx->cfg_system.msg_mask_selected = IPX_MSG_IPFIX;
        ctx->permissions = IPX_CP_MSG_SUB;
        break;
    }

    /* Change name of the current thread and block all signals because the instance can create
     * new threads and we want to preserve correct inheritance of these configurations
     */
    char old_ident[16];  // Up to 16 bytes can be stored based on the manual page of prctl
    thread_get_name(old_ident);
    thread_set_name(ctx->name);

    sigset_t set_new, set_old;
    sigfillset(&set_new);
    pthread_sigmask(SIG_SETMASK, &set_new, &set_old);

    // Try to initialize the plugin
    const char *plugin_name = ctx->plugin_cbs->info->name;
    IPX_CTX_DEBUG(ctx, "Calling instance constructor of the plugin '%s'", plugin_name);
    // Temporarily remove permission to pass messages
    uint32_t permissions_old = ctx->permissions;
    ctx->permissions &= ~(uint32_t) IPX_CP_MSG_PASS;
    rc = ctx->plugin_cbs->init(ctx, params);
    ctx->permissions = permissions_old;

    // Restore the previous thread identification and signal mask
    pthread_sigmask(SIG_SETMASK, &set_old, NULL);
    thread_set_name(old_ident);

    if (rc != IPX_OK) {
        IPX_CTX_ERROR(ctx, "Initialization function of the instance failed!");
        // Restore default default parameters
        ctx->permissions = 0;
        ctx->cfg_system.msg_mask_selected = 0;
        ctx->cfg_system.msg_mask_allowed = IPX_MSG_IPFIX | IPX_MSG_SESSION;
        return IPX_ERR_DENIED;
    }

    if (plugin_type != IPX_PT_INPUT && ctx->cfg_system.msg_mask_selected == 0) {
        IPX_CTX_WARNING(ctx, "The instance is not subscribed to receive any kind of message!");
    }

    ctx->type = plugin_type;
    ctx->state = IPX_CS_INIT;
    return IPX_OK;
}

/**
 * \brief Try to receive a request from the feedback pipe and process it
 *
 * \param[in] ctx Instance context
 * \return #IPX_OK on success and instance can continue
 * \return #IPX_ERR_EOF if a request to terminated has been received
 */
static int
thread_input_process_pipe(struct ipx_ctx *ctx)
{
    // Is there a message to process
    ipx_msg_t *msg_ptr = ipx_fpipe_read(ctx->pipeline.feedback);
    if (!msg_ptr) {
        return IPX_OK;
    }

    enum ipx_msg_type msg_type = ipx_msg_get_type(msg_ptr);
    if (msg_type == IPX_MSG_SESSION) {
        // Request to close a Transport Session
        ipx_msg_session_t *session_msg = ipx_msg_base2session(msg_ptr);
        enum ipx_msg_session_event event = ipx_msg_session_get_event(session_msg);
        if (event != IPX_MSG_SESSION_CLOSE) {
            IPX_CTX_ERROR(ctx, "Received a Session message from the feedback pipe with "
                "non-close event type! Ignoring.");
            ipx_msg_session_destroy(session_msg);
            return IPX_OK;
        }

        if (ctx->plugin_cbs->ts_close == NULL) {
            const char *plugin_name = ctx->plugin_cbs->info->name;
            IPX_CTX_ERROR(ctx, "Received a request to close a Transport Session but the "
                "input plugin '%s' doesn't support this feature. Ignoring.", plugin_name);
            ipx_msg_session_destroy(session_msg);
            return IPX_OK;
        }

        IPX_CTX_DEBUG(ctx, "Received a request to close a Transport Session.");
        // Warning: do not access Session properties, they can be already freed!
        const struct ipx_session *session = ipx_msg_session_get_session(session_msg);
        ctx->plugin_cbs->ts_close(ctx, ctx->cfg_plugin.private, session);
        ipx_msg_session_destroy(session_msg);
        return IPX_OK;
    }

    if (msg_type == IPX_MSG_TERMINATE) {
        // Destroy the instance (usually produce garbage messages, etc)
        const char *plugin_name = ctx->plugin_cbs->info->name;
        IPX_CTX_DEBUG(ctx, "Calling instance destructor of the input plugin '%s'", plugin_name);
        ctx->plugin_cbs->destroy(ctx, ctx->cfg_plugin.private);
        // Pass the termination message
        ipx_ring_push(ctx->pipeline.dst, msg_ptr);
        return IPX_ERR_EOF;
    }

    IPX_CTX_ERROR(ctx, "Received unexpected message from the feedback pipe (type %d). "
        "It will be passed on to an IPFIX parser.", msg_type);
    ipx_ring_push(ctx->pipeline.dst, msg_ptr);
    return IPX_OK;
}

/**
 * \brief Input instance control thread
 *
 * Infinite loop that process messages from a feedback pipe, call a plugin getter.
 * \param[in] arg Instance context
 * \return NULL
 */
static void *
thread_input(void *arg)
{
    struct ipx_ctx *ctx = (struct ipx_ctx *) arg;
    assert(ctx->type == IPX_PT_INPUT);
    thread_set_name(ctx->name);

    const char *plugin_name = ctx->plugin_cbs->info->name;
    IPX_CTX_DEBUG(ctx, "Instance thread of the input plugin '%s' has started!", plugin_name);

    bool terminate = false;

    while (!terminate) {
        int rc = thread_input_process_pipe(ctx);
        if (rc == IPX_ERR_EOF) {
            // Received request to destroy the instance
            terminate = true;
            continue;
        }

        // Try to get a new IPFIX message
        ctx->plugin_cbs->get(ctx, ctx->cfg_plugin.private); // TODO: check return value
    }

    IPX_CTX_DEBUG(ctx, "Instance thread of the input plugin '%s' has been terminated!",
        plugin_name);
    pthread_exit(NULL);
}

/**
 * \brief Intermediate instance control thread
 *
 * Infinite loop that process messages from an input ring buffer and eventually pass them to
 * an output ring buffer.
 * \param[in] arg Instance context
 * \return NULL
 */
static void *
thread_intermediate(void *arg)
{
    struct ipx_ctx *ctx = (struct ipx_ctx *) arg;
    assert(ctx->type == IPX_PT_INTERMEDIATE || ctx->type == IPX_PT_OUTPUT_MGR);
    thread_set_name(ctx->name);

    const char *plugin_name = ctx->plugin_cbs->info->name;
    IPX_CTX_DEBUG(ctx, "Instance thread of the intermediate plugin '%s' has started!", plugin_name);

    ipx_msg_t *msg_ptr;
    enum ipx_msg_type msg_type;

    bool terminate = false;
    bool process_en = true; // enable message processing

    while (!terminate) {
        // Get a new message for the buffer
        msg_ptr = ipx_ring_pop(ctx->pipeline.src);
        msg_type = ipx_msg_get_type(msg_ptr);
        bool processed = false; // only not processed messages are automatically passed

        if ((process_en && (msg_type & ctx->cfg_system.msg_mask_selected) != 0)
                || ctx->type == IPX_PT_OUTPUT_MGR) { // Pass all message to the output manager
            // Process the message
            ctx->plugin_cbs->process(ctx, ctx->cfg_plugin.private, msg_ptr); // TODO:check return value
            processed = true;
        }

        if (msg_type == IPX_MSG_TERMINATE) {
            ipx_msg_terminate_t *terminate_msg = ipx_msg_base2terminate(msg_ptr);
            enum ipx_msg_terminate_type type = ipx_msg_terminate_get_type(terminate_msg);

            if (type == IPX_MSG_TERMINATE_PROCESSING) {
                // We received a request to stop passing message to the instance
                process_en = false;
            } else {
                // We received a request to terminate the instance
                terminate = true;
                continue; // Prevent sending the termination message
            }
        }

        if (!processed) {
            // Not processed by the instance, pass the message
            ipx_ring_push(ctx->pipeline.dst, msg_ptr);
        }
    }

    // Destroy the instance (usually produce garbage messages)
    IPX_CTX_DEBUG(ctx, "Calling instance destructor of the intermediate plugin '%s'", plugin_name);
    ctx->plugin_cbs->destroy(ctx, ctx->cfg_plugin.private);

    // Pass the termination message as the last message to the buffer
    assert(msg_type == IPX_MSG_TERMINATE);
    if (ctx->type != IPX_PT_OUTPUT_MGR) {
        // All intermediate plugins (except the output manager) have to pass the message here
        ipx_ring_push(ctx->pipeline.dst, msg_ptr);
    }

    IPX_CTX_DEBUG(ctx, "Instance thread of the intermediate plugin '%s' has been terminated!",
        plugin_name);
    pthread_exit(NULL);
}

/**
 * \brief Output instance control thread
 *
 * Infinite loop that process messages from an input ring buffer.
 * \param[in] arg Instance context
 * \return NULL
 */
static void *
thread_output(void *arg)
{
    struct ipx_ctx *ctx = (struct ipx_ctx *) arg;
    assert(ctx->type == IPX_PT_OUTPUT);
    thread_set_name(ctx->name);

    const char *plugin_name = ctx->plugin_cbs->info->name;
    IPX_CTX_DEBUG(ctx, "Instance thread of the output plugin '%s' has started!", plugin_name);

    bool terminate = false;
    bool process_en = true; // enable message processing

    while (!terminate) {
        // Get a new message for the buffer
        ipx_msg_t *msg_ptr = ipx_ring_pop(ctx->pipeline.src);
        enum ipx_msg_type msg_type = ipx_msg_get_type(msg_ptr);

        if (process_en && (msg_type & ctx->cfg_system.msg_mask_selected) != 0) {
            // Process the message
            ctx->plugin_cbs->process(ctx, ctx->cfg_plugin.private, msg_ptr); // TODO:check return value
        }

        if (msg_type == IPX_MSG_TERMINATE) {
            ipx_msg_terminate_t *terminate_msg = ipx_msg_base2terminate(msg_ptr);
            enum ipx_msg_terminate_type type = ipx_msg_terminate_get_type(terminate_msg);

            if (type == IPX_MSG_TERMINATE_PROCESSING) {
                // We received a request to stop passing message to the instance
                process_en = false;
            } else {
                // We received a request to terminate the instance
                terminate = true;
            }
        }

        // Decrement the counter - DO NOT TOUCH the message from this point beyond
        if (ipx_msg_header_cnt_dec(msg_ptr)) {
            // This instance is the last user, destroy it
            ipx_msg_destroy(msg_ptr);
        }
    }

    // Destroy the instance
    IPX_CTX_DEBUG(ctx, "Calling instance destructor of the output plugin '%s'", plugin_name);
    ctx->plugin_cbs->destroy(ctx, ctx->cfg_plugin.private);

    IPX_CTX_DEBUG(ctx, "Instance thread of the output plugin '%s' has been terminated!",
        plugin_name);
    pthread_exit(NULL);
}

int
ipx_ctx_run(ipx_ctx_t *ctx)
{
    if (ctx->state != IPX_CS_INIT) {
        IPX_CTX_ERROR(ctx, "Unable to start a thread of the instance because %s!",
            (ctx->state == IPX_CS_NEW) ? "a context is not initialized" : "it is running");
        return IPX_ERR_DENIED;
    }

    void *(*thread_func)(void *) = NULL;
    switch (ctx->type) {
    case IPX_PT_INPUT:
        thread_func = &thread_input;
        break;
    case IPX_PT_INTERMEDIATE:
    case IPX_PT_OUTPUT_MGR:  // Output manager is implemented as intermediate plugin
        thread_func = &thread_intermediate;
        break;
    case IPX_PT_OUTPUT:
        thread_func = &thread_output;
        break;
    default:
        IPX_CTX_ERROR(ctx, "Unable to start a thread of the instance because of unknown plugin "
            "type (%" PRIu16 ")", ctx->type);
        return IPX_ERR_DENIED;
    }

    // Block processing all signals
    sigset_t set_new, set_old;
    sigfillset(&set_new);
    pthread_sigmask(SIG_SETMASK, &set_new, &set_old);
    ctx->state = IPX_CS_RUNNING;

    // Start the thread
    int rc = pthread_create(&ctx->thread_id, NULL, thread_func, ctx);

    // Restore the previous signal mask
    pthread_sigmask(SIG_SETMASK, &set_old, NULL);

    if (rc != 0) {
        const char *err_str;
        ipx_strerror(rc, err_str);
        IPX_CTX_ERROR(ctx, "Failed to start a instance thread. pthread_create() failed: %s",
            err_str);
        ctx->state = IPX_CS_INIT;
        return IPX_ERR_DENIED;
    }

    return IPX_OK;
}

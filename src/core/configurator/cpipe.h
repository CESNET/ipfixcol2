/**
 * @file src/core/configurator/cpipe.h
 * @author Lukas Hutak <lukas.hutak@cesnet.cz>
 * @brief Configuration request pipe (header file)
 * @date 2019
 *
 * Copyright(c) 2020 CESNET z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef IPFIXCOL2_CONFIGURATOR_PIPE_H
#define IPFIXCOL2_CONFIGURATOR_PIPE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../context.h"

/*
 * Configuration pipeline
 *
 * The purpose of this pipe is to allow, for example, plugin instances and
 * signal handlers to send requests to terminate or reconfigure the collector.
 *
 * Since the pipe MUST be accessible from signal handlers, the pipe is
 * implemented as a global variable.
 */

/// Type of configuration request
enum ipx_cpipe_type {
    /**
     * @brief Slow termination request
     *
     * Request to slowly terminate the collector. Usually this request should be send when
     * there are NO more data to process. For example:
     * - When an input plugin is reading flow data from a file and end-of-file has been reached.
     *   All loaded flow data in the processing pipeline will be processed and plugin instances,
     *   which completed processing, will be gradually terminated.
     * - When an intermediate plugin is configured to stop after processing specified amount
     *   of flow records (e.g. filter plugin which should stop after matching 20 flow records).
     *   All flow records in the pipeline AFTER this plugin will be fully processed, however,
     *   flow records in the pipeline before this plugin instance are not interesting anymore and
     *   should be dropped.
     *
     * As a reaction to this request the configurator will send a termination message
     * (type #IPX_MSG_TERMINATE_INSTANCE) to the processing pipeline.
     *
     * Moreover, if the context of a plugin instance, which sent the request, is not NULL,
     * the configurator will also force stop calling processing functions (i.e. ipx_plugin_get()
     * and ipx_plugin_process()) of plugin instances BEFORE (including) the calling instance
     * as their data are no longer required.
     *
     * @note
     *   The plugin context (if not NULL) in the request will be used to inform a configuration
     *   controller about the source of termination.
     */
    IPX_CPIPE_TYPE_TERM_SLOW,
    /**
     * @brief Fast termination request
     *
     * Request to terminate the collector as fast as possible. Usually this request should be
     * send in case of a fatal failure of any plugin instance (by a particular instance thread)
     * or on a user request to terminate the collector (e.g. SIGINT/SIGTERM signals).
     *
     * As a reaction to this request the configurator will force stop calling processing functions
     * of ALL plugin instances (i.e. ipx_plugin_get() and ipx_plugin_process() callbacks) and send
     * a termination message (type #IPX_MSG_TERMINATE_INSTANCE) to the processing pipeline.
     * Unprocessed flow data in processing pipeline will be ignored.
     *
     * @note
     *   The plugin context (if not NULL) in the request will be used to inform a configuration
     *   controller about the source of termination.
     */
    IPX_CPIPE_TYPE_TERM_FAST,
    /**
     * @brief Termination complete notification (internal only!)
     *
     * The request MUST be send after termination of all plugin instances to notify
     * the configurator that it is safe to perform final cleanup. Usually, this request is
     * automatically send when a termination message (i.e. ipx_msg_terminate_t), which was send
     * by the configurator to the processing pipeline as a response to a previous
     * #IPX_CPIPE_TYPE_TERM_SLOW or #IPX_CPIPE_TYPE_TERM_FAST request, is destroyed.
     *
     * Sending this request before termination of all plugin instances is considered as fatal.
     */
    IPX_CPIPE_TYPE_TERM_DONE        ///< Terminate request - complete

    // Proposed types for the future runtime reconfiguration
    // IPX_CPIPE_TYPE_RECONF_START,
    // IPX_CPIPE_TYPE_RECONF_DONE
};

/// Configuration request
struct ipx_cpipe_req {
    /// Type of configuration message
    enum ipx_cpipe_type type;
    /// Plugin context which send the request (can be NULL)
    ipx_ctx_t *ctx;

    /// Place for additional data
};

/**
 * @brief Initialize internal configuration pipe
 *
 * @note
 *   This function MUST be called exactly ONCE before start of the configuration process.
 * @return #IPX_OK on success
 * @return #IPX_ERR_DENIED if initialization fails (e.g. unable to create a pipe, etc.)
 */
int
ipx_cpipe_init();

/**
 * @brief Destroy internal configuration pipe
 *
 * @note
 *   This function MUST be called exactly ONCE after termination of the configuration process.
 */
void
ipx_cpipe_destroy();

/**
 * @brief Get a request from the configuration pipe
 *
 * The function blocks until a request is received. If the waiting is interrupted (for example,
 * by a signal handler), waiting for the request is silently restarted.
 *
 * @warning
 *   The function MUST be called only from the configurator! (as reading is not atomic operation)
 * @param[out] msg Received message
 * @return #IPX_OK on success and @p msg is filled
 * @return #IPX_ERR_DENIED on a fatal error and the content of @p msg is undefined
 *
 */
int
ipx_cpipe_receive(struct ipx_cpipe_req *msg);

/**
 * @brief Send a new termination request
 *
 * See the description of #IPX_CPIPE_TYPE_TERM_SLOW and #IPX_CPIPE_TYPE_TERM_FAST requests for
 * more details.
 *
 * @note
 *   The function is safe to be called from a signal handler! In this case parameter @p ctx
 *   should be set to NULL.
 * @param[in] ctx  Plugin context (which is sending request) or NULL
 * @param[in] type Type of termination request
 * @return #IPX_OK on success
 * @return #IPX_ERR_ARG if the @p type is not termination request i.e. IPX_CPIPE_TYPE_TERM_*
 * @return #IPX_ERR_DENIED if the request failed to be sent
 */
int
ipx_cpipe_send_term(ipx_ctx_t *ctx, enum ipx_cpipe_type type);

#ifdef __cplusplus
}
#endif

#endif //IPFIXCOL2_CONFIGURATOR_PIPE_H

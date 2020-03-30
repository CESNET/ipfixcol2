/**
 * \file src/core/context.h
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Plugin context (internal header file)
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

#ifndef IPFIXCOL_CONTEXT_INTERNAL_H
#define IPFIXCOL_CONTEXT_INTERNAL_H

#include <ipfixcol2.h>
#include <libfds.h>
#include "fpipe.h"
#include "ring.h"

/** List of plugin callbacks  */
struct ipx_ctx_callbacks {
    /** Plugin library handle (from dlopen)                                     */
    void *handle;
    /** Description of the plugin                                               */
    const struct ipx_plugin_info *info;

    /** Plugin constructor                                                      */
    int  (*init)    (ipx_ctx_t *, const char *);
    /** Plugin destructor                                                       */
    void (*destroy) (ipx_ctx_t *, void *);

    /** Getter function (INPUT plugins only)                                    */
    int  (*get)     (ipx_ctx_t *, void *);
    /** Process function (INTERMEDIATE and OUTPUT plugins only)                 */
    int  (*process) (ipx_ctx_t *, void *, ipx_msg_t *);
    /** Close session request (INPUT plugins only, can be NULL)                 */
    void  (*ts_close)(ipx_ctx_t *, void *, const struct ipx_session *);
};

/** Identification number of output manager plugin */
#define IPX_PT_OUTPUT_MGR 255

/**
 * \brief Create a context
 *
 * Context holds local information of a plugin instance and provides uniform interface for its
 * configuration. After the context is created, almost all parameters are set to default values:
 * - record size:                         size of a record without any extensions
 * - verbosity level:                     inherited from the global configuration
 * - source and destination ring buffers: not connected (NULL)
 * - feedback pipeline:                   not connected (NULL)
 * - manager of Information Elements:     not defined (NULL)
 * - subscription mask:                   ::IPX_MSG_IPFIX (IPFIX Message)
 *
 * \note
 *   If \p callback is NULL, the context cannot be used to initialize and start new instance
 *   thread. Only purpose of this is to create a dummy context for testing. Dummy context is
 *   allowed to pass messages. However, if the output ring buffer is not specified, the
 *   messages are immediately destroyed.
 *
 * \param[in] name      Identification of the context
 * \param[in] callbacks Callback functions and description of the plugin (can be NULL, see notes)
 * \return Pointer or NULL (memory allocation error)
 */
IPX_API ipx_ctx_t *
ipx_ctx_create(const char *name, const struct ipx_ctx_callbacks *callbacks);

/**
 * \brief Destroy a context
 *
 * \note
 *   If the context has been successfully initialized but a thread hasn't been started yet, the
 *   destructor callback is called.
 * \note
 *   If the thread of the context is running, the function waits (in other words, blocks)
 *   until the thread is joined. For more information how to stop the thread, see ipx_ctx_run().
 *   In this case, the destructor callback is not called by this function. The thread is
 *   responsible.
 * \param[in] ctx Context to destroy
 */
IPX_API void
ipx_ctx_destroy(ipx_ctx_t *ctx);

/**
 * \brief Initialize a plugin instance
 *
 * The function checks that all necessary parameters for required plugin type are configured
 * and call the initialize function of the plugin instance.
 *
 * To initialize the instance of the plugin you MUST always specify:
 * - IE manager i.e. ipx_ctx_iemgr_set()
 * - initialization and destruction callback function (see struct ipx_ctx_callbacks)
 * - plugin name and correct type (see struct ipx_ctx_callbacks)
 * Moreover, Input plugins require:
 * - getter callback (see struct ipx_ctx_callbacks)
 * - feedback pipe i.e. ipx_ctx_fpipe_set()
 * - output ring buffer i.e. ipx_ctx_ring_dst_set()
 * Moreover, Intermediate plugins require:
 * - processing callback (see struct ipx_ctx_callbacks)
 * - input and output ring buffer i.e. ipx_ctx_ring_src_set() and ipx_ctx_ring_dst_set()
 * Moreover, Output plugins require:
 * - processing callback (see struct ipx_ctx_callbacks)
 * - input ring buffer i.e. ipx_ctx_ring_src_set()
 *
 * \param[in] ctx    Plugin context
 * \param[in] params XML parameters for plugin constructor
 * \return #IPX_OK on success and the plugin is ready to run (ipx_ctx_run())
 * \return #IPX_ERR_DENIED if the plugin refused to initialize itself (for example, due to invalid
 *   parameters, memory allocation error, etc.)
 * \return #IPX_ERR_ARG if not all required parameters are configured correctly
 */
IPX_API int
ipx_ctx_init(ipx_ctx_t *ctx, const char *params);

/**
 * \brief Start plugin thread
 *
 * Based on the type of the plugin (i.e. input, intermediate or output), start a thread that
 * processes pipeline messages, calls plugin callback functions, etc.
 *
 * \note
 *   To terminate the thread pass a Terminate message to the input ring buffer (Intermediate and
 *   Output plugins) or feedback pipeline (Input plugins). Context destructor ipx_ctx_destroy()
 *   waits until the thread is joined.
 * \warning
 *   Only successfully initialized instances can be started. For more information,
 *   see ipx_ctx_init()
 * \return #IPX_OK on success
 * \return #IPX_ERR_DENIED if the operation failed and the thread is not running.
 */
IPX_API int
ipx_ctx_run(ipx_ctx_t *ctx);

/**
 * \brief Get size of one IPFIX record with registered extensions (in bytes)
 * \param[in] ctx Plugin context
 * \return Size (always non-zero)
 */
IPX_API size_t
ipx_ctx_recsize_get(const ipx_ctx_t *ctx);

/**
 * \brief Set size of one IPFIX record withe registered extensions (in bytes)
 *
 * \warning
 *   The \p size MUST be at least large enough to cover a simple IPFIX record structure without
 *   extensions. Otherwise the behavior is undefined.
 * \param[in] ctx Plugin context
 * \param[in] size New size
 */
IPX_API void
ipx_ctx_recsize_set(ipx_ctx_t *ctx, size_t size);

/**
 * \brief Get a feedback pipe (only for input plugins and the IPFIX parser)
 *
 * Purpose of the pipe is to send a request to close a Transport Session. An IPFIX parser
 * generates requests and an input plugin accepts and process request. If the input plugin
 * doesn't support feedback (for example, a stream cannot be closed), the pipe is not available.
 *
 * \note Only available for input plugins and IPFIX parser
 * \param[in] ctx Plugin context
 * \return Pointer to the pipe or NULL (pipe is not available)
 */
IPX_API ipx_fpipe_t *
ipx_ctx_fpipe_get(ipx_ctx_t *ctx);

/**
 * \brief Set a reference to a feedback pipe
 *
 * Feedback pipe MUST be set for all input plugins. Moreover, IPFIX parser plugins directly
 * connected to input plugins that implement interface for processing request to close a Transport
 * Session SHOULD have connection to the pipe..
 * \param[in] ctx  Plugin context
 * \param[in] pipe New feedback pipe
 */
IPX_API void
ipx_ctx_fpipe_set(ipx_ctx_t *ctx, ipx_fpipe_t *pipe);

/**
 * \brief Set a reference to source input ring buffer (only for Intermediate and Output plugins)
 * \param[in] ctx  Plugin context
 * \param[in] ring New source ring buffer
 */
IPX_API void
ipx_ctx_ring_src_set(ipx_ctx_t *ctx, ipx_ring_t *ring);

/**
 * \brief Set a reference to destination ring buffer (only for Input and Intermediate plugins)
 * \param[in] ctx  Plugin context
 * \param[in] ring New destination ring buffer
 */
IPX_API void
ipx_ctx_ring_dst_set(ipx_ctx_t *ctx, ipx_ring_t *ring);

/**
 * \brief Set a reference to a manager of Information Elements
 * \param[in] ctx Plugin context
 * \param[in] mgr New IE manager
 */
IPX_API void
ipx_ctx_iemgr_set(ipx_ctx_t *ctx, const fds_iemgr_t *mgr);

/**
 * \brief Set verbosity of the context
 * \param[in] ctx  Plugin context
 * \param[in] verb New verbosity level
 */
IPX_API void
ipx_ctx_verb_set(ipx_ctx_t *ctx, enum ipx_verb_level verb);

/**
 * \brief Change number of termination messages that must received before plugin terminates
 *
 * By default, the context terminates running thread of its instance when a termination message
 * (type ::IPX_MSG_TERMINATE_INSTANCE) is received. However, the first intermediate instance
 * after multiple input instances can be terminated only when all input instances are not running
 * anymore.
 *
 * This function sets an internal counter that is decremented every time a termination message
 * (type ::IPX_MSG_TERMINATE_INSTANCE) is received. If the counter is non-zero after decrementation,
 * the termination message is dropped. If the counter is zero, the thread is stopped and the
 * message is passed.
 *
 * \warning
 *   This configuration parameter affects only intermediate plugins (the output manager
 *   is also intermediate plugin). Be aware of consequences of changing value if the thread
 *   is already running!
 * \param[in] ctx Plugin context
 * \param[in] cnt Non-zero number of messages
 * \return #IPX_OK on success
 * \return #IPX_ERR_DENIED if the value is not valid
 */
IPX_API int
ipx_ctx_term_cnt_set(ipx_ctx_t *ctx, unsigned int cnt);

/**
 * \brief Enable/disable data processing
 *
 * If disabled, the plugin is not allowed to process IPFIX and Session messages i.e. the
 * getter (input plugins) or processing function (intermediate and output plugins) will not
 * be called on a message arrival. Moreover, IPFIX and Session Messages will be dropped as
 * the plugin cannot process them. Other message types (termination, garbage, etc) will be
 * processed by the thread controller and automatically passed to the following plugin(s)
 * - usually valid only for intermediate plugins.
 *
 * \note
 *   By default, data processing is enabled.
 * \param[in] ctx Plugin context
 * \param[in] en  Enable/disable processing
 */
IPX_API void
ipx_ctx_processing_set(ipx_ctx_t *ctx, bool en);

/**
 * \brief Get registered extensions and dependencies
 *
 * \note
 *   Keep on mind that the array is filled only after plugin initialization. Moreover,
 *   the most plugins don't use extension at all, so the array is usually empty.
 * \warning
 *   Don't change the size and offset of extensions if the plugin is already running!
 * \param[in] ctx       Plugin context
 * \param[out] arr      Array with extensions and dependencies
 * \param[out] arr_size Size of the array
 */
IPX_API void
ipx_ctx_ext_defs(ipx_ctx_t *ctx, struct ipx_ctx_ext **arr, size_t *arr_size);

/**
 * @brief Get plugin description (name, version, etc.)
 * @param[in] ctx Plugin context
 * @return Pointer to the plugin info (loaded directly from the plugin)
 */
IPX_API const struct ipx_plugin_info *
ipx_ctx_plugininfo_get(const ipx_ctx_t *ctx);

#endif // IPFIXCOL_CONTEXT_INTERNAL_H

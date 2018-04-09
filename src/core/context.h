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
 * \brief Initialize plugin
 *
 * The function checks that all necessary parameters for required plugin type are configured
 * and call the initialize function of the plugin instance.
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
 * \brief Overwrite default types of messages that can be processed by an instance
 *   (only for Intermediate and Output plugins)
 *
 * By default, only ::IPX_MSG_IPFIX (IPFIX Message) and ::IPX_MSG_SESSION (Transport Session
 * Message) types can be passed to plugin instance for processing. However, implementation of the
 * output manager (as an intermediate plugin) requires processing of almost all types of messages.
 * \warning
 *   This configuration does NOT change subscription mask of the plugin. It must be done separately.
 * \note
 *  To allow processing of all type of messages, use #IPX_MSG_MASK_ALL
 * \param[in] ctx  Plugin context
 * \param[in] mask New mask
 */
IPX_API void
ipx_ctx_overwrite_msg_mask(ipx_ctx_t *ctx, ipx_msg_mask_t mask);

#endif // IPFIXCOL_CONTEXT_INTERNAL_H

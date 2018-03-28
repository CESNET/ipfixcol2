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

/** Extension record of Data Record  */
struct ipx_ctx_rext {
    /** Extension offset      */
    uint16_t offset;
    /** Data size             */
    uint16_t size;
};

/** List of plugin callbacks  */
struct ipx_ctx_callbacks {
    /** Plugin constructor                                                      */
    int  (*init)    (ipx_ctx_t *, const char *);
    /** Plugin destructor                                                       */
    void (*destroy) (ipx_ctx_t *, void *);

    /** Getter function (INPUT plugins only)                                    */
    int  (*get)     (ipx_ctx_t *, void *);
    /** Process function (INTERMEDIATE and OUTPUT plugins only)                 */
    int  (*process) (ipx_ctx_t *, void *, ipx_msg_t *);
    /** Close session request (INPUT plugins only, can be NULL)                 */
    int  (*ts_close)(ipx_ctx_t *, void *, const struct ipx_session *);

    /** Update prepare (can be NULL)                                            */
    int  (*update_prepare)(ipx_ctx_t *, void *, uint16_t, const char *);
    /** Update commit (can be NULL)                                             */
    int  (*update_commit) (ipx_ctx_t *, void *, void *);
    /** Update abort (can be NULL)                                              */
    void (*update_abort)  (ipx_ctx_t *, void *, void *);
};

/*
IPX_API ipx_ctx_t *
ipx_ctx_create(const char *name, enum ipx_plugin_type type, const struct ipx_ctx_callbacks *cbs);

IPX_API void
ipx_ctx_destroy(ipx_ctx_t *ctx);

IPX_API int
ipx_ctx_initialize(ipx_ctx_t *ctx);

IPX_API void
ipx_ctx_initialize(ipx_ctx_t *ctx);

IPX_API int
ipx_ctx_run(ipx_ctx_t *ctx);
*/

/**
 * \brief Get size of one IPFIX record with registered extensions (in bytes)
 * \param[in] ctx Plugin context
 * \return Size (always non-zero)
 */
IPX_API size_t
ipx_ctx_recsize_get(const ipx_ctx_t *ctx);

/**
 * \brief Get a feedback pipe
 *
 * Purpose of the pipe is to send a request to close a Transport Session. An IPFIX parser
 * generates requests and an input plugin accepts and process request. If the input plugin
 * doesn't support feedback (for example, a stream cannot be closed), the pipe is not available.
 *
 * \note Only available for input plugins and IPFIX parser
 * \param[in] ctx Plugin context
 * \return
 */
IPX_API ipx_fpipe_t *
ipx_ctx_fpipe_get(ipx_ctx_t *ctx);




#endif // IPFIXCOL_CONTEXT_INTERNAL_H

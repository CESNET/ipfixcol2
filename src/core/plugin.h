/**
 * \file src/core/plugin.c
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

#ifndef IPFIXCOL_PLUGIN_CTX_H
#define IPFIXCOL_PLUGIN_CTX_H

#include <ipfixcol2.h>
#include <libfds.h>
#include "ring.h"

/** Invalid file descriptor                       */
#define IPX_FD_INV (-1)
/** Invalid offset                                */
#define IPX_REXT_OFFSET_INV (65535)
/** Maximum number of registered extensions       */
#define IPX_REXT_MAX (16)

enum ipx_ctx_permissions {
    /** Permission to pass a message              */
    IPX_CP_MSG_PASS    = (1 << 0),
    /** Permission to register a new extension    */
    IPX_CP_REXT_REG    = (1 << 1),
    /** Permission to deregister an extension     */
    IPX_CP_REXT_DEREG  = (1 << 2),
    /** Permission to subscribe to an extension   */
    IPX_CT_REXT_SUB    = (1 << 3),
    /** Permission to unsubscribe to an extension */
    IPX_CT_REXT_UNSUB  = (1 << 4)
};

/** Extension record of Data Record  */
struct ipx_ctx_rext {
    /** Extension offset      */
    uint16_t offset;
    /** Data size             */
    uint16_t size;
};

/**
 * \brief Context a plugin instance
 */
struct ipx_ctx {
    /** Plugin identification name                                               */
    char *name;
    /** Plugin type                                                              */
    enum ipx_plugin_type type;
    /** Permission flags (see #ipx_ctx_permissions)                              */
    uint32_t permissions;

    struct {
        /**
         * Previous plugin (i.e. source of messages)
         * \note NULL for input plugins
         */
        ipx_ring_t *prev;
        /**
         * Next plugin (i.e. destination of messages)
         * \note NULL for output plugins
         */
        ipx_ring_t *next;
    } pipeline; /**< Connection to internal communication pipeline               */

    struct {
        /**
         * Pipe file descriptor for an input plugin that supports parser's feedback
         * \note If the file descriptor is not valid, value ::IPX_FD_INV is set.
         */
        int fd_read;
        /**
         * Pipe file descriptor for an IPFIX message processor
         * \note If the file descriptor is not valid, value ::IPX_FD_INV is set.
         */
        int fd_write;

        // TODO: pointer to the control variable

    } feedback; /**< IPFIX parser feedback                                       */

    struct {
        /** Private data of the instance                                         */
        void *private;
        /** Update data                                                          */
        void *update;
    } cfg_plugin; /**< Plugin configuration                                      */

    struct {
        /**
         * Message subscription mask
         * \note The value represents bitwise OR of #ipx_msg_type flags
         */
        uint16_t msg_mask;
        /** Pointer to the current manager of Information Elements (can be NULL) */
        const fds_iemgr_t *ie_mgr;
        /** Current size of IPFIX record (with registered extensions)            */
        size_t rec_size;
        /** Verbosity level of the plugin                                        */
        uint8_t  vlevel;

        // TODO: reference to extension table

    } cfg_system; /**< System configuration                                      */

    /** Array of record extensions                                               */
    struct ipx_ctx_rext rext[IPX_REXT_MAX];
};



// Is transport session "close request" available
IPX_API int
ipx_ctx_ts_creq_avail(const ipx_ctx_t *ctx); // is feedback available
// Send transport session "close request"
IPX_API int
ipx_ctx_ts_creq_send(ipx_ctx_t *ctx, const struct ipx_session *session);






#endif // IPFIXCOL_PLUGIN_CTX_H

/**
 * \file src/core/message_base.h
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief General specification of messages for the IPFIXcol pipeline (internal header file)
 * \date 2016-2018
 */

/* Copyright (C) 2016-2018 CESNET, z.s.p.o.
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

#ifndef IPFIXCOL_MESSAGE_BASE_H
#define IPFIXCOL_MESSAGE_BASE_H

#include <ipfixcol2.h>
#include <assert.h>

/**
 * \internal
 * \defgroup ipxInternalGeneralMessage Collector internal messages
 * \ingroup internalAPIs
 * \brief General specification of messages for the collector pipeline
 * \warning This structure and functions are only for internal use!
 * @{
 */

/**
 * \brief Header of all messages for the collector pipeline.
 *
 * \remark Never use this structure directly, instead use API functions, because internal elements
 *   can be changed.
 * \warning This structure MUST always be the first element of any message structure for the
 *   collector pipeline, because it serves as an identification of the message type.
 */
struct ipx_msg {
    /** Type of the message */
    enum ipx_msg_type type;

    /*
     * TODO: add number of references to this message only for output manager!
     * atomic_uint ref_cnt;
     */
};

static_assert(offsetof(struct ipx_msg, type) == 0,
    "Message type must be the first element of each IPFIXcol message.");

/**
 * \brief Initialize the header of a general message
 * \param[in] header  Pointer to the header of the message
 * \param[in]  type   Type of the header
 * \return On success returns #IPX_OK. Otherwise returns non-zero value.
 */
static inline void
ipx_msg_header_init(struct ipx_msg *header, enum ipx_msg_type type)
{
    header->type = type;
}

/**
 * \brief Destroy the header of a general message
 *
 * This functions is now just a placeholder for possible future implementation.
 * \param[in] header Pointer to the header of the mesage
 */
static inline void
ipx_msg_header_destroy(struct ipx_msg *header)
{
    (void) header;
}

#endif //IPFIXCOL_MESSAGE_BASE_H

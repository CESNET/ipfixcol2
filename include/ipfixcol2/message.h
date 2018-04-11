/**
 * \file include/ipfixcol2/message.h
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief General specification of messages for the IPFIXcol pipeline
 *   (header file)
 * \date 2018
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

#ifndef IPX_MESSAGE_H
#define IPX_MESSAGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <ipfixcol2/api.h>
#include <stddef.h>
#include <stdint.h>

/**
 * \defgroup ipxBaseMessage Collector internal messages
 * \ingroup publicAPIs
 * \brief Base specification of messages for the collector pipeline
 *
 * Base type for all messages in the collector pipeline. It is guaranteed that all messages for
 * the pipeline can be cast to this base type and than be transferred throw the pipeline and
 * converted back to the original type.
 *
 * @{
 */

/**
 * \brief Types of messages for the collector pipeline
 */
enum ipx_msg_type {
    /** A message with a parsed IPFIX message from a source of flows                */
    IPX_MSG_IPFIX     = (1 << 0),
    /** A transport session status i.e. information about (dis)connections          */
    IPX_MSG_SESSION   = (1 << 1),
    /** An generic object destructor (usually only for internal usage)              */
    IPX_MSG_GARBAGE   = (1 << 2),
    /** A terminate message (only for internal usage)                               */
    IPX_MSG_TERMINATE = (1 << 3),
    // An internal configuration message (only for internal usage)
    //IPX_MSG_CONFIG  = (1 << 4)
};

/** The data type of the base message                                               */
typedef struct ipx_msg ipx_msg_t;
/** Unsigned numeric type that is able to handle bit OR of all message types        */
typedef uint16_t ipx_msg_mask_t;
/** Mask that covers all types of message                                           */
#define IPX_MSG_MASK_ALL (UINT16_MAX)

/**
 * \brief Get the type of a message for the collector pipeline
 * \param[in] msg  Message
 * \return Type of the message
 */
IPX_API enum ipx_msg_type
ipx_msg_get_type(ipx_msg_t *msg);

/**
 * \brief Destroy a message for the collector pipeline
 *
 * Based on type of the message call particular destructor of the message and
 * free allocated memory.
 * \param[in] msg Pointer to the message
 */
IPX_API void
ipx_msg_destroy(ipx_msg_t *msg);

#include <ipfixcol2/message_ipfix.h>
#include <ipfixcol2/message_garbage.h>
#include <ipfixcol2/message_session.h>

/**
 * \brief Cast from a base message to a session message
 * \warning If the base message is not a session message, the result is undefined!
 * \param[in] msg Pointer to the base message
 * \return Pointer to the session message.
 */
static inline ipx_msg_session_t *
ipx_msg_base2session(ipx_msg_t *msg)
{
    assert(ipx_msg_get_type(msg) == IPX_MSG_SESSION);
    return (ipx_msg_session_t *) msg;
}

/**
 * \brief Cast from a base message to a garbage message
 * \warning If the base message is not a garbage message, the result is undefined!
 * \param[in] msg Pointer to the base message
 * \return Pointer to the garbage message
 */
static inline ipx_msg_garbage_t *
ipx_msg_base2garbage(ipx_msg_t *msg)
{
    assert(ipx_msg_get_type(msg) == IPX_MSG_GARBAGE);
    return (ipx_msg_garbage_t *) msg;
}

/**
 * \brief Cast from a base message to an IPFIX message
 * \param[in] msg Pointer to the base message
 * \warning If the base message is not an IPFIX message, the result is undefined!
 * \return Pointer to the IPFIX message
 */
static inline ipx_msg_ipfix_t *
ipx_msg_base2ipfix(ipx_msg_t *msg)
{
    assert(ipx_msg_get_type(msg) == IPX_MSG_IPFIX);
    return (ipx_msg_ipfix_t *) msg;
}

/**@}*/

#ifdef __cplusplus
}
#endif
#endif // IPX_MESSAGE_H

/**
 * \file include/ipfixcol2/message.h
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief General specification of messages for the IPFIXcol pipeline
 *   (header file)
 * \date 2016
 */

/* Copyright (C) 2016 CESNET, z.s.p.o.
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

#ifndef IPFIXCOL_MESSAGE_H
#define IPFIXCOL_MESSAGE_H

#include <ipfixcol2/api.h>
#include <stddef.h>

/**
 * \defgroup ipxGeneralMessage Collector internal messages
 * \ingroup publicAPIs
 * \brief General specification of messages for the collector pipeline
 *
 * General type for all messages in the collector pipeline. It is guaranteed
 * that all messages for the pipeline can be cast to this general type and
 * than be transferred throw the pipeline and converted back to the original
 * type.
 *
 * @{
 */

/**
 * \brief Types of messages for the collector pipeline
 */
enum IPX_MSG_TYPE {
	/** A message with a parsed IPFIX packet from a source of flows         */
	IPX_MSG_IPFIX   = (1 << 0),
	/** A transport session status i.e. information about (dis)connections  */
	IPX_MSG_SESSION = (1 << 1),
	/** An generic object destructor (usually only for internal usage)      */
	IPX_MSG_GARBAGE = (1 << 2)
};

/** The datatype of the general message                                     */
typedef struct ipx_msg ipx_msg_t;

/**
 * \brief Get the type of a message for the collector pipeline
 * \param[in] message  Message
 * \return Type of the message
 */
IPX_API enum IPX_MSG_TYPE
ipx_msg_get_type(ipx_msg_t *message);

/**
 * \brief Destroy a message for the collector pipeline
 *
 * Based on type of the message call particular destructor of the message and
 * free allocated memory.
 * \param[in] message Pointer to the message
 */
IPX_API void
ipx_msg_destroy(ipx_msg_t *message);

#include <ipfixcol2/message_garbage.h>
#include <ipfixcol2/message_session.h>

/**
 * \brief Try to cast from a general message to a session message
 * \param[in] message Pointer to the general message
 * \return If the general \p message can be cast to the session message,
 *   returns a valid pointer to the session message. Otherwise returns NULL.
 */
static inline ipx_session_msg_t *
ipx_msg_cast2session(ipx_msg_t *message)
{
	return (ipx_msg_get_type(message) == IPX_MSG_SESSION)
		? ((ipx_session_msg_t *) message)
		: NULL;
}

/**
 * \brief Try to cast from a general message to a garbage message
 * \param[in] message Pointer to the general message
 * \return If the general \p message can be cast to the garbage message,
 *   returns a valid pointer to the garbage message. Otherwise returns NULL.
 */
static inline ipx_garbage_msg_t *
ipx_msg_cast2garbage(ipx_msg_t *message)
{
	return (ipx_msg_get_type(message) == IPX_MSG_GARBAGE)
		? ((ipx_garbage_msg_t *) message)
		: NULL;
}


/**@}*/

#endif //IPFIXCOL_MESSAGE_H

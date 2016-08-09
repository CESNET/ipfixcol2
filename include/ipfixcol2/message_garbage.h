/**
 * \file include/ipfixcol2/message_garbage.h
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Garbage message (header file)
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

#ifndef IPFIXCOL_MESSAGE_GARBAGE_H
#define IPFIXCOL_MESSAGE_GARBAGE_H

/**
 * \defgroup ipxInternalGarbageMessage Garbage message
 * \ingroup ipxGeneralMessage
 *
 * \brief Garbage message interface
 *
 * A message for destruction of objects that are intended to be destroyed, but
 * can be referenced by other still existing objects farther down in
 * the collector's pipeline.
 *
 * For example, a withdrawn template that can be referenced by still existing
 * packets >>farther down<< in the pipeline. This message will call a callback
 * to destroy the object before the destruction of this message. It ensures
 * that there are no objects >>farther down<< in the pipeline with references
 * to this object, because that objects have been already destroyed.
 *
 * \warning Already destroyed objects MUST never be used again.
 * \remark Identification type of this message is #IPX_MSG_GARBAGE.
 *
 * @{
 */

/** \brief The type of garbage message                                      */
typedef struct ipx_garbage_msg ipx_garbage_msg_t;

#include <ipfixcol2/message.h>

/**
 * \typedef ipx_garbage_msg_cb
 * \brief Object destruction callback for a garbage message type
 * \param[in,out] object Object to be destroyed
 */
typedef void (*ipx_garbage_msg_cb)(void *object);

/**
 * \brief Create a garbage message
 *
 * An \p object and a callback \p cb must be defined.
 * \param[in,out] object   Pointer to the object to be destroyed
 * \param[in]     callback Object destruction function
 * \return On success returns pointer to the message. Otherwise returns NULL.
 */
API ipx_garbage_msg_t *
ipx_garbage_msg_create(void *object, ipx_garbage_msg_cb callback);

/**
 * \brief Destroy a garbage message
 *
 * First, call the destructor of an internal object and than destroy itself.
 * \param[in,out] message Pointer to the message
 */
API void
ipx_garbage_msg_destroy(ipx_garbage_msg_t *message);

/**
 * \brief Cast from a general message to a garbage message
 * \param[in] message Pointer to the garbage message
 * \return Pointer to the general message.
 */
static inline ipx_msg_t *
ipx_garbage_msg_cast2general(ipx_garbage_msg_t *message)
{
	return (ipx_msg_t *) message;
}

/**@}*/

#endif //IPFIXCOL_MESSAGE_DESTRUCTIVE_H
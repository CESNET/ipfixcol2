/**
 * \file src/core/message_terminate.h
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Terminate message (internal header file)
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

#ifndef IPFIXCOL_MESSAGE_TERMINATE_H
#define IPFIXCOL_MESSAGE_TERMINATE_H

#ifdef __cplusplus
extern "C" {
#endif

/** \brief The type of terminate message           */
typedef struct ipx_msg_terminate ipx_msg_terminate_t;

#include <ipfixcol2/message.h>

/** Type of instance termination */
enum ipx_msg_terminate_type {
    /**
     * \brief Stop instance
     *
     * After receiving this message, a context MUST call plugin destructor on its instance and
     * terminate thread of the context.
     */
    IPX_MSG_TERMINATE_INSTANCE
};

/**
 * \brief Create a termination message
 *
 * Purpose of the message is to signalize instance that it's time to stop processing messages,
 * execute destructor of the instance and terminate context thread. So evidently, this MUST be the
 * last message send through pipeline.
 *
 * \return Pointer ot the message or NULL (memory allocation error)
 */
IPX_API ipx_msg_terminate_t *
ipx_msg_terminate_create(enum ipx_msg_terminate_type type);

/**
 * \brief Destroy a termination message
 * \param[in] msg Pointer to the message
 */
IPX_API void
ipx_msg_terminate_destroy(ipx_msg_terminate_t *msg);

/**
 * \brief Get the termination type
 * \param msg Pointer to the the message
 * \return Type
 */
IPX_API enum ipx_msg_terminate_type
ipx_msg_terminate_get_type(const ipx_msg_terminate_t *msg);

/**
 * \brief Cast from a terminate message to a base message
 * \param[in] msg Pointer to the terminate message
 * \return Pointer to the base message
 */
static inline ipx_msg_t *
ipx_msg_terminate2base(ipx_msg_terminate_t *msg)
{
    return (ipx_msg_t *) msg;
}

#ifdef __cplusplus
}
#endif
#endif //IPFIXCOL_MESSAGE_TERMINATE_H

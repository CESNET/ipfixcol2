/**
 * \file include/ipfixcol2/message_session.h
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Source session status messages (header file)
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

#ifndef IPX_MESSAGE_SESSION_H
#define IPX_MESSAGE_SESSION_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \defgroup ipxSourceSessionMessage Source session status message
 * \ingroup ipxGeneralMessage
 *
 * \brief Source session status message interface
 *
 * A message with a notification about an Exporting Process connection or
 * disconnection. The notification is related to a Source Session and can be
 * useful for preparing or cleaning up internal structures of plugins.
 *
 * \remark Identification type of this message is #IPX_MSG_SESSION.
 *
 * @{
 */

/** \brief The type of source session message                                */
typedef struct ipx_msg_session ipx_msg_session_t;

#include <ipfixcol2/message.h>
#include <ipfixcol2/session.h>

/**
 * \brief Type of a session event of a flow source
 */
enum ipx_msg_session_event {
    IPX_MSG_SESSION_OPEN,   /**< New source connected (new Source Session)  */
    IPX_MSG_SESSION_CLOSE   /**< Source disconnected or connection timeout  */
};

/**
 * \brief Create a new status message about a Source Session
 * \param[in] event   Type of a session event
 * \param[in] session Pointer to the session
 * \return On success returns a pointer to the message. Otherwise returns NULL.
 */
IPX_API ipx_msg_session_t *
ipx_msg_session_create(const struct ipx_session *session, enum ipx_msg_session_event event);

/**
 * \brief Destroy a status message
 *
 * Only destroy the message. A Source Session is not freed.
 * \param[in] msg Pointer to the message
 */
IPX_API void
ipx_msg_session_destroy(ipx_msg_session_t *msg);

/**
 * \brief Get the event type of a Source Session
 * \param[in] msg Pointer to the message
 * \return Event type
 */
IPX_API enum ipx_msg_session_event
ipx_msg_session_get_event(const ipx_msg_session_t *msg);

/**
 * \brief Get the Source Session referenced in a message
 * \param[in] msg Pointer to the message
 * \return Source Session
 */
IPX_API const struct ipx_session *
ipx_msg_session_get_session(const ipx_msg_session_t *msg);

/**
 * \brief Cast from a source session message to a base message
 * \param[in] msg Pointer to the session message
 * \return Pointer to the base message.
 */
static inline ipx_msg_t *
ipx_msg_session2base(ipx_msg_session_t *msg)
{
    return (ipx_msg_t *) msg;
}

/**@}*/
#ifdef __cplusplus
}
#endif
#endif // IPX_MESSAGE_SESSION_H

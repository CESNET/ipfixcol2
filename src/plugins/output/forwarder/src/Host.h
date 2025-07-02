/**
 * \file src/plugins/output/forwarder/src/Host.h
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * \brief Host class header
 * \date 2021
 */

/* Copyright (C) 2021 CESNET, z.s.p.o.
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

#pragma once

#include <unordered_map>
#include <memory>
#include <ipfixcol2.h>
#include "common.h"
#include "Config.h"
#include "Connection.h"
#include "connector/Connector.h"

/// A class representing one of the subcollectors messages are forwarded to
class Host {
public:
    /**
     * \brief The constructor
     * \param ident                      The host identification
     * \param con_params                 The connection parameters
     * \param log_ctx                    The logging context
     * \param tmplts_resend_pkts         Interval in packets after which templates are resend (UDP only)
     * \param tmplts_resend_secs         Interval in seconds after which templates are resend (UDP only)
     * \param indicate_lost_msgs         Indicate that the message has been lost if it couldn't be forwarded
     *                                   by increasing the sequence numbers
     */
    Host(const std::string &ident, ConnectionParams con_params, ipx_ctx_t *log_ctx,
         unsigned int tmplts_resend_pkts, unsigned int tmplts_resend_secs, bool indicate_lost_msgs,
         Connector &connector);

    /**
     * Disable copy and move constructors
     */
    Host(const Host &) = delete;
    Host(Host &&) = delete;

    /**
     * \brief The destructor - finishes all the connections
     */
    ~Host();

    /**
     * \brief Set up a new connection for the sesison
     * \param session  The session
     */
    void
    setup_connection(const ipx_session *session);

    /**
     * \brief Finish a connection for the sesison
     * \param session  The session
     */
    void
    finish_connection(const ipx_session *session);

    /**
     * \brief Forward an IPFIX message to this host
     * \param msg  The IPFIX message
     * \return true on success, false on failure
     * \throw ConnectionError when the connection fails
     */
    bool
    forward_message(ipx_msg_ipfix_t *msg);

    /**
     * \brief Advance the unfinished transfers
     */
    void
    advance_transfers();

private:
    const std::string &m_ident;

    ConnectionParams m_con_params;

    ipx_ctx_t *m_log_ctx;

    unsigned int m_tmplts_resend_pkts;

    unsigned int m_tmplts_resend_secs;

    bool m_indicate_lost_msgs;

    Connector &m_connector;

    std::unordered_map<const ipx_session *, std::unique_ptr<Connection>> m_session_to_connection;
};

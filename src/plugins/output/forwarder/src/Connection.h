/**
 * \file src/plugins/output/forwarder/src/Connection.h
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * \brief Connection class header
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

#include <vector>
#include <unordered_map>
#include <memory>
#include <atomic>

#include <ipfixcol2.h>

#include "common.h"
#include "connector/Connector.h"
#include "Sender.h"

class Connection;

/// An error to be thrown on connection errors
class ConnectionError {
public:
    ConnectionError(std::string message)
    : m_message(message) {}

    ConnectionError(std::string message, std::shared_ptr<Connection> &connection)
    : m_message(message), m_connection(&connection) {}

    ConnectionError with_connection(std::shared_ptr<Connection> &connection) const
    { return ConnectionError(m_message, connection); }

    const char *what() const { return m_message.c_str(); }

    std::shared_ptr<Connection> *connection() const { return m_connection; }

private:
    std::string m_message;
    std::shared_ptr<Connection> *m_connection;
};

/// A transfer to be sent through the connection
struct Transfer {
    /// The data to send
    std::vector<uint8_t> data;
    /// The offset to send from, i.e. the amount of data that was already sent
    uint16_t offset;
};

/// A class representing one of the connections to the subcollector
/// Each host opens one connection per session
class Connection {
public:
    /**
     * \brief The constructor
     * \param ident               The host identification
     * \param con_params          The connection parameters
     * \param log_ctx             The logging context
     * \param tmplts_resend_pkts  Interval in packets after which templates are resend (UDP only)
     * \param tmplts_resend_secs  Interval in seconds after which templates are resend (UDP only)
     */
    Connection(const std::string &ident, ConnectionParams con_params, ipx_ctx_t *log_ctx,
               unsigned int tmplts_resend_pkts, unsigned int tmplts_resend_secs,
               Connector &connector);

    /// Do not permit copying or moving as the connection holds a raw socket that is closed in the destructor
    /// (we could instead implement proper moving and copying behavior, but we don't really need it at the moment)
    Connection(const Connection &) = delete;
    Connection(Connection &&) = delete;

    /**
     * \brief Connect the connection socket
     * \throw ConnectionError if the socket couldn't be connected
     */
    void
    connect();

    /**
     * \brief Forward an IPFIX message
     * \param msg  The IPFIX message
     */
    void
    forward_message(ipx_msg_ipfix_t *msg);

    /**
     * \brief Lose an IPFIX message, i.e. update the internal state as if it has been forwarded
     *        even though it is not being sent
     * \param msg  The IPFIX message
     */
    void
    lose_message(ipx_msg_ipfix_t *msg);

    /**
     * \brief Advance the unfinished transfers
     */
    void
    advance_transfers();

    /**
     * \brief Check if the connection socket is currently connected
     * \return true or false
     */
    bool check_connected();

    /**
     * \brief Get number of transfers still waiting to be transmitted
     * \return The number of waiting transfers
     */
    size_t waiting_transfers_cnt() const { return m_transfers.size(); }

    /**
     * \brief The identification of the connection
     */
    const std::string &ident() const { return m_ident; }

private:
    const std::string &m_ident;

    ConnectionParams m_con_params;

    ipx_ctx_t *m_log_ctx;

    unsigned int m_tmplts_resend_pkts;

    unsigned int m_tmplts_resend_secs;

    UniqueFd m_sockfd;

    std::shared_ptr<FutureSocket> m_future_socket;

    std::unordered_map<uint32_t, std::unique_ptr<Sender>> m_senders;

    std::vector<Transfer> m_transfers;

    Connector &m_connector;

    void
    store_unfinished_transfer(Message &msg, uint16_t offset);

    void
    send_message(Message &msg);

    Sender &
    get_or_create_sender(ipx_msg_ipfix_t *msg);

    void
    check_socket_error(ssize_t sock_ret);

    void
    on_connection_lost();
};

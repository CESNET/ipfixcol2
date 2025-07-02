/**
 * \file src/plugins/output/forwarder/src/Host.cpp
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * \brief Host class implementation
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

#include "Host.h"
#include <algorithm>

Host::Host(const std::string &ident, ConnectionParams con_params, ipx_ctx_t *log_ctx,
           unsigned int tmplts_resend_pkts, unsigned int tmplts_resend_secs, bool indicate_lost_msgs,
           Connector &connector) :
    m_ident(ident),
    m_con_params(con_params),
    m_log_ctx(log_ctx),
    m_tmplts_resend_pkts(tmplts_resend_pkts),
    m_tmplts_resend_secs(tmplts_resend_secs),
    m_indicate_lost_msgs(indicate_lost_msgs),
    m_connector(connector)
{
}

void
Host::setup_connection(const ipx_session *session)
{
    // There shouldn't be a connection for the session yet
    assert(m_session_to_connection.find(session) == m_session_to_connection.end());

    // Create new connection
    IPX_CTX_INFO(m_log_ctx, "Setting up new connection to %s", m_ident.c_str());

    m_session_to_connection.emplace(session,
        std::unique_ptr<Connection>(new Connection(
            m_ident,
            m_con_params,
            m_log_ctx,
            m_tmplts_resend_pkts,
            m_tmplts_resend_secs,
            m_connector)));
    m_session_to_connection[session]->connect();
}

void
Host::finish_connection(const ipx_session *session)
{
    IPX_CTX_INFO(m_log_ctx, "Finishing a connection to %s", m_ident.c_str());

    // Get the corresponding connection and remove it from the session map
    std::unique_ptr<Connection> &connection = m_session_to_connection[session];
    assert(connection);

    // Try to send the waiting transfers if there are any, if the connection is dropped just finish anyway
    if (connection->check_connected()) {

        try {
            connection->advance_transfers();

        } catch (const ConnectionError &) {
            // Ignore...
        }
    }

    if (connection->waiting_transfers_cnt() > 0) {
        IPX_CTX_WARNING(m_log_ctx, "Dropping %zu transfers when finishing connection",
                        connection->waiting_transfers_cnt());
    }

    IPX_CTX_INFO(m_log_ctx, "Connection to %s finished", m_ident.c_str());

    m_session_to_connection.erase(session);
}

bool
Host::forward_message(ipx_msg_ipfix_t *msg)
{
    const ipx_session *session = ipx_msg_ipfix_get_ctx(msg)->session;
    Connection &connection = *m_session_to_connection[session].get();

    if (!connection.check_connected()) {
        if (m_indicate_lost_msgs) {
            connection.lose_message(msg);
        }
        return false;
    }

    try {
        connection.advance_transfers();

        if (connection.waiting_transfers_cnt() > 0) {
            IPX_CTX_DEBUG(m_log_ctx, "Message to %s not forwarded because there are unsent transfers\n", m_ident.c_str());
            if (m_indicate_lost_msgs) {
                connection.lose_message(msg);
            }
            return false;
        }

        IPX_CTX_DEBUG(m_log_ctx, "Forwarding message to %s\n", m_ident.c_str());

        connection.forward_message(msg);

    } catch (const ConnectionError &err) {
        IPX_CTX_ERROR(m_log_ctx, "Lost connection while forwarding: %s", err.what());
        connection.connect();
        return false;
    }

    return true;
}

void
Host::advance_transfers()
{
    for (auto &p : m_session_to_connection) {
        Connection &connection = *p.second.get();
        if (connection.check_connected()) {
            connection.advance_transfers();
        }
    }
}

Host::~Host()
{
    for (auto &p : m_session_to_connection) {
        std::unique_ptr<Connection> &connection = p.second;

        // Try to send the waiting transfers if there are any, if the connection is dropped just finish anyway
        if (connection->check_connected()) {

            try {
                connection->advance_transfers();

            } catch (const ConnectionError &) {
                // Ignore...
            }
        }

        if (connection->waiting_transfers_cnt() > 0) {
            IPX_CTX_WARNING(m_log_ctx, "Dropping %zu transfers when closing connection %s",
                            connection->waiting_transfers_cnt(), connection->ident().c_str());
        }
    }

    IPX_CTX_INFO(m_log_ctx, "All connections to %s closed", m_ident.c_str());
}

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
           unsigned int tmplts_resend_pkts, unsigned int tmplts_resend_secs) :
    m_ident(ident),
    m_con_params(con_params),
    m_log_ctx(log_ctx),
    m_tmplts_resend_pkts(tmplts_resend_pkts),
    m_tmplts_resend_secs(tmplts_resend_secs)
{
}

void
Host::setup_connection(const ipx_session *session)
{
    // There shouldn't be a connection for the session yet
    assert(m_session_to_connection.find(session) == m_session_to_connection.end());

    // Create new connection
    auto connection_holder = std::unique_ptr<Connection>(
        new Connection(m_ident, m_con_params, m_log_ctx, m_tmplts_resend_pkts, m_tmplts_resend_secs)
    );
    Connection *connection = connection_holder.get();
    m_connections.push_back(std::move(connection_holder));

    IPX_CTX_INFO(m_log_ctx, "Setting up new connection to %s", m_ident.c_str());

    // Try to connect
    try {
        connection->connect();

    } catch (const ConnectionError &err) {
        IPX_CTX_WARNING(m_log_ctx, "Setting up new connection to %s failed! Will try to reconnect...", 
                        m_ident.c_str());
        m_reconnects.push_back(connection);
    }

    // Set up map from session to connection
    m_session_to_connection.emplace(session, connection);
}

void
Host::finish_connection(const ipx_session *session)
{
    IPX_CTX_INFO(m_log_ctx, "Finishing a connection to %s", m_ident.c_str());

    // Get the corresponding connection and remove it from the session map
    Connection *connection = m_session_to_connection[session];
    assert(connection);
    m_session_to_connection.erase(session);

    // Check if we can close the connection yet or there are still transfers to process
    if (connection->waiting_transfers_cnt() > 0) {
        connection->m_finishing_flag = true;

        IPX_CTX_INFO(m_log_ctx, "Connection %s still has %lu transfers waiting",
                      m_ident.c_str(), connection->waiting_transfers_cnt());

    } else {
        m_reconnects.erase(std::remove(m_reconnects.begin(), m_reconnects.end(), connection), 
                           m_reconnects.end());

        m_connections.erase(
            std::remove_if(m_connections.begin(), m_connections.end(),
                [=](const std::unique_ptr<Connection> &item) { return item.get() == connection; }),
            m_connections.end());

        IPX_CTX_INFO(m_log_ctx, "Connection %s closed", m_ident.c_str());
    }
}

bool
Host::forward_message(ipx_msg_ipfix_t *msg)
{
    const ipx_session *session = ipx_msg_ipfix_get_ctx(msg)->session;
    Connection *connection = m_session_to_connection[session];
    assert(connection);

    if (!connection->is_connected()) {
        return false;
    }

    IPX_CTX_DEBUG(m_log_ctx, "Forwarding message to %s\n", m_ident.c_str());

    try {
        connection->forward_message(msg);

    } catch (const ConnectionError &err) {
        IPX_CTX_INFO(m_log_ctx, "Planning reconnection to %s", m_ident.c_str());
        m_reconnects.push_back(connection);
        return false;
    }

    return true;
}

size_t
Host::process_reconnects()
{
    for (auto it = m_reconnects.begin(); it != m_reconnects.end(); ) {
        Connection *connection = *it;

        try {
            connection->connect();
            IPX_CTX_WARNING(m_log_ctx, "A connection to %s reconnected", m_ident.c_str());
            it = m_reconnects.erase(it);

        } catch (const ConnectionError &err) {
            it++;
        }
    }

    return m_reconnects.size();
}

size_t
Host::advance_transfers()
{
    size_t waiting_transfers_cnt = 0;

    for (auto it = m_connections.begin(); it != m_connections.end(); ) {
        Connection *connection = it->get();

        if (!connection->is_connected()) {
            it++;
            continue;
        }

        try {
            connection->advance_transfers();

        } catch (const ConnectionError &err) {
            IPX_CTX_INFO(m_log_ctx, "Planning reconnection to %s", m_ident.c_str());
            m_reconnects.push_back(connection);
        }

        // Remove finished connection
        if (connection->m_finishing_flag && connection->waiting_transfers_cnt() == 0) {
            assert(std::find(m_reconnects.begin(), m_reconnects.end(), connection) == m_reconnects.end());

            it = m_connections.erase(it);
            continue;
        }

        waiting_transfers_cnt += connection->waiting_transfers_cnt();

        it++;
    }

    return waiting_transfers_cnt;
}

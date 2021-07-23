/**
 * \file src/plugins/output/forwarder/src/Forwarder.cpp
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * \brief Forwarder class implementation
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

#include "Forwarder.h"

Forwarder::Forwarder(Config config, ipx_ctx_t *log_ctx) :
    m_config(config),
    m_log_ctx(log_ctx)
{
    // Set up hosts
    for (const auto &host_config : m_config.hosts) {
        auto con_params = ConnectionParams{host_config.address, host_config.port, m_config.protocol};
        auto host = std::unique_ptr<Host>(
            new Host(host_config.name, std::move(con_params), m_log_ctx,
                     m_config.tmplts_resend_pkts, m_config.tmplts_resend_secs)
        );
        m_hosts.push_back(std::move(host));
    }
}

void Forwarder::handle_session_message(ipx_msg_session_t *msg)
{
    const ipx_session *session = ipx_msg_session_get_session(msg);

    switch (ipx_msg_session_get_event(msg)) {
    case IPX_MSG_SESSION_OPEN:
        IPX_CTX_DEBUG(m_log_ctx, "New session %s", session->ident);
        for (auto &host : m_hosts) {
            host->setup_connection(session);
        }
        break;

    case IPX_MSG_SESSION_CLOSE:
        IPX_CTX_DEBUG(m_log_ctx, "Closing session %s", session->ident);
        for (auto &host : m_hosts) {
            host->finish_connection(session);
        }
        break;
    }
}

void Forwarder::handle_ipfix_message(ipx_msg_ipfix_t *msg)
{
    // Process reconnects
    time_t now = get_monotonic_time();

    if (now - m_last_reconnect_check >= m_config.reconnect_secs) {
        for (auto &host : m_hosts) {
            host->process_reconnects();
        }

        m_last_reconnect_check = now;
    }

    // Advance waiting transfers
    for (auto &host : m_hosts) {
        host->advance_transfers();
    }

    // Forward message
    switch (m_config.forward_mode) {
    case ForwardMode::SENDTOALL:
        forward_to_all(msg);
        break;

    case ForwardMode::ROUNDROBIN:
        forward_round_robin(msg);
        break;

    default: assert(0);
    }
}

void Forwarder::finalize()
{
    IPX_CTX_INFO(m_log_ctx, "Forwarder is finishing...");

    time_t start = get_monotonic_time();
    time_t then = 0;

    for (;;) {
        time_t now = get_monotonic_time();

        // Try reconnects every second now
        if (now - then >= 1) {
            for (auto &host : m_hosts) {
                host->process_reconnects();
            }

            then = now;
        }

        // Advance transfers and keep track of how many are still waiting
        size_t waiting_transfers_cnt = 0;

        for (auto &host : m_hosts) {
            waiting_transfers_cnt += host->advance_transfers();
        }

        // If there are no more transfers or it's taking too long, finish
        if (waiting_transfers_cnt == 0 || now - start >= MAX_FINALIZE_WAIT_SECS) {

            if (waiting_transfers_cnt > 0) {
                IPX_CTX_WARNING(m_log_ctx, "Maximum finalize time reached, %lu transfers will be dropped!",
                                waiting_transfers_cnt);
            }

            break;
        }
    }

    IPX_CTX_INFO(m_log_ctx, "Forwarder finished");
}

void
Forwarder::forward_to_all(ipx_msg_ipfix_t *msg)
{
    for (auto &host : m_hosts) {
        host->forward_message(msg);
    }
}

void
Forwarder::forward_round_robin(ipx_msg_ipfix_t *msg)
{
    bool ok = false;

    for (size_t i = 0; i < m_hosts.size(); i++) {
        auto &host = m_hosts[m_rr_index];
        ok = host->forward_message(msg);

        m_rr_index = (m_rr_index + 1) % m_hosts.size();

        if (ok) {
            break;
        }
    }

    if (!ok) {
        IPX_CTX_WARNING(m_log_ctx, "Couldn't forward to any of the hosts, dropping message!");
    }
}

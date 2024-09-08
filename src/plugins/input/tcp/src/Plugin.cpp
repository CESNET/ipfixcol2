/**
 * \file
 * \author Jakub Antonín Štigler <xstigl00@stud.fit.vutbr.cz>
 * \brief Tcp input plugin for ipfixcol2 (source file)
 * \date 2024
 *
 * Copyright: (C) 2023 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "Plugin.hpp"

#include <array>     // array
#include <stdexcept> // exception
#include <cstddef>   // size_t

#include <ipfixcol2.h> // ipx_ctx_t, IPX_CTX_ERROR, IPX_CTX_INFO, IPX_CTX_WARNING, ipx_session

#include "Config.hpp"         // Config
#include "DecoderFactory.hpp" // DecoderFactory
#include "Connection.hpp"     // Connection

namespace tcp_in {

Plugin::Plugin(ipx_ctx_t *ctx, Config &config) :
    m_ctx(ctx),
    m_clients(ctx, DecoderFactory()),
    m_acceptor(m_clients, ctx)
{
    m_acceptor.bind_addresses(config);
    m_acceptor.start();
}

void Plugin::get() {
    /** Maximum number of connections to process in one call to get */
    constexpr int MAX_CONNECTION_BATCH_SIZE = 16;
    std::array<Connection *, MAX_CONNECTION_BATCH_SIZE> connections{};

    auto count = m_clients.wait_for_connections(connections.begin(), connections.size());

    for (size_t i = 0; i < count; ++i) {
        try {
            if (!connections[i]->receive(m_ctx)) {
                // EOF reached
                auto session = connections[i]->get_session();
                IPX_CTX_INFO(m_ctx, "Closing %s", session->ident);
                m_clients.close_connection(session);
            }
        } catch (std::exception &ex) {
            IPX_CTX_ERROR(m_ctx, "%s", ex.what());
            auto session = connections[i]->get_session();
            IPX_CTX_INFO(m_ctx, "Closing %s", session->ident);
            m_clients.close_connection(session);
        }
    }
}

void Plugin::close_session(const ipx_session *session) noexcept {
    m_clients.close_connection(session);
}

Plugin::~Plugin() {
    try {
        m_acceptor.stop();
        m_clients.close_all_connections();
    } catch (std::exception &ex) {
        IPX_CTX_WARNING(m_ctx, "%s", ex.what());
    }
}

} // namespace tcp_in


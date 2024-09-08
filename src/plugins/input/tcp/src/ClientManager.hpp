/**
 * \file
 * \author Jakub Antonín Štigler <xstigl00@stud.fit.vutbr.cz>
 * \brief Manages TCP connection (header file)
 * \date 2024
 *
 * Copyright: (C) 2023 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <vector>  // std::vector
#include <memory>  // std::unique_ptr
#include <mutex>   // std::mutex
#include <cstddef> // size_t

#include <ipfixcol2.h> // ipx_session, ipx_ctx_t

#include "Connection.hpp" // Connection
#include "DecoderFactory.hpp"    // Decoder
#include "UniqueFd.hpp"   // UniqueFd
#include "Epoll.hpp"      // Epoll

namespace tcp_in {

/** Manager for TCP connections */
class ClientManager {
public:
    /**
     * @brief Creates client manager with no clients.
     * @throws when fails to create epoll
     */
    ClientManager(ipx_ctx_t *ctx, DecoderFactory factory);

    /**
     * @brief Adds connection to the vector and epoll.
     * @param fd file descriptor of the new tcp connection.
     * @throws when fails to add the new connection to epoll or when fails to create new session.
     */
    void add_connection(UniqueFd fd);

    /**
     * @brief Removes connection from the vector based on its session. This is safe only for the
     * main thread (not the acceptor thread).
     * @param ctx Context used for closing sessions.
     * @param session session of the connection to remove.
     */
    void close_connection(const ipx_session *session);

    /**
     * @brief Waits for new data on at least one connection and gets all the connections with new
     * data.
     * @param connectons Where to put the connections.
     * @param max_connections Max number of connections to set to `connections`
     * @return number of connections set to `connectinos`
     * @throws when fails to wait
     */
    size_t wait_for_connections(Connection **connections, int max_connections);

    /** Closes all connections. */
    void close_all_connections();
private:
    /** Closes connection at the given index. DOES NOT SYNCHRONIZE */
    void close_connection_internal(size_t connection_idx) noexcept;

    ipx_ctx_t *m_ctx;
    Epoll m_epoll;
    std::mutex m_mutex;
    std::vector<std::unique_ptr<Connection>> m_connections;
    DecoderFactory m_factory;
};

} // namespace tcp_in


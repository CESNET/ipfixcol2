/**
 * \file
 * \author Jakub Antonín Štigler <xstigl00@stud.fit.vutbr.cz>
 * \brief Acceptor thread for TCP clients (header file)
 * \date 2024
 *
 * Copyright: (C) 2023 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <thread>  // std::thread
#include <vector>  // std::vector
#include <cstdint> // uint16_t

#include <ipfixcol2.h> // ipx_ctx_t

#include "ClientManager.hpp"  // ClientManager
#include "DecoderFactory.hpp" // DecoderFactory
#include "Config.hpp"         // Config
#include "IpAddress.hpp"      // IpAddress
#include "UniqueFd.hpp"       // UniqueFd
#include "Epoll.hpp"          // Epoll

namespace tcp_in {

/** Acceptor thread for TCP clients. */
class Acceptor {
public:
    /**
     * @brief Creates the acceptor thread.
     *
     * @param clients Reference to client manager.
     * @param config File configuration.
     * @param ctx The plugin context.
     */
    Acceptor(ClientManager &clients, ipx_ctx_t *ctx);

    // force that acceptor stays in its original memory (so that `this` pointer stays valid on the
    // other thread)
    Acceptor(const Acceptor &) = delete;
    Acceptor(Acceptor &&) = delete;

    /**
     * @brief Creates sockets for each address. If there are no addresses, listens on all
     * interfaces.
     * @param config Configuration with the addresses and port.
     */
    void bind_addresses(Config &config);

    /**
     * @brief Starts the acceptor thread.
     * @throws When the acceptor thread is already started.
     */
    void start();

    /** Stops the acceptor thread. */
    void stop();

private:
    void add_address(IpAddress &adr, uint16_t port, bool ipv6_only);

    UniqueFd bind_address(IpAddress &adr, uint16_t port, bool ipv6_only);

    /** Runs on the other therad */
    void mainloop();

    /** File descriptor of epoll for accepting connections. */
    Epoll m_epoll;
    /** Sockets listened to by epoll. */
    std::vector<UniqueFd> m_sockets;

    /** Write 'x' to this to gracefully exit the thread. */
    UniqueFd m_pipe_in;
    /** Epoll listens to this, when it activates the acceptor thread will gracefuly exit. */
    UniqueFd m_pipe_out;

    /** Accepted clients. */
    ClientManager &m_clients;
    std::thread m_thread;
    ipx_ctx_t *m_ctx;
};

} // namespace tcp_in

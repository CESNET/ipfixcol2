/**
 * \file
 * \author Jakub Antonín Štigler <xstigl00@stud.fit.vutbr.cz>
 * \brief Tcp input plugin for ipfixcol2 (header file)
 * \date 2024
 *
 * Copyright: (C) 2023 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <ipfixcol2.h> // ipx_ctx_t, ipx_session

#include "Config.hpp"        // Config
#include "ClientManager.hpp" // ClientManager
#include "Acceptor.hpp"      // Acceptor

namespace tcp_in {

/** TCP input plugin for ipfixcol2. */
class Plugin {
public:
    /**
     * @brief Creates new TCP plugin instance.
     * @param[in] ctx
     * @param[in] config configuration of the plugin
     * @throws when fails to init acceptor
     * @throws when fails to bind to addresses from config
     */
    Plugin(ipx_ctx_t *ctx, Config &config);

    // force that Plugin stays in its original memory (so that reference to `m_clients` in acceptor
    // stays valid)
    Plugin(const Plugin &) = delete;
    Plugin(Plugin &&) = delete;

    /**
     * @brief Wait for the next tcp message and process all received messages.
     * @throws when fails to wait for connections
     * @throws when fails to receive
     */
    void get();

    /**
     * @brief Close the given session.
     * @param session Session to close.
     */
    void close_session(const ipx_session *session) noexcept;

    ~Plugin();
private:
    ipx_ctx_t *m_ctx;
    ClientManager m_clients;
    /** Acceptor thread. */
    Acceptor m_acceptor;
};

} // namespace tcp_in


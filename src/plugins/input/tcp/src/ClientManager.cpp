/**
 * \file
 * \author Jakub Antonín Štigler <xstigl00@stud.fit.vutbr.cz>
 * \brief Manages TCP connection (source file)
 * \date 2024
 *
 * Copyright: (C) 2023 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "ClientManager.hpp"

#include <stdexcept> // runtime_error, exception
#include <string>    // string
#include <memory>    // unique_ptr
#include <cerrno>    // errno, EINTR
#include <array>     // array
#include <cstddef>   // size_t
#include <mutex>     // mutex, lock_guard
#include <vector>    // vector

#include <fcntl.h>      // fcntl, F_GETFL, F_SETFL, O_NONBLOCK
#include <netinet/in.h> // INET6_ADDRSTRLEN

#include <ipfixcol2.h> // ipx_strerror, ipx_ctx_t, ipx_session, IPX_CTX_INFO, IPX_CTX_WARNING

#include "Connection.hpp" // Connection
#include "UniqueFd.hpp"   // UniqueFd

namespace tcp_in {

ClientManager::ClientManager(ipx_ctx_t *ctx, DecoderFactory factory) :
    m_ctx(ctx),
    m_epoll(),
    m_mutex(),
    m_connections(),
    m_factory(std::move(factory))
{}

void ClientManager::add_connection(UniqueFd fd) {
    const char *err_str;

    // get the flags and set it to non-blocking mode
    int flags = fcntl(fd.get(), F_GETFL, 0);
    if (flags == -1) {
        ipx_strerror(errno, err_str);
        throw std::runtime_error(
            "Failed to get flags from file descriptor: " + std::string(err_str)
        );
    }

    flags |= O_NONBLOCK;
    if (fcntl(fd.get(), F_SETFL, flags) == -1) {
        ipx_strerror(errno, err_str);
        throw std::runtime_error("Failed to set non-blocking mode: " + std::string(err_str));
    }

    int borrowed_fd = fd.get();

    std::unique_ptr<Connection> connection(new Connection(std::move(fd), m_factory, m_ctx));

    auto net = &connection->get_session()->tcp.net;
    std::array<char, INET6_ADDRSTRLEN> src_addr_str{};
    inet_ntop(net->l3_proto, &net->addr_src, src_addr_str.begin(), src_addr_str.size());
    IPX_CTX_INFO(m_ctx, "New exporter connected from '%s'.", src_addr_str.begin());

    std::lock_guard<std::mutex> lock(m_mutex);

    auto con_ptr = connection.get();
    m_connections.push_back(std::move(connection));

    m_epoll.add(borrowed_fd, con_ptr);
}

void ClientManager::close_connection(const ipx_session *session) {
    std::lock_guard<std::mutex> lock(m_mutex);

    size_t i;
    for (i = 0; i < m_connections.size(); ++i) {
        if (m_connections[i]->get_session() == session) {
            break;
        }
    }

    if (i == m_connections.size()) {
        return;
    }

    close_connection_internal(i);
}


size_t ClientManager::wait_for_connections(Connection **connections, int max_connections) {
    // timeout for waiting for new data (in milliseconds, -1 => infinite)
    constexpr int GETTER_TIMEOUT = 10;

    std::vector<epoll_event> events(max_connections);

    int ev_valid = m_epoll.wait(events.data(), max_connections, GETTER_TIMEOUT);
    if (ev_valid == -1) {
        const char *err_str;
        ipx_strerror(errno, err_str);
        throw std::runtime_error("Failed to wait for new data: " + std::string(err_str));
    }

    events.resize(ev_valid);

    for (size_t i = 0; i < events.size(); ++i) {
        connections[i] = reinterpret_cast<Connection *>(events[i].data.ptr);
    }

    return events.size();
}

void ClientManager::close_all_connections() {
    std::lock_guard<std::mutex> lock(m_mutex);

    while (m_connections.size() != 0) {
        close_connection_internal(m_connections.size() - 1);
    }
}

void ClientManager::close_connection_internal(
    size_t connection_idx
) noexcept {
    if (connection_idx != m_connections.size() - 1) {
        m_connections[connection_idx].swap(m_connections[m_connections.size() - 1]);
    }

    auto con = m_connections[m_connections.size() - 1].get();

    if (!m_epoll.remove(con->get_fd())) {
        const char *err_str;
        ipx_strerror(errno, err_str);
        IPX_CTX_WARNING(
            m_ctx,
            "Failed to deregister the session %s from epoll: %s",
            con->get_session()->ident,
            err_str
        )
    }

    try {
        con->close(m_ctx);
    } catch (std::exception &ex) {
        IPX_CTX_WARNING(m_ctx, "%s", ex.what());
    }

    m_connections.pop_back();
}

} // namespace tcp_in

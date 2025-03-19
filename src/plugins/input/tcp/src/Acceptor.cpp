/**
 * \file
 * \author Jakub Antonín Štigler <xstigl00@stud.fit.vutbr.cz>
 * \brief Acceptor thread for TCP clients (header file)
 * \date 2024
 *
 * Copyright: (C) 2023 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "Acceptor.hpp"

#include <array>     // array
#include <stdexcept> // runtime_error
#include <cerrno>    // errno, EINTR
#include <string>    // string
#include <cstdint>   // uint16_t
#include <cinttypes> // PRIu16
#include <cstddef>   // size_t

#include <unistd.h>     // pipe, write
#include <sys/socket.h> // AF_INET, AF_INET6, SOCK_STREAM, sockaddr, socket, setsockopt, SOL_SOCKET,
                        // SO_REUSEADDR, IPPROTO_IPV6, IPV6_V6ONLY, SOMAXCONN
#include <netinet/in.h> // INET6_ADDRSTRLEN, sockaddr_in, sockaddr_in6, inet_ntop, in6addr_any

#include <ipfixcol2.h> // ipx_ctx_t, ipx_strerror, IPX_CTX_WARNING

#include "ClientManager.hpp"  // ClientManager
#include "Config.hpp"         // Config
#include "UniqueFd.hpp"       // UniqueFd
#include "IpAddress.hpp"      // IpAddress, IpVersion

namespace tcp_in {

Acceptor::Acceptor(ClientManager &clients, ipx_ctx_t *ctx) :
    m_epoll(),
    m_sockets(),
    m_pipe_in(),
    m_pipe_out(),
    m_clients(clients),
    m_thread(),
    m_ctx(ctx)
{
    std::array<int, 2> pipe_fd{};
    if (pipe(pipe_fd.begin()) != 0) {
        const char *err_str;
        ipx_strerror(errno, err_str);
        throw std::runtime_error("Failed to create pipe: " + std::string(err_str));
    }

    UniqueFd pipe_in(pipe_fd[1]);
    UniqueFd pipe_out(pipe_fd[0]);
    m_pipe_in.swap(pipe_in);
    m_pipe_out.swap(pipe_out);

    m_epoll.add(m_pipe_out.get(), nullptr);
}

void Acceptor::bind_addresses(Config &config) {
    if (config.local_addrs.size() == 0) {
        IpAddress addr(in6addr_any);
        add_address(addr, config.local_port, false);
    }

    for (auto &addr : config.local_addrs) {
        add_address(addr, config.local_port, true);
    }
}

void Acceptor::add_address(IpAddress &adr, uint16_t port, bool ipv6_only) {
    auto sd = bind_address(adr, port, ipv6_only);
    m_epoll.add(sd.get(), nullptr);
    m_sockets.push_back(std::move(sd));
}

UniqueFd Acceptor::bind_address(IpAddress &addr, uint16_t port, bool ipv6_only) {
    sockaddr_storage saddr_storage{};
    auto *saddr = reinterpret_cast<sockaddr *>(&saddr_storage);
    auto *v4 = reinterpret_cast<sockaddr_in *>(&saddr_storage);
    auto *v6 = reinterpret_cast<sockaddr_in6 *>(&saddr_storage);
    size_t addr_len;

    if (addr.version == IpVersion::IP4) {
        v4->sin_family = AF_INET;
        v4->sin_port = htons(port);
        v4->sin_addr = addr.v4;
        addr_len = sizeof(*v4);
    } else {
        v6->sin6_scope_id = 0;
        v6->sin6_family = AF_INET6;
        v6->sin6_port = htons(port);
        v6->sin6_addr = addr.v6;
        addr_len = sizeof(*v6);
    }

    const char *err_str;

    UniqueFd sd(socket(saddr->sa_family, SOCK_STREAM, 0));
    if (!sd) {
        ipx_strerror(errno, err_str);
        throw std::runtime_error("Failed to create socket: " + std::string(err_str));
    }

    int on = 1;
    if (setsockopt(sd.get(), SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == -1) {
        ipx_strerror(errno, err_str);
        IPX_CTX_WARNING(
            m_ctx,
            "Cannot turn on socket reuse option. It may take a while before the port can be used "
            "again: %s",
            err_str
        );
    }

    if (addr.version == IpVersion::IP6) {
        int is_on = ipv6_only;
        if (setsockopt(sd.get(), IPPROTO_IPV6, IPV6_V6ONLY, &is_on, sizeof(is_on)) == -1) {
            ipx_strerror(errno, err_str);
            IPX_CTX_WARNING(
                m_ctx,
                "Failed to turn %s socket option IPV6_VONLY. Plugin may %s connections: %s",
                ipv6_only ? "on" : "off",
                ipv6_only ? "accept IPV6" : "not accept IPV4",
                err_str
            )
        }
    }

    std::array<char, INET6_ADDRSTRLEN> addr_str{};
    inet_ntop(static_cast<int>(addr.version), &addr.v6, addr_str.begin(), INET6_ADDRSTRLEN);

    if (bind(sd.get(), saddr, addr_len) == -1) {
        ipx_strerror(errno, err_str);
        throw std::runtime_error(
            "Failed to bind to socket (local IP: "
            + std::string(addr_str.begin())
            + ", port: "
            + std::to_string(port)
            + "): "
            + err_str
        );
    }

    if (listen(sd.get(), SOMAXCONN) == -1) {
        ipx_strerror(errno, err_str);
        throw std::runtime_error(
            "Failed to listen on a socket (local IP: "
            + std::string(addr_str.begin())
            + ", port: "
            + std::to_string(port)
            + "): "
            + err_str
        );
    }

    IPX_CTX_INFO(m_ctx, "Listening on %s (port %" PRIu16 ")", addr_str.begin(), port);
    return sd;
}

void Acceptor::start() {
    if (m_thread.joinable()) {
        throw std::runtime_error("Cannot start acceptor, it is already running.");
    }
    std::thread acceptor([=](){ mainloop(); });
    m_thread.swap(acceptor);
}

void Acceptor::stop() {
    if (!m_thread.joinable()) {
        return;
    }

    if (write(m_pipe_in.get(), "x", 1) != 1) {
        const char *err_str;
        ipx_strerror(errno, err_str);
        throw std::runtime_error(
            "Faidled to notify acceptor thread to exit by writing to pipe: "
                + std::string(err_str)
        );
    }

    m_thread.join();
}

void Acceptor::mainloop() {
    epoll_event ev;
    const char *err_str;

    while (true) {
        int ret = m_epoll.wait(&ev, 1);
        if (ret == -1) {
            ipx_strerror(errno, err_str);
            IPX_CTX_ERROR(m_ctx, "Acceptor: failed to wait for new connections: %s", err_str);
            return;
        }

        if (ret != 1) {
            continue;
        }

        if (ev.data.fd == m_pipe_out.get()) {
            char b;
            int ret = read(m_pipe_out.get(), &b, 1);
            if (ret == -1) {
                ipx_strerror(errno, err_str);
                IPX_CTX_ERROR(m_ctx, "Acceptor: Failed to read command from pipe: %s", err_str);
                return;
            }

            // the other end of pipe was closed
            if (ret == 0) {
                IPX_CTX_INFO(m_ctx, "Acceptor: Command pipe was closed. Exiting.");
                return;
            }

            // exit command was sent
            if (b == 'x') {
                IPX_CTX_INFO(m_ctx, "Acceptor: Exit command received. Exiting.");
                return;
            }

            IPX_CTX_WARNING(m_ctx, "Acceptor: Received unknown command: '%c'", b);
        }

        UniqueFd new_sd(accept(ev.data.fd, nullptr, nullptr));
        if (!new_sd) {
            ipx_strerror(errno, err_str);
            IPX_CTX_ERROR(m_ctx, "Acceptor: Failed to accept a new connection: ");
            continue;
        }

        try {
            m_clients.add_connection(std::move(new_sd));
        } catch (std::exception &ex) {
            IPX_CTX_ERROR(m_ctx, "Acceptor: %s", ex.what());
        }
    }
}

} // namespace tcp_in

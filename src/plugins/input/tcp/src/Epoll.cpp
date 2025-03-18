/**
 * \file
 * \author Jakub Antonín Štigler <xstigl00@stud.fit.vutbr.cz>
 * \brief C++ wrapper around epoll (source file)
 * \date 2024
 *
 * Copyright: (C) 2023 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "Epoll.hpp"

#include <stdexcept> // runtime_error
#include <cerrno>    // errno, EINTR
#include <string>    // string

#include <ipfixcol2.h> // ipx_strerror

#include "UniqueFd.hpp" // UniqueFd

namespace tcp_in {

Epoll::Epoll() : m_fd(epoll_create(1)) {
    if (!m_fd) {
        const char *err_str;
        ipx_strerror(errno, err_str);
        throw std::runtime_error("Failed to create epoll: " + std::string(err_str));
    }
}

void Epoll::add(int fd, void *data) {
    epoll_event ev;
    ev.events = EPOLLIN;
    if (data) {
        ev.data.ptr = data;
    } else {
        ev.data.fd = fd;
    }
    if (epoll_ctl(m_fd.get(), EPOLL_CTL_ADD, fd, &ev) == -1) {
        const char *err_str;
        ipx_strerror(errno, err_str);
        throw std::runtime_error("Failed to add to epoll: " + std::string(err_str));
    }
}

int Epoll::wait(epoll_event *events, int max_events, int timeout) {
    int ev_valid = epoll_wait(m_fd.get(), events, max_events, timeout);
    if (ev_valid == -1) {
        if (errno == EINTR) {
            return 0;
        }
    }

    return ev_valid;
}

} // namespace tcp_in

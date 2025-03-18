/**
 * \file
 * \author Jakub Antonín Štigler <xstigl00@stud.fit.vutbr.cz>
 * \brief C++ wrapper around epoll (header file)
 * \date 2024
 *
 * Copyright: (C) 2023 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <sys/epoll.h> // epoll_event
#undef close           // fix FreeBSD build: (tcp_in::Connection has own close(),
                       // but preprocesor changes it for epoll_shim_close())

#include "UniqueFd.hpp" // UniqueFd

namespace tcp_in {

class Epoll {
public:
    /**
     * @brief Craetes empty epoll.
     * @throws when fails to create epoll.
     */
    Epoll();

    /**
     * @brief Add file descriptor to the epoll.
     * @param fd File descriptor to add.
     * @param [in] data data asociated with the file descriptor, when null, descriptor is set as
     * data
     * @throws when fails to add to the epoll.
     */
    void add(int fd, void *data);

    /**
     * @brief Waits for any of the file descriptors in the epoll to be active.
     * @param[out] ev store all active descriptors here
     * @param max_events max number of events to store in `events`
     * @param timeout maximum time to wait in milliseconds, -1 for infinite wait
     * @returns Number of events written to `events`, negative on error (errno is set)
     */
    int wait(epoll_event *events, int max_events, int timeout = -1);

    /**
     * @brief Removes file descriptor from the epoll.
     * @param fd File descriptor to remove from the epoll.
     * @return true on success, otherwise false
     */
    bool remove(int fd) noexcept {
        return epoll_ctl(m_fd.get(), EPOLL_CTL_DEL, fd, nullptr) == 0;
    }

private:
    UniqueFd m_fd;
};

} // namespace tcp_in

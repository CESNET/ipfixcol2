/**
 * \file
 * \author Jakub Antonín Štigler <xstigl00@stud.fit.vutbr.cz>
 * \brief C++ file descriptor wrapper (header file)
 * \date 2024
 *
 * Copyright: (C) 2023 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <unistd.h> // close
#include <utility>  // std::swap

namespace tcp_in {

class UniqueFd {
public:
    static constexpr int INVALID_FD = -1;

    /** Creates invalid file descriptor */
    UniqueFd() : m_fd(INVALID_FD) {}

    /** Creates owned file descriptor from the given file descriptor */
    UniqueFd(int fd) : m_fd(fd) {}

    UniqueFd(const UniqueFd &) = delete;

    UniqueFd &operator=(const UniqueFd &) = delete;

    /** Takes the ownership of file descriptor from the given file descriptor */
    UniqueFd(UniqueFd &&other) : m_fd(other.m_fd) {
        other.m_fd = INVALID_FD;
    }

    /** Takes ownership of the other file descriptor. */
    UniqueFd &operator=(UniqueFd &&other) {
        swap(other);
        return *this;
    }

    /** gets the file descriptor */
    int get() const noexcept {
        return m_fd;
    }

    /** gets the file descriptor and releases the ownership */
    int release() noexcept {
        int fd = m_fd;
        m_fd = INVALID_FD;
        return fd;
    }

    /** swaps the file descriptors */
    void swap(UniqueFd &other) noexcept {
        std::swap(m_fd, other.m_fd);
    }

    void close() noexcept {
        if (m_fd != INVALID_FD) {
            ::close(m_fd);
            m_fd = INVALID_FD;
        }
    }

    ~UniqueFd() noexcept {
        close();
    }

    /** checks whether the file descriptor is valid */
    explicit operator bool() const noexcept {
        return m_fd != INVALID_FD;
    }

    bool operator ==(const UniqueFd &other) const {
        return m_fd == other.m_fd;
    }

private:
    int m_fd;
};

} // namespace tcp_in


/**
 * \file
 * \author Jakub Antonín Štigler <xstigl00@cesnet.cz>
 * \brief Generic reader function. (header file)
 * \date 2025
 *
 * Copyright: (C) 2023 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <cerrno>
#include <cstdint>
#include <functional>
#include <stdexcept>

#include <unistd.h>
#include <sys/socket.h>

#include <ipfixcol2.h>

/** Describes result of read operation. */
enum class ReadResult {
    /** Successufully read some data. */
    READ,
    /** Non blocking socket needs to wait for more data. */
    WAIT,
    /** Connection has ended. */
    END,
};

using Reader = std::function<ReadResult(std::uint8_t *data, std::size_t &length)>;

static inline Reader tcp_reader(int fd) {
    return [=](std::uint8_t *data, std::size_t &length) -> ReadResult {
        int res = recv(fd, data, length, 0);
        if (res == -1) {
            length = 0;
            int err = errno;
            if (err == EWOULDBLOCK || err == EAGAIN) {
                return ReadResult::WAIT;
            }
            const char *err_str;
            ipx_strerror(err, err_str);
            throw std::runtime_error("Failed to read from descruptor " + std::string(err_str));
        }

        length = std::size_t(res);
        return length == 0 ? ReadResult::END : ReadResult::READ;
    };
}

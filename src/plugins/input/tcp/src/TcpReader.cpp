/**
 * \file
 * \author Jakub Antonín Štigler <xstigl00@cesnet.cz>
 * \brief Reader implementation for TCP. (header file)
 * \date 2025
 *
 * Copyright: (C) 2023 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "TcpReader.hpp"

#include <cerrno>
#include <functional>
#include <stdexcept>

#include <unistd.h>
#include <sys/socket.h>

namespace tcp_in {

ReadResult TcpReader::read(std::uint8_t *data, std::size_t &length) {
    int res = recv(m_fd, data, length, 0);
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
}

} // namespace tcp_in


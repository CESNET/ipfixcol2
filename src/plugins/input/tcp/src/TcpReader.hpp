/**
 * \file
 * \author Jakub Antonín Štigler <xstigl00@cesnet.cz>
 * \brief Reader implementation for TCP. (header file)
 * \date 2025
 *
 * Copyright: (C) 2023 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "Reader.hpp"

namespace tcp_in {

/** Reader that reads from TCP. */
class TcpReader : public Reader {
public:
    /**
     * @brief Create new reader that reads from TCP.
     * @param fd File descriptor for TCP connection.
     */
    TcpReader(int fd) : m_fd(fd) {}

    TcpReader() = delete;

    ReadResult read(std::uint8_t *data, std::size_t &length) override;
private:
    int m_fd;
};

} // namespace tcp_in

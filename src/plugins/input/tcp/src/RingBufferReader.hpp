/**
 * \file
 * \author Jakub Antonín Štigler <xstigl00@cesnet.cz>
 * \brief Reader implementation for ring buffer (header file)
 * \date 2025
 *
 * Copyright: (C) 2023 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "Reader.hpp"

namespace tcp_in {

/** Reader that reads from ring buffer. */
class RingBufferReader : public Reader {
public:

    /**
     * @brief Creates reader that reads from circullar buffer. The reader will return WAIT when all
     * data is read.
     * @param buf Circullar buffer to read from.
     * @param buf_len Length of the circullar buffer.
     * @param data_len Length of data in the circullar buffer.
     * @param data_pos Position of first byte of data in the circullar buffer.
     */
    RingBufferReader(
        std::uint8_t *buf,
        std::size_t buf_len,
        std::size_t data_len,
        std::size_t data_pos
    );

    RingBufferReader() = delete;

    ReadResult read(std::uint8_t *data, std::size_t &length) override;

private:
    std::uint8_t *m_buf1;
    std::uint8_t *m_buf2;
    std::size_t m_len1;
    std::size_t m_len2;
};

} // namespace tcp_in


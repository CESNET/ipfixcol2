/**
 * \file
 * \author Jakub Antonín Štigler <xstigl00@cesnet.cz>
 * \brief Reader implementation for ring buffer (header file)
 * \date 2025
 *
 * Copyright: (C) 2023 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "RingBufferReader.hpp"

#include <algorithm>
#include <cstring>

namespace tcp_in {

RingBufferReader::RingBufferReader(
    std::uint8_t *buf,
    std::size_t buf_len,
    std::size_t data_len,
    std::size_t data_pos
) {
    if (data_pos + data_len > buf_len) {
        m_buf1 = buf + data_pos;
        m_len1 = buf_len - data_pos;
        m_buf2 = buf;
        m_len2 = data_len - m_len1;
    } else {
        m_buf1 = buf;
        m_len1 = 0;
        m_buf2 = buf + data_pos;
        m_len2 = data_len;
    }
}

ReadResult RingBufferReader::read(std::uint8_t *data, std::size_t &length) {
    if (m_len1 == 0 && m_len2 == 0) {
        return ReadResult::WAIT;
    }

    auto r1 = std::min(m_len1, length);
    std::memcpy(data, m_buf1, r1);
    m_buf1 += r1;
    m_len1 -= r1;

    auto r2 = std::min(m_len2, length - r1);
    std::memcpy(data + r1, m_buf2, r2);
    m_buf2 += r2;
    m_len2 += r2;

    length = r1 + r2;
    return ReadResult::READ;
}

} // namespace tcp_in


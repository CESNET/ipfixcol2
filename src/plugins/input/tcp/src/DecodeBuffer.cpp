/**
 * \file
 * \author Jakub Antonín Štigler <xstigl00@stud.fit.vutbr.cz>
 * \brief Buffer for managing decoded IPFIX data (source file)
 * \date 2024
 *
 * Copyright: (C) 2023 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "DecodeBuffer.hpp"

#include <cstdint>   // uint8_t
#include <algorithm> // copy_n
#include <stdexcept> // runtime_error
#include <cstddef>   // size_t

#include <ipfixcol2.h> // fds_ipfix_msg_hdr

namespace tcp_in {

void DecodeBuffer::read_from(const uint8_t *data, size_t size) {
    // read until there is something to read
    while (size) {
        // get the message size
        if (m_part_decoded.size() < sizeof(struct fds_ipfix_msg_hdr)) {
            auto new_data = read_header(data, size);
            size -= new_data - data;
            data = new_data;
        }

        if (size == 0) {
            return;
        }

        auto remaining = m_decoded_size - m_part_decoded.size();

        if (read_min(data, size, remaining) != remaining) {
            // all data is readed
            return;
        }

        data += remaining;
        size -= remaining;

        add(std::move(m_part_decoded));
        m_part_decoded = ByteVector();
    }
}

void DecodeBuffer::read_from(
    const uint8_t *data,
    size_t buffer_size,
    size_t data_size,
    size_t position
) {
    // read until wrap
    auto block = data + position;
    auto block_size = std::min(data_size, buffer_size - position);
    read_from(block, block_size);

    if (block_size == data_size) {
        // there was no need to wrap
        return;
    }

    // read the second block (after wrap)
    read_from(data, data_size - block_size);
}

void DecodeBuffer::signal_eof() {
    if (m_part_decoded.size() != 0) {
        throw std::runtime_error("Received incomplete message.");
    }
    m_eof_reached = true;
}

const uint8_t *DecodeBuffer::read_header(const uint8_t *data, size_t size) {
    constexpr size_t HDR_SIZE = sizeof(fds_ipfix_msg_hdr);

    auto filled = m_part_decoded.size();
    auto remaining = 0;
    if (filled < HDR_SIZE) {
        remaining = HDR_SIZE - filled;
        if (read_min(data, size, remaining) == size) {
            // not enough data for the whole header
            return data + size;
        }
    }

    auto hdr = reinterpret_cast<const fds_ipfix_msg_hdr *>(m_part_decoded.data());
    m_decoded_size = ntohs(hdr->length);

    m_part_decoded.reserve(m_decoded_size);
    return data + remaining;
}


size_t DecodeBuffer::read_min(const uint8_t *data, size_t size1, size_t size2) {
    auto size = std::min(size1, size2);
    auto filled = m_part_decoded.size();
    m_part_decoded.resize(filled + size);
    std::copy_n(data, size, m_part_decoded.data() + filled);
    m_total_bytes_decoded += size;
    return size;
}

} // namespace tcp_in


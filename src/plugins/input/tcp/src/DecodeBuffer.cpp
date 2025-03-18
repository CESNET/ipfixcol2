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
        if (!read_header(&data, &size)) {
            // There is not enough data to read the whole header.
            break;
        }

        // Read the body of the message.
        if (!read_body(&data, &size)) {
            // There is not enough data to read the whole body of the message.
            break;
        }
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

bool DecodeBuffer::read_header(const uint8_t **data, size_t *size) {
    if (m_decoded_size != 0) {
        // The header is already read, but the message body is incomplete.
        return true;
    }

    // Read the header.
    if (!read_until_n(sizeof(fds_ipfix_msg_hdr), data, size)) {
        // There is not enough data to read the whole header.
        return false;
    }

    // The header has been successfully read. Load the size of the message.
    auto hdr = reinterpret_cast<const fds_ipfix_msg_hdr *>(m_part_decoded.data());
    m_decoded_size = ntohs(hdr->length);

    if (m_decoded_size < sizeof(fds_ipfix_msg_hdr)) {
        throw std::runtime_error("Invalid IPFIX message header size.");
    }

    return true;
}

bool DecodeBuffer::read_body(const uint8_t **data, size_t *size) {
    // Read the body
    if (!read_until_n(m_decoded_size, data, size)) {
        // There is not enough data to read the whole body.
        return false;
    }

    m_decoded.push_back(std::move(m_part_decoded));
    m_part_decoded = ByteVector();
    m_decoded_size = 0;
    return true;
}

bool DecodeBuffer::read_until_n(size_t n, const uint8_t **data, size_t *data_len) {
    m_part_decoded.reserve(n);
    auto cnt = read_min(*data, n, *data_len);
    *data_len -= cnt;
    *data += cnt;
    return m_part_decoded.size() == n;
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


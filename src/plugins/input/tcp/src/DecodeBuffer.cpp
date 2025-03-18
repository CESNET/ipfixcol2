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

void DecodeBuffer::read_from(Reader &reader, std::size_t consume) {
    consume += m_total_bytes_decoded;
    while (!enough_data() || consume < m_total_bytes_decoded) {
        if (!read_header(reader)) {
            break;
        }

        if (!read_body(reader)) {
            break;
        }
    }
}

void DecodeBuffer::signal_eof() {
    if (m_part_decoded.size() != 0) {
        throw std::runtime_error("Received incomplete message.");
    }
    m_eof_reached = true;
}

bool DecodeBuffer::read_header(Reader &reader) {
    if (m_decoded_size != 0) {
        // The header is already read, but the message body is incomplete.
        return true;
    }

    // Read the header.
    if (!read_until_n(sizeof(fds_ipfix_msg_hdr), reader)) {
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

bool DecodeBuffer::read_body(Reader &reader) {
    // Read the body
    if (!read_until_n(m_decoded_size, reader)) {
        // There is not enough data to read the whole body.
        return false;
    }

    m_decoded.push_back(std::move(m_part_decoded));
    m_part_decoded = ByteVector();
    m_decoded_size = 0;
    return true;
}

bool DecodeBuffer::read_until_n(size_t n, Reader &reader) {
    m_total_bytes_decoded += reader.read_until_n(n, m_part_decoded, *this);
    return m_part_decoded.size() == n;
}

} // namespace tcp_in


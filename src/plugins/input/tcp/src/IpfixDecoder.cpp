/**
 * \file
 * \author Jakub Antonín Štigler <xstigl00@stud.fit.vutbr.cz>
 * \brief IPFIX decoder for tcp plugin (header file)
 * \date 2024
 *
 * Copyright: (C) 2023 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "IpfixDecoder.hpp"

#include <stdexcept>

#include "DecodeBuffer.hpp" // DecodeBuffer
#include "Reader.hpp"

namespace tcp_in {

void IpfixDecoder::progress() {
    // Directly calling `m_decoded.read_from` is somehow much slower than copying the code directly
    // here.

    while (!m_decoded.enough_data()) {
        if (!read_header()) {
            break;
        }

        if (!read_body()) {
            break;
        }
    }
}

bool IpfixDecoder::read_header() {
    if (m_decoded_size != 0) {
        return true;
    }

    if (!read_until_n(sizeof(fds_ipfix_msg_hdr))) {
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

bool IpfixDecoder::read_body() {
    // Read the body
    if (!read_until_n(m_decoded_size)) {
        // There is not enough data to read the whole body.
        return false;
    }

    m_decoded.add(std::move(m_part_decoded));
    m_part_decoded = ByteVector();
    m_decoded_size = 0;
    return true;
}

bool IpfixDecoder::read_until_n(size_t n) {
    m_reader.read_until_n(n, m_part_decoded, m_decoded);
    return m_part_decoded.size() == n;
}

} // namespace tcp_in


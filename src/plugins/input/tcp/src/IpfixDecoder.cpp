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

#include <stdexcept> // runtime_error
#include <string>    // string
#include <errno.h>   // errno, EWOULDBLOCK, EAGAIN
#include <stddef.h>  // size_t

#include <unistd.h> // read

#include <ipfixcol2.h> // fds_ipfix_msg_hdr, ipx_strerror

#include "DecodeBuffer.hpp" // DecodeBuffer
#include "read_until_n.hpp" // read_until_n

namespace tcp_in {

DecodeBuffer &IpfixDecoder::decode() {
    while (!m_decoded.enough_data()) {
        // Read the header of the message.
        if (!read_header()) {
            // There is not enough data to read the whole header.
            break;
        }

        // Read the body of the message.
        if (!read_body()) {
            // There is not enough data to read the whole body of the message.
            break;
        }
    }

    if (m_decoded.is_eof_reached() && m_part_readed.size() != 0) {
        throw std::runtime_error("Received incomplete message.");
    }

    return m_decoded;
}

bool IpfixDecoder::read_header() {
    if (m_msg_size != 0) {
        // The header is already read, but the message body is incomplete.
        return true;
    }

    // Read the header
    if (!read_until_n(sizeof(fds_ipfix_msg_hdr))) {
        // There is not enough data to read the whole header.
        return false;
    }

    // The header has been read, load the size of the message.
    auto hdr = reinterpret_cast<fds_ipfix_msg_hdr *>(m_part_readed.data());
    m_msg_size = ntohs(hdr->length);
    return true;
}

bool IpfixDecoder::read_body() {
    // Read the body.
    if (!read_until_n(m_msg_size)) {
        // There is not enough data to read the whole body
        return false;
    }

    // Whole message has been read. Add it to the decode buffer.
    m_decoded.add(std::move(m_part_readed));
    m_part_readed = ByteVector();
    m_msg_size = 0;
    return true;
}

bool IpfixDecoder::read_until_n(size_t n) {
    return ::read_until_n(n, m_fd, m_part_readed, m_decoded);
}

} // namespace tcp_in


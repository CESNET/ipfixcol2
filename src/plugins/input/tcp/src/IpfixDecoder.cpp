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

using namespace std;

DecodeBuffer &IpfixDecoder::decode() {
    while (!m_decoded.enough_data()) {
        if (!read_header()) {
            break;
        }

        if (!read_body()) {
            break;
        }
    }

    if (m_decoded.is_eof_reached() && m_part_readed.size() != 0) {
        throw runtime_error("Received incomplete message.");
    }

    return m_decoded;
}

bool IpfixDecoder::read_header() {
    if (m_msg_size != 0) {
        return true;
    }

    if (!read_until_n(sizeof(fds_ipfix_msg_hdr))) {
        return false;
    }

    auto hdr = reinterpret_cast<fds_ipfix_msg_hdr *>(m_part_readed.data());
    m_msg_size = ntohs(hdr->length);
    return true;
}

bool IpfixDecoder::read_body() {
    if (!read_until_n(m_msg_size)) {
        return false;
    }

    m_decoded.add(m_part_readed);
    m_msg_size = 0;
    return true;
}

bool IpfixDecoder::read_until_n(size_t n) {
    return ::read_until_n(n, m_fd, m_part_readed, m_decoded);
}

} // namespace tcp_in


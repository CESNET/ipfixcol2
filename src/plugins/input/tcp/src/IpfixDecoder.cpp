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

#include "DecodeBuffer.hpp" // DecodeBuffer
#include "Reader.hpp"

namespace tcp_in {

DecodeBuffer &IpfixDecoder::decode() {
    auto reader = tcp_reader(m_fd);
    m_decoded.read_from(reader);
    return m_decoded;
}

} // namespace tcp_in


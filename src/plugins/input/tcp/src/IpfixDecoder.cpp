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

void IpfixDecoder::progress() {
    m_decoded.read_from(m_reader);
}

} // namespace tcp_in


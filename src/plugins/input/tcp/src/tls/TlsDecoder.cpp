/**
 * \file
 * \author Jakub Antonín Štigler <xstigl00@cesnet.cz>
 * \brief TLS decoder for IPFIX plugin. (source file)
 * \date 2025
 *
 * Copyright: (C) 2023 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "TlsDecoder.hpp"

#include "../Reader.hpp"

namespace tcp_in {
namespace tls {

TlsDecoder::TlsDecoder(SslCtx &ctx, int fd) :
    m_ssl(ctx.create_ssl())
{
    SslBio bio(BIO_s_socket());
    bio.set_fd(fd);
    m_ssl.set_bio(std::move(bio));

    m_handshake_complete = m_ssl.accept();
}

DecodeBuffer &TlsDecoder::decode() {
    if (!m_handshake_complete) {
        m_handshake_complete = m_ssl.accept();
        if (!m_handshake_complete) {
            return m_decoded;
        }
    }

    Reader reader = [&](std::uint8_t *data, std::size_t &length) -> ReadResult {
        return m_ssl.read_ex(data, length);
    };
    m_decoded.read_from(reader);

    return m_decoded;
}

} // namespace tls
} // namespace tcp_in

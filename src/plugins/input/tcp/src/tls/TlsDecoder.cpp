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

namespace tcp_in {
namespace tls {

// Size of buffer for copying data from OpenSSL to DecodeBuffer.
constexpr std::size_t BUFFER_SIZE = 4096;

TlsDecoder::TlsDecoder(SslCtx &ctx, int fd) :
    m_ssl(ctx.create_ssl()),
    m_buffer(BUFFER_SIZE)
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

    // XXX: The copy to intermidiate `m_buffer` can be avoided by extending `DecodeBuffer` to be
    //      able to read with generic read function.

    while (!m_decoded.enough_data()) {
        auto res = m_ssl.read_ex(m_buffer);
        m_decoded.read_from(m_buffer.data(), m_buffer.size());
        switch (res) {
            case ReadResult::READ:
                break;
            case ReadResult::WAIT:
                return m_decoded;
            case ReadResult::FINISHED:
                // FIXME: properly check that the shutdown is complete.
                m_ssl.shutdown();
                // Intentional falltrough
            case ReadResult::CLOSED:
                m_decoded.signal_eof();
                return m_decoded;
        }
    }

    return m_decoded;
}

} // namespace tls
} // namespace tcp_in

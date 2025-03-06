/**
 * \file
 * \author Jakub Antonín Štigler <xstigl00@cesnet.cz>
 * \brief Factory for tls connections. (source file)
 * \date 2025
 *
 * Copyright: (C) 2023 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "DecoderFactory.hpp"

#include "TlsDecoder.hpp"

// OpenSSL v1.1.1 compatibility
#ifndef SSL_OP_IGNORE_UNEXPECTED_EOF
#define SSL_OP_IGNORE_UNEXPECTED_EOF 0
#endif // ifndef SSL_OP_IGNORE_UNEXPECTED_EOF

namespace tcp_in {
namespace tls {

DecoderFactory::DecoderFactory(const std::string &cert_path) : m_ctx(TLS_server_method()) {
    // Cache configuration
    constexpr std::chrono::seconds CACHE_TIMEOUT(3600);
    const std::string cache_id = "ipfixcol2";
    constexpr std::size_t CACHE_SIZE = 1024;

    m_ctx.set_min_proto_version(TLS1_2_VERSION);
    m_ctx.set_options(
        SSL_OP_IGNORE_UNEXPECTED_EOF | SSL_OP_NO_RENEGOTIATION | SSL_OP_CIPHER_SERVER_PREFERENCE
    );

    m_ctx.use_certificate_chain_file(cert_path.c_str());
    m_ctx.use_private_key_file(cert_path.c_str(), SSL_FILETYPE_PEM);

    m_ctx.set_session_id_context(cache_id.data(), unsigned(cache_id.size()));
    m_ctx.set_session_cache_mode(SSL_SESS_CACHE_SERVER);
    m_ctx.sess_set_cache_size(CACHE_SIZE);
    m_ctx.sess_set_timeout(CACHE_TIMEOUT);

    // Don't verify peer. Servers usually don't verify clients, but maybe we want to verify clients?
    m_ctx.set_verify(SSL_VERIFY_NONE);
}

std::unique_ptr<Decoder> DecoderFactory::create(int fd) {
    return std::unique_ptr<Decoder>(new TlsDecoder(m_ctx, fd));
}

} // namespace tls
} // namespace tcp_in

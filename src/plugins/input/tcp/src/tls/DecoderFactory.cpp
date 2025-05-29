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

#include "../Config.hpp"
#include "TlsDecoder.hpp"

// OpenSSL v1.1.1 compatibility.
// The flag SSL_OP_IGNORE_UNEXPECTED_EOF is not supported in v1.1.1. This will define it as flag
// without effect if it is not supported so that it may be used in any case.
// The flag is not neccesary, but with it if peer closes the connection without sending proper TLS
// message, it will not be treated as error. This is OK because can check if IPFIX messages are
// complete.
#ifndef SSL_OP_IGNORE_UNEXPECTED_EOF
#define SSL_OP_IGNORE_UNEXPECTED_EOF 0
#endif // ifndef SSL_OP_IGNORE_UNEXPECTED_EOF

namespace tcp_in {
namespace tls {

/**
 * @brief Load certificate authority based on the configuration.
 * @param conf Configuration of what should be load as certificate authority.
 * @param ctx Context to which will the ca load.
 */
static void load_ca(const Config &conf, SslCtx &ctx);

DecoderFactory::DecoderFactory(const Config &conf) : m_ctx(TLS_server_method()) {
    // Cache configuration. Cache is used to speed up initial handshake for clients that were
    // recently connected.
    constexpr long CACHE_TIMEOUT_SECONDS = 3600;
    const std::string cache_id = "ipfixcol2";
    constexpr std::size_t CACHE_SIZE = 1024;

    m_ctx.set_min_proto_version(TLS1_2_VERSION);
    m_ctx.set_options(
        SSL_OP_IGNORE_UNEXPECTED_EOF | SSL_OP_NO_RENEGOTIATION | SSL_OP_CIPHER_SERVER_PREFERENCE
    );

    sleep(1);

    m_ctx.use_certificate_chain_file(conf.certificate_file.c_str());
    m_ctx.use_private_key_file(conf.private_key_file.c_str(), SSL_FILETYPE_PEM);

    m_ctx.set_session_id_context(cache_id.data(), unsigned(cache_id.size()));
    m_ctx.set_session_cache_mode(SSL_SESS_CACHE_SERVER);
    m_ctx.sess_set_cache_size(CACHE_SIZE);
    m_ctx.sess_set_timeout(CACHE_TIMEOUT_SECONDS);

    if (conf.verify_peer) {
        m_ctx.set_verify(SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT);
        load_ca(conf, m_ctx);
    } else {
        m_ctx.set_verify(SSL_VERIFY_NONE);
    }
}

std::unique_ptr<Decoder> DecoderFactory::create(int fd) {
    return std::unique_ptr<Decoder>(new TlsDecoder(m_ctx, fd));
}

static void load_ca(const Config &conf, SslCtx &ctx) {
    if (conf.use_default_ca) {
        ctx.set_default_verify_paths();
        return;
    }

    const char *ca_file = nullptr;
    const char *ca_dir = nullptr;

    if (conf.default_ca_file) {
        ctx.set_default_verify_file();
    } else if (!conf.ca_file.empty()) {
        ca_file = conf.ca_file.c_str();
    }

    if (conf.default_ca_dir) {
        ctx.set_default_verify_dir();
    } else if (!conf.ca_dir.empty()) {
        ca_dir = conf.ca_dir.c_str();
    }

    if (ca_file != nullptr || ca_dir != nullptr) {
        ctx.load_verify_locations(ca_file, ca_dir);
    }

// Supported only from OpenSSL  3.0.0-0 release
#if OPENSSL_VERSION_NUMBER >= 0x03000000f
    if (conf.default_ca_store) {
        ctx.set_default_verify_store();
    } else if (!conf.ca_store.empty()) {
        ctx.load_verify_store(conf.ca_store.c_str());
    }
#endif // OPENSSL_VERSION_NUMBER >= 0x03000000f
}

} // namespace tls
} // namespace tcp_in

/**
 * \file
 * \author Jakub Antonín Štigler <xstigl00@cesnet.cz>
 * \brief Wrapper around SSL_CTX from OpenSSL. (header file)
 * \date 2025
 *
 * Copyright: (C) 2023 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <array>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>

#include <openssl/err.h>
#include <openssl/ssl.h>

#include "Ssl.hpp"
#include "throw_ssl_err.hpp"

namespace tcp_in {
namespace tls {

namespace dealloc {

/** Deallocator for `SSL_CTX` from OpenSSL. Can be used for example in `std::unique_ptr` */
class SslCtx {
public:
    void operator()(SSL_CTX *ptr) { SSL_CTX_free(ptr); }
};

} // namespace dealloc

/** Wrapper around `SSL_CTX` from OpenSSL. */
class SslCtx : public std::unique_ptr<SSL_CTX, dealloc::SslCtx> {
public:
    /** Unique pointer of `SSL_CTX`. This is base class for `SslCtx`. */
    using SelfPtr = std::unique_ptr<SSL_CTX, dealloc::SslCtx>;

    /**
     * @brief Construct this with the given method.
     * @param method Method for creating `SSL_CTX`. Can be obtained for example with
     * `TLS_server_method()`.
     * @throws On failure.
     */
    SslCtx(const SSL_METHOD *method) : SelfPtr(SSL_CTX_new(method)) {
        if (!get()) {
            throw_ssl_err("Failed to create ssl context.");
        }
    }

    /**
     * @brief Set the verify mode. (What should be verified about peer)
     * @param mode Verify mode (e.g. SSL_VERIFY_NONE).
     * @param cb Verify callback.
     */
    void set_verify(int mode, SSL_verify_cb cb = nullptr) noexcept {
        SSL_CTX_set_verify(get(), mode, cb);
    }

    /**
     * @brief Sets the paths to trusted certificates to the default locations.
     *
     * This will try to set the paths from environment variables, and if that fails, it will use the
     * os defaults.
     * @throws On failure.
     */
    void set_default_verify_paths() {
        if (!SSL_CTX_set_default_verify_paths(get())) {
            throw_ssl_err("Failed to set default trusted certificate store.");
        }
    }

    /**
     * @brief Set the minimum allowed TLS version.
     * @param version Minimum allowed tls version. `TLS1_2_VERSION` and higher is recommended.
     * @throws On failure.
     */
    void set_min_proto_version(long version) {
        if (!SSL_CTX_set_min_proto_version(get(), version)) {
            throw_ssl_err("Failed to set minimum tls version.");
        }
    }

    /**
     * @brief Set `SslCtx` flags.
     * @param opts Flags for `SSL_CTX`.
     */
    void set_options(std::uint64_t opts) noexcept { SSL_CTX_set_options(get(), opts); }

    /**
     * @brief Set file with certificate with which this will verify to peer.
     * @param f Path to file in PEM format with certificate (and parent certificate(s)).
     * @throws On failure.
     */
    void use_certificate_chain_file(const char *f) {
        if (SSL_CTX_use_certificate_chain_file(get(), f) <= 0) {
            throw_ssl_err("Failed to load certificate chain file.");
        }
    }

    /**
     * @brief Set private key of used certificate.
     *
     * This may prompt the user for password for the private key.
     * @param f Path to file with the private key.
     * @param type Format of the file (e.g. `SSL_FILETYPE_PEM`).
     * @throws On failure.
     */
    void use_private_key_file(const char *f, int type) {
        if (SSL_CTX_use_PrivateKey_file(get(), f, type) <= 0) {
            throw_ssl_err("Failed to load private key file.");
        }
    }

    /**
     * @brief Set cache id unique to aplication.
     * @param id_data Arbitrary cache id data.
     * @param id_len Length of `id_data`.
     */
    void set_session_id_context(const char *id_data, unsigned id_len) noexcept {
        SSL_CTX_set_session_id_context(
            get(),
            reinterpret_cast<const unsigned char *>(id_data),
            id_len
        );
    }

    /**
     * @brief Set the session caching mode. (Caching for connections from the same client.)
     * @param mode Caching mode (e.g. `SSL_SESS_CACHE_SERVER`).
     */
    void set_session_cache_mode(long mode) noexcept {
        SSL_CTX_set_session_cache_mode(get(), mode);
    }

    /**
     * @brief Sets the size of cache.
     * @param size Size of the cache.
     */
    void sess_set_cache_size(long size) noexcept {
        SSL_CTX_sess_set_cache_size(get(), size);
    }

    /**
     * @brief Timeout for cached connection data.
     * @param timeout Timeout for cached connection data.
     */
    void sess_set_timeout(std::chrono::seconds timeout) noexcept {
        SSL_CTX_set_timeout(get(), long(timeout.count()));
    }

    /**
     * @brief Uses this to create new `Ssl`.
     * @return Newly create ssl.
     * @throws On failure.
     */
    Ssl create_ssl() { return { get() }; }
};

} // namespace tls
} // namespace tcp_in

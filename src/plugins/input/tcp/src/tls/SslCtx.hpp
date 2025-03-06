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
#include <memory>
#include <stdexcept>
#include <string>

#include <openssl/err.h>
#include <openssl/ssl.h>

#include "Ssl.hpp"
#include "throw_ssl_err.hpp"

namespace tcp_in {
namespace tls {

/** Wrapper around `SSL_CTX` from OpenSSL. */
class SslCtx {
public:
    /**
     * @brief Construct this with the given method.
     * @param method Method for creating `SSL_CTX`. Can be obtained for example with
     * `TLS_server_method()`.
     * @throws `std::runtime_error` on failure.
     */
    SslCtx(const SSL_METHOD *method) : m_ctx(SSL_CTX_new(method)) {
        if (!m_ctx.get()) {
            throw_ssl_err("Failed to create ssl context.");
        }
    }

    /**
     * @brief Set the verify mode. (What should be verified about peer)
     * @param mode Verify mode (e.g. SSL_VERIFY_NONE).
     * @param cb Verify callback.
     */
    void set_verify(int mode, SSL_verify_cb cb = nullptr) noexcept {
        SSL_CTX_set_verify(m_ctx.get(), mode, cb);
    }

    /**
     * @brief Sets the paths to trusted certificates to the default locations.
     *
     * This will try to set the paths from environment variables, and if that fails, it will use the
     * os defaults.
     * @throws `std::runtime_error` on failure.
     */
    void set_default_verify_paths() {
        if (!SSL_CTX_set_default_verify_paths(m_ctx.get())) {
            throw_ssl_err("Failed to set default trusted certificate paths.");
        }
    }

    /**
     * @brief Sets the path to trusted certificate file.
     *
     * This will try to set the path from environment variable `SSL_CERT_FILE`, and if that fails,
     * it will use the os defaults.
     * @throws `std::runtime_error` on failure.
     */
    void set_default_verify_file() {
        if (!SSL_CTX_set_default_verify_file(m_ctx.get())) {
            throw_ssl_err("Failed to set default trusted certificate file.");
        }
    }

    /**
     * @brief Sets the path to trusted certificate file.
     * @throws `std::runtime_error` on failure.
     */
    void load_verify_file(const char *path) {
        if (!SSL_CTX_load_verify_file(m_ctx.get(), path)) {
            throw_ssl_err("Failed to set default trusted certificate file `", path, "`.");
        }
    }

    /**
     * @brief Sets the path to trusted certificates directory.
     *
     * This will try to set the path from environment variable `SSL_CERT_DIR`, and if that fails, it
     * will use the os defaults.
     * @throws `std::runtime_error` on failure.
     */
    void set_default_verify_dir() {
        if (!SSL_CTX_set_default_verify_dir(m_ctx.get())) {
            throw_ssl_err("Failed to set default trusted certificate directory.");
        }
    }

    /**
     * @brief Sets the path to trusted certificates directory.
     * @throws `std::runtime_error` on failure.
     */
    void load_verify_dir(const char *path) {
        if (!SSL_CTX_load_verify_dir(m_ctx.get(), path)) {
            throw_ssl_err("Failed to set default trusted certificate directory `", path, "`.");
        }
    }

    /**
     * @brief Sets the path to trusted certificates store.
     *
     * It will use the OS defaults.
     * @throws `std::runtime_error` on failure.
     */
    void set_default_verify_store() {
        if (!SSL_CTX_set_default_verify_store(m_ctx.get())) {
            throw_ssl_err("Failed to set default trusted certificate store.");
        }
    }

    /**
     * @brief Sets the path to trusted certificates store.
     * @throws `std::runtime_error` on failure.
     */
    void load_verify_store(const char *path) {
        if (!SSL_CTX_load_verify_store(m_ctx.get(), path)) {
            throw_ssl_err("Failed to set default trusted certificate store `", path, "`.");
        }
    }

    /**
     * @brief Set the minimum allowed TLS version.
     * @param version Minimum allowed tls version. `TLS1_2_VERSION` and higher is recommended.
     * @throws `std::runtime_error` on failure.
     */
    void set_min_proto_version(long version) {
        if (!SSL_CTX_set_min_proto_version(m_ctx.get(), version)) {
            throw_ssl_err("Failed to set minimum tls version.");
        }
    }

    /**
     * @brief Set `SslCtx` flags.
     * @param opts Flags for `SSL_CTX`.
     */
    void set_options(std::uint64_t opts) noexcept { SSL_CTX_set_options(m_ctx.get(), opts); }

    /**
     * @brief Set file with certificate with which this will verify to peer.
     * @param file_path Path to file in PEM format with certificate (and parent certificate(s)).
     * @throws `std::runtime_error` on failure.
     */
    void use_certificate_chain_file(const char *file_path) {
        if (SSL_CTX_use_certificate_chain_file(m_ctx.get(), file_path) <= 0) {
            throw_ssl_err("Failed to load certificate chain file.");
        }
    }

    /**
     * @brief Set private key of used certificate.
     *
     * This may prompt the user for password for the private key.
     * @param file_path Path to file with the private key.
     * @param type Format of the file (e.g. `SSL_FILETYPE_PEM`).
     * @throws `std::runtime_error` on failure.
     */
    void use_private_key_file(const char *file_path, int type) {
        if (SSL_CTX_use_PrivateKey_file(m_ctx.get(), file_path, type) <= 0) {
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
            m_ctx.get(),
            reinterpret_cast<const unsigned char *>(id_data),
            id_len
        );
    }

    /**
     * @brief Set the session caching mode. (Caching for connections from the same client.)
     * @param mode Caching mode (e.g. `SSL_SESS_CACHE_SERVER`).
     */
    void set_session_cache_mode(long mode) noexcept {
        SSL_CTX_set_session_cache_mode(m_ctx.get(), mode);
    }

    /**
     * @brief Sets the size of cache.
     * @param size Size of the cache.
     */
    void sess_set_cache_size(long size) noexcept {
        SSL_CTX_sess_set_cache_size(m_ctx.get(), size);
    }

    /**
     * @brief Set timeout for cached connection data.
     * @param seconds Timeout for cached connection data in seconds.
     */
    void sess_set_timeout(long seconds) noexcept {
        SSL_CTX_set_timeout(m_ctx.get(), seconds);
    }

    /**
     * @brief Uses this to create new `Ssl`.
     * @return Newly create ssl.
     * @throws `std::runtime_error` on failure.
     */
    Ssl create_ssl() { return { m_ctx.get() }; }

private:
    /** Deleter for SSL_CTX. */
    class Deleter {
    public:
        void operator()(SSL_CTX *ptr) { SSL_CTX_free(ptr); }
    };

    std::unique_ptr<SSL_CTX, Deleter> m_ctx;
};

} // namespace tls
} // namespace tcp_in

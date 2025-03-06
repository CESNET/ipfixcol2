/**
 * \file
 * \author Jakub Antonín Štigler <xstigl00@cesnet.cz>
 * \brief Wrapper around SSL from OpenSSL. (header file)
 * \date 2025
 *
 * Copyright: (C) 2023 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include <openssl/ssl.h>

#include "SslBio.hpp"
#include "throw_ssl_err.hpp"

namespace tcp_in {
namespace tls {

/** Describes result of read operation. */
enum class ReadResult {
    /** Successufully read some data. */
    READ,
    /** Non blocking socket needs to wait for more data. */
    WAIT,
    /** Peer will not send any further data. */
    FINISHED,
    /** The connection has been closed. (socket cannot read) */
    CLOSED,
};

/**
 * @brief Wrapper around `SSL` from OpenSSL.
 *
 * This is basically io stream for data encrypted with TLS.
 */
class Ssl {
public:
    /**
     * @brief Construct `Ssl` from the given `SSL_CTX`.
     * @param ctx Base context for the ssl object.
     * @throws `std::runtime_error` if failed to create new bio.
     */
    Ssl(SSL_CTX *ctx) : m_ssl(SSL_new(ctx)) {
        if (!m_ssl) {
            throw_ssl_err("Failed to create SSL object.");
        }
    }

    /**
     * @brief Set the bio object from which this will read data.
     * @param bio Bio object from which to read. This will take ownership of the given `SslBio`.
     */
    void set_bio(SslBio bio) noexcept {
        auto raw_bio = bio.release_ptr();
        SSL_set_bio(m_ssl.get(), raw_bio, raw_bio);
    }

    /**
     * @brief Set the server name for SNI.
     * @param hostname Name of the server.
     * @throws `std::runtime_error` on failure.
     */
    void set_tlsext_host_name(const char *hostname) {
        if (!SSL_set_tlsext_host_name(m_ssl.get(), hostname)) {
            throw_ssl_err("Failed to set the SNI hostname.");
        }
    }

    /**
     * @brief Set the expected DNS name of the server.
     * @param hostname Hostname of the server.
     * @throws `std::runtime_error` on failure.
     */
    void set1_host(const char *hostname) {
        if (!SSL_set1_host(m_ssl.get(), hostname)) {
            throw_ssl_err(
                "Failed to set the cergificate verification hostname."
            );
        }
    }

    /**
     * @brief Perform TLS handshake.
     *
     * In case of nonblocking server, this can be multiple times to continue the tls handshake.
     * @returns `true` if TLS handshake is successfully complete. `false` if the tls handshake is
     * incomplete because the underlaying socket is nonblocking and it needs to wait for more data.
     * @throws `std::runtime_error` on failure.
     */
    bool accept();

    /**
     * @brief Read application data from the TLS connection.
     *
     * @param [out] buf Will be cleared. The read data will be put here. Up to the capacity of the
     * buffer will be used.
     * @return result of the read operation:
     * - `READ`: Successfully read some data. `buf` shouldn't be empty.
     * - `WAIT`: Non blocking socket needs to wait for more data. `buf` should be empty.
     * - `FINISHED`: Peer will not send any further data. `buf` should be empty.
     * - `CLOSED`: The connection has been closed. (socket cannot read) `buf` should be empty.
     * @throws `std::runtime_error` on failure.
     */
    ReadResult read_ex(std::vector<std::uint8_t> &buf);

    /**
     * @brief Shuts the connection down.
     * @return `true` on successful shutdown, false if shutdown is in process but not complete yet.
     * @throws `std::runtime_error` on failure.
     */
    bool shutdown() {
        auto ret = SSL_shutdown(m_ssl.get());
        if (ret < 0) {
            throw_ssl_err("SSL failed to shutdown connection.");
        }
        return ret == 1;
    }

private:
    /** Deleter for SSL. */
    class Deleter {
    public:
        void operator()(SSL *ssl) { SSL_free(ssl); }
    };

    std::unique_ptr<SSL, Deleter> m_ssl;
};

} // namespace tls
} // namespace tcp_in

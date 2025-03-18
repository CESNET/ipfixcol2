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

#include "../Reader.hpp"
#include "SslBio.hpp"
#include "throw_ssl_err.hpp"

namespace tcp_in {
namespace tls {

namespace dealloc {

/** Deletor for `SSL` from OpenSSL. Can be used for example in `std::unique_ptr` */
class Ssl {
public:
    void operator()(SSL *ssl) { SSL_free(ssl); }
};

} // namespace dealloc

/**
 * @brief Wrapper around `SSL` from OpenSSL.
 *
 * This is basically io stream for data encrypted with TLS.
 */
class Ssl : public std::unique_ptr<SSL, dealloc::Ssl> {
public:
    /** Unique pointer of `SSL`. This is base class for `Ssl`. */
    using SelfPtr = std::unique_ptr<SSL, dealloc::Ssl>;

    /**
     * @brief Construct `Ssl` from the given `SSL_CTX`.
     * @param ctx Base context for the ssl object.
     * @throws If failed to create new bio.
     */
    Ssl(SSL_CTX *ctx) : SelfPtr(SSL_new(ctx)) {
        if (!get()) {
            throw_ssl_err("Failed to create SSL object.");
        }
    }

    /**
     * @brief Set the bio object from which this will read data.
     * @param bio Bio object from which to read. This will take ownership of the given `SslBio`.
     */
    void set_bio(SslBio bio) noexcept {
        auto raw_bio = bio.release();
        SSL_set_bio(get(), raw_bio, raw_bio);
    }

    /**
     * @brief Set the server name for SNI.
     * @param hostname Name of the server.
     * @throws On failure.
     */
    void set_tlsext_host_name(const char *hostname) {
        if (!SSL_set_tlsext_host_name(get(), hostname)) {
            throw_ssl_err("Failed to set the SNI hostname.");
        }
    }

    /**
     * @brief Set the expected DNS name of the server.
     * @param hostname Hostname of the server.
     * @throws On failure.
     */
    void set1_host(const char *hostname) {
        if (!SSL_set1_host(get(), hostname)) {
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
     * @throws On failure.
     */
    bool accept();

    /**
     * @brief Read application data from the TLS connection.
     * @param [out] data Where to write the data.
     * @param [out,in] length Maximum length of data to write. It will be set to the actual number
     * of read bytes.
     * @throws On failure.
     */
    ReadResult read_ex(std::uint8_t *data, std::size_t &length);

    /**
     * @brief Shuts the connection down.
     * @return `true` on successful shutdown, false if shutdown is in process but not complete yet.
     * @throws On failure.
     */
    bool shutdown() {
        auto ret = SSL_shutdown(get());
        if (ret < 0) {
            throw_ssl_err("SSL failed to shutdown connection.");
        }
        return ret == 1;
    }
};

} // namespace tls
} // namespace tcp_in

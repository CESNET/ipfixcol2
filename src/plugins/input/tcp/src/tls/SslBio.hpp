/**
 * \file
 * \author Jakub Antonín Štigler <xstigl00@cesnet.cz>
 * \brief Wrapper around BIO from OpenSSL. (header file)
 * \date 2025
 *
 * Copyright: (C) 2023 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <memory>

#include <openssl/bio.h>

#include "../UniqueFd.hpp"
#include "throw_ssl_err.hpp"

namespace tcp_in {
namespace tls {

/**
 * @brief Wrapper around `BIO` from OpenSSL.
 *
 * This is basically extension of file descriptor used in OpenSSL.
 */
class SslBio {
public:
    /**
     * @brief Constructs new `SslBio` for the given `BIO_METHOD`.
     * @param method Method created for example by `BIO_s_socket()`.
     * @throws `std::runtime_error` on failure.
     */
    SslBio(const BIO_METHOD *method) : m_bio(BIO_new(method)) {
        if (!m_bio.get()) {
            throw_ssl_err("Failed to create bio.");
        }
    }

    /**
     * @brief Set file descriptor of the bio.
     * @param fd File descriptor for the bio. This will NOT take ownership of the file descriptor.
     */
    void set_fd(int fd) noexcept { BIO_set_fd(m_bio.get(), fd, BIO_NOCLOSE); }

    /**
     * @brief Gets the pointer to bio and releases ownership of it.
     * @return Pointer to the inner bio.
     */
    BIO *release_ptr() noexcept { return m_bio.release(); }

private:
    /** Deleter for BIO. */
    class Deleter {
    public:
        void operator()(BIO *bio) { BIO_free(bio); }
    };

    std::unique_ptr<BIO, Deleter> m_bio;
};


} // namespace tls
} // namespace tcp_in

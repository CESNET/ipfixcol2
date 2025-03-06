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

namespace dealloc {

/** Deleter for `BIO` from OpenSSL. Can be used for example in `std::unique_ptr` */
class SslBio {
public:
    void operator()(BIO *bio) { BIO_free(bio); }
};

}

/**
 * @brief Wrapper around `BIO` from OpenSSL.
 *
 * This is basically extension of file descriptor used in OpenSSL.
 */
class SslBio : public std::unique_ptr<BIO, dealloc::SslBio> {
public:
    /** Unique pointer of `BIO`. This is base class for `SslBio`. */
    using SelfPtr = std::unique_ptr<BIO, dealloc::SslBio>;

    /**
     * @brief Construct `SslBio` by taking ownership of the given `BIO`.
     * @param bio Underlaying `BIO`. This will take ownership of it.
     * @throws If `bio` is `nullptr`.
     */
    SslBio(BIO *bio) : SelfPtr(bio) {
        if (!get()) {
            throw_ssl_err("Failed to create bio.");
        }
    }

    /**
     * @brief Constructs new `SslBio` for the given `BIO_METHOD`.
     * @param method Method created for example by `BIO_s_socket()`.
     * @throws On failure.
     */
    SslBio(const BIO_METHOD *method) : SslBio(BIO_new(method)) { }
    /**
     * @brief Construct `SslBio` server listening at the given port (and address).
     * @param host_port Port and optionally also address of the server. `[address:]port`.
     * @throws On failure.
     */
    SslBio(const char *host_port) : SslBio(BIO_new_accept(host_port)) { }

    /**
     * @brief Set file descriptor of the bio.
     * @param fd File descriptor for the bio. This will NOT take ownership of the file descriptor.
     */
    void set_fd(int fd) noexcept { BIO_set_fd(get(), fd, BIO_NOCLOSE); }
};


} // namespace tls
} // namespace tcp_in

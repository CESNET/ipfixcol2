/**
 * \file
 * \author Jakub Antonín Štigler <xstigl00@cesnet.cz>
 * \brief Wrapper around SSL from OpenSSL. (source file)
 * \date 2025
 *
 * Copyright: (C) 2023 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "Ssl.hpp"

namespace tcp_in {
namespace tls {

bool Ssl::accept() {
    auto res = SSL_accept(m_ssl.get());
    if (res <= 0) {
        auto err = SSL_get_error(m_ssl.get(), res);
        // Nonblocking client needs to wait.
        if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
            return false;
        }
        throw_ssl_err(err, "Failed performing TLS handshake with client.");
    }
    return true;
}

ReadResult Ssl::read_ex(std::vector<std::uint8_t> &buf) {
    buf.resize(buf.capacity());
    std::size_t len;
    auto ret = SSL_read_ex(m_ssl.get(), buf.data(), buf.size(), &len);

    if (ret <= 0) {
        auto err = SSL_get_error(m_ssl.get(), ret);
        buf.resize(0);
        switch (err) {
            case SSL_ERROR_SSL:
                // The TLS connection is broken.
            case SSL_ERROR_ZERO_RETURN:
                return ReadResult::FINISHED;
            case SSL_ERROR_SYSCALL:
                return ReadResult::CLOSED;
            case SSL_ERROR_WANT_READ:
            case SSL_ERROR_WANT_WRITE:
                return ReadResult::WAIT;
            default:
                throw_ssl_err(err, "SSL failed to read data.");
        }
    }

    buf.resize(std::min(len, buf.size()));
    return ReadResult::READ;
}

} // namespace tls
} // namespace tcp_in

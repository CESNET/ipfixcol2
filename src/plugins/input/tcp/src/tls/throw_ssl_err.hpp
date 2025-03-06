/**
 * \file
 * \author Jakub Antonín Štigler <xstigl00@cesnet.cz>
 * \brief Helper functions for forming OpenSSL error messages. (header file)
 * \date 2025
 *
 * Copyright: (C) 2023 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <array>
#include <sstream>
#include <string>

#include <openssl/err.h>

namespace tcp_in {
namespace tls {

/**
 * @brief Throws exception with the given message appended with arguments and OpenSSL error message
 * based on error code.
 * @param code Error code of the OpenSSL message.
 * @param msg Message describing the error.
 * @throws `std::runtime_error`
 */
[[noreturn]]
static void throw_ssl_err(int code, std::ostringstream msg) {
    std::array<char, 256> err;
    ERR_error_string_n(code, err.data(), err.size());
    msg << err.data();

    auto res = msg.str();
    if (*res.rbegin() == '.') {
        res.pop_back();
        res += ": ";
    }

    throw std::runtime_error(res + err.data());
}

/**
 * @brief Throws exception with the given message appended with arguments and OpenSSL error message
 * based on error code.
 * @param code Error code of the OpenSSL message.
 * @param msg Partial message describing the error.
 * @param arg Next argument to be appended to the message.
 * @param args This will be converted to string and appended to the message.
 * @throws `std::runtime_error`
 */
template<typename ARG, typename... ARGS>
[[noreturn]]
void throw_ssl_err(int code, std::ostringstream msg, ARG &&arg, ARGS &&...args) {
    msg << arg;
    throw_ssl_err(code, std::move(msg), std::forward<ARGS>(args)...);
}

/**
 * @brief Throws exception with the given message appended with arguments and OpenSSL error message
 * based on error code.
 * @param code Error code of the OpenSSL message.
 * @param what Message describing the error.
 * @param args This will be converted to string and appended to the message.
 * @throws `std::runtime_error`
 */
template<typename... ARGS>
[[noreturn]]
void throw_ssl_err(int code, const std::string &what, ARGS &&...args) {
    std::ostringstream res;
    res << what;
    throw_ssl_err(code, std::move(res), std::forward<ARGS>(args)...);
}

/**
 * @brief Throws exception with the given message appended with arguments and error message from top
 * of the OpenSSL error stack. The error is popped from the error stack.
 * @param what Message describing the error.
 * @param args This will be converted to string and appended to the message.
 * @throws `std::runtime_error`
 */
template<typename... ARGS>
[[noreturn]]
void throw_ssl_err(const std::string &what, ARGS &&...args) {
    throw_ssl_err(ERR_get_error(), what, std::forward<ARGS>(args)...);
}

} // namespace tls
} // namespace tcp_in

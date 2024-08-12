/**
 * \file
 * \author Jakub Antonín Štigler <xstigl00@stud.fit.vutbr.cz>
 * \brief Function for reading from file descriptor (header file)
 * \date 2024
 *
 * Copyright: (C) 2023 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <cstddef>   // size_t
#include <cerrno>    // errno, EWOULDBLOCK, EAGAIN
#include <string>    // std::string
#include <stdexcept> // std::runtime_error

#include <sys/socket.h> // recv

#include <ipfixcol2.h> // ipx_strerror

/**
 * @brief Reads to vector from file descriptor until the vector has the desired number of bytes
 *
 * @tparam Vec vector like object
 * @tparam EofSig `EofSig::signal_eof()` is called when eof is reached
 * @param n number of bytes that should be in the vector
 * @param fd file descriptor to read from
 * @param result where to read the bytes to
 * @param eofSignaler has method to signal eof
 * @return true if the vector has at least `n` bytes after the call
 * @throws when fails to read from the file descriptor
 */
template<typename Vec, typename EofSig>
bool read_until_n(size_t n, int fd, Vec &result, EofSig &eofSignaler) {
    auto filled = result.size();
    if (filled >= n) {
        return true;
    }

    auto remaining = n - filled;
    result.resize(n);

    int res = recv(fd, result.data() + filled, remaining, 0);
    if (res == -1) {
        result.resize(filled);
        int err = errno;
        if (err == EWOULDBLOCK || err == EAGAIN) {
            return false;
        }

        const char *err_str;
        ipx_strerror(err, err_str);
        throw std::runtime_error("Failed to read from descriptor: " + std::string(err_str));
    }

    result.resize(filled + res);

    if (res == 0) {
        eofSignaler.signal_eof();
    }

    if (static_cast<size_t>(res) != remaining) {
        return false;
    }

    return true;
}

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

#include <cerrno>    // errno, EWOULDBLOCK, EAGAIN
#include <cstddef>   // size_t
#include <stdexcept> // std::runtime_error
#include <string>    // std::string

#include <ipfixcol2.h> // ipx_strerror

#include "Reader.hpp"

/**
 * @brief Reads to vector from file descriptor until the vector has the desired number of bytes
 *
 * @tparam Vec vector like object
 * @tparam EofSig `EofSig::signal_eof()` is called when eof is reached
 * @param n number of bytes that should be in the vector
 * @param reader generic read function
 * @param result where to read the bytes to
 * @param eofSignaler has method to signal eof
 * @return true if the vector has at least `n` bytes after the call
 * @throws when fails to read from the file descriptor
 */
template<typename Vec, typename EofSig>
bool read_until_n(size_t n, Reader &reader, Vec &result, EofSig &eofSignaler) {
    auto filled = result.size();
    if (filled >= n) {
        return true;
    }

    result.resize(n);

    auto remaining = n - filled;
    auto res = reader(result.data() + filled, remaining);

    result.resize(filled + remaining);

    if (res == ReadResult::END) {
        eofSignaler.signal_eof();
    }

    return result.size() == n;
}

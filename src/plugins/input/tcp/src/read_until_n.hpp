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
 * @brief Reads to vector from file descriptor until the vector has the desired number of bytes.
 *
 * If the vector doesn't have n bytes after calling this function, it means that the reader doesn't
 * have any more available data, but may have in future.
 *
 * @tparam Vec vector like object.
 * @tparam EofSig `EofSig::signal_eof()` is called when eof is reached.
 * @param n Number of bytes that should be in the vector.
 * @param reader Generic read function.
 * @param result Where to read the bytes to.
 * @param eofSignaler Has method to signal eof.
 * @return Number of new read bytes.
 * @throws When `reader` throws.
 */
template<typename Vec, typename EofSig>
std::size_t read_until_n(size_t n, Reader &reader, Vec &result, EofSig &eofSignaler) {
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

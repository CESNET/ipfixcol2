/**
 * \file
 * \author Jakub Antonín Štigler <xstigl00@cesnet.cz>
 * \brief Generic reader class. (header file)
 * \date 2025
 *
 * Copyright: (C) 2023 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <cstdint>

#include <ipfixcol2.h>

namespace tcp_in {


/** Describes result of read operation. */
enum class ReadResult {
    /** Successufully read some data. */
    READ,
    /** Non blocking socket needs to wait for more data. */
    WAIT,
    /** Connection has ended. */
    END,
};

class Reader {
public:
    /**
     * @brief Read next portion of data.
     * @param[out] data Here will be store read data.
     * @param[in,out] length As input is the maximum amount of data to read. Output value will be
     * the actual amount of read data.
     * @return ReadResult Result of the read operation.
     */
    virtual ReadResult read(std::uint8_t *data, std::size_t &length) = 0;

    /**
     * @brief Reads to vector from file descriptor until the vector has the desired number of bytes.
     *
     * If the vector doesn't have n bytes after calling this function, it means that the reader
     * doesn't have any more available data, but may have in future.
     *
     * @tparam Vec vector like object.
     * @tparam EofSig `EofSig::signal_eof()` is called when eof is reached.
     * @param n Number of bytes that should be in the vector.
     * @param result Where to read the bytes to.
     * @param eofSignaler Has method to signal eof.
     * @return Number of new read bytes.
     */
    template<typename Vec, typename EofSig>
    std::size_t read_until_n(std::size_t n, Vec &result, EofSig &eofSignaler) {
        auto filled = result.size();
        if (filled >= n) {
            return true;
        }

        result.resize(n);

        auto read_len = n - filled;
        auto res = read(result.data() + filled, read_len);

        result.resize(filled + read_len);

        if (res == ReadResult::END) {
            eofSignaler.signal_eof();
        }

        return read_len;
    }
};

} // namespace tcp_in

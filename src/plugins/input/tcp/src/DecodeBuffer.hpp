/**
 * \file
 * \author Jakub Antonín Štigler <xstigl00@stud.fit.vutbr.cz>
 * \brief Buffer for managing decoded IPFIX data (header file)
 * \date 2024
 *
 * Copyright: (C) 2023 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <cstdint> // uint8_t, UINT16_MAX
#include <cstddef> // size_t
#include <vector>  // std::vector

#include "ByteVector.hpp" // ByteVector
#include "Reader.hpp"

namespace tcp_in {

/** Buffer for collecting and reconstructing decoded IPFIX messages. */
class DecodeBuffer {
public:
    /** Creates empty decode buffer. */
    DecodeBuffer() :
        m_total_bytes_decoded(0),
        m_eof_reached(false),
        m_decoded(),
        m_part_decoded(),
        m_decoded_size(0)
    {};

    template<typename Fun>
    void process_decoded(Fun fun) {
        for (auto &msg : m_decoded) {
            fun(std::move(msg));
        }

        m_decoded.clear();
        m_total_bytes_decoded = 0;
    }

    /**
     * @brief Adds new decoded IPFX message to the buffer. The passed buffer is emptied.
     * @param data IPFIX message to add. This buffer will be emptied.
     */
    void add(ByteVector &&data) {
        m_total_bytes_decoded += data.size();
        m_decoded.push_back(std::move(data));
    }

    /**
     * @brief Read data using generic reader function.
     * @param reader Generic reader function.
     * @param consume Minimum number of bytes that should be read from reader if possible.
     */
    void read_from(Reader &reader, std::size_t consume = 0);

    /**
     * @brief Checks whether enough data has been read since last time. When this returns true,
     * decoder should consider returning.
     * @return true if decoder should stop reading the data adn return.
     */
    bool enough_data() const {
        // Limit for number of bytes in decoded messages in one decode call to decoder.
        // It is this value because this is the theoretical maximum size of IPFIX message, so each
        // decode call has chance to decode at least one message.
        constexpr size_t SIZE_LIMIT = UINT16_MAX;

        return m_total_bytes_decoded >= SIZE_LIMIT;
    }

    /**
     * @brief Signals that eof has been reached.
     * @throws when there is incomplete message.
     */
    void signal_eof();

    /**
     * @brief Checks whether eof has been reached.
     * @return true if eof has been reached.
     */
    bool is_eof_reached() const {
        return m_eof_reached;
    }

private:
    /**
     * @brief Reads the length from ipfix header to `m_decoded_size`.
     * @param reader Reader from which to read.
     * @return `true` if the whole header could be read.
     */
    bool read_header(Reader &reader);
    /**
     * @brief Reads the body of ipfix message.
     * @param reader Reader from which to read.
     * @return `true` if the whole header could be read.
     */
    bool read_body(Reader &reader);
    /**
     * @brief Reads to `m_part_decoded` until it reaches length of `n`.
     * @param n Target length for `m_part_decoded`.
     * @param reader Generic reader from which to read.
     * @return `true` if `m_part_decoded` has reached length of `n`.
     */
    bool read_until_n(size_t n, Reader &reader);

    /** number of bytes readed since last call to `get_decoded` */
    size_t m_total_bytes_decoded;
    bool m_eof_reached;
    /** Decoded data waiting to be sent. */
    std::vector<ByteVector> m_decoded;
    /** Partially decoded data. */
    ByteVector m_part_decoded;
    /** Expected length of fully decoded data. */
    size_t m_decoded_size;
};

} // namespace tcp_in

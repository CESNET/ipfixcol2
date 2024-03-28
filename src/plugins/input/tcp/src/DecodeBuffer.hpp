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

#include <vector>  // std::vector
#include <cstdint> // uint8_t, UINT16_MAX
#include <cstddef> // size_t

#include "ByteVector.hpp" // ByteVector

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
     * @brief Copies IPFIX data from buffer.
     *
     * The data may be any part of message (possibly incomplete or even multiple messages) but
     * multiple calls to this method must be with the message data in correct order so that it can
     * be reconstructed.
     * @param data data with the message
     * @param size size of the data in `data`
     */
    void read_from(const uint8_t *data, size_t size);

    /**
     * @brief Copies IPFIX data from circular buffer.
     *
     * The data may be any part of message (possibly incomplete or even multiple messages) but
     * multiple calls to this metod must be with the message data in correct order so that it can be
     * reconstructed.
     * @param data data of the circullar buffer
     * @param buffer_size size of the circullar buffer (allocated space)
     * @param data_size size of data to copy from the buffer
     * @param position start position of the data in the buffer.
     */
    void read_from(const uint8_t *data, size_t buffer_size, size_t data_size, size_t position);

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
     * @brief reads the length from ipfix header to `m_decoded_size`
     * @return Pointer to the first unreaded byte from `data`
     */
    const uint8_t *read_header(const uint8_t *data, size_t size);
    /**
     * @brief Adds N elements from `src` to `m_part_decoded` where N is the smaller of the two
     * sizes.
     * @return The smaller of the two sizes.
     */
    size_t read_min(const uint8_t *data, size_t size1, size_t size2);

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

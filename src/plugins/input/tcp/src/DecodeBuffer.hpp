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
        m_bytes_read(0),
        m_eof_reached(false),
        m_messages(),
        m_current_message()
    {}

    template<typename Fun>
    void process_decoded(Fun fun) {
        for (auto &msg : m_messages) {
            fun(std::move(msg));
        }

        m_messages.clear();
        m_bytes_read = 0;
    }

    /**
     * @brief Adds new decoded IPFX message to the buffer. The passed buffer is emptied.
     * @param data IPFIX message to add. This buffer will be emptied.
     */
    void add(ByteVector&& data);

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
     * @param buffer data of the circullar buffer
     * @param buffer_size size of the circullar buffer (allocated space)
     * @param data_size size of data to copy from the buffer
     * @param position start position of the data in the buffer.
     */
    void read_from(const uint8_t *buffer, size_t buffer_size, size_t data_size, size_t position);

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

        return m_bytes_read >= SIZE_LIMIT;
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
     * @brief Reads at most `max_read` bytes from `data` of size `data_len` and appends it to the current chunk.
     * @param data The source data.
     * @param data_len The source data length.
     * @param max_read The maximum number of bytes to read from `data`.
     * @return Number of bytes read.
     */
    size_t read_at_most(const uint8_t *data, size_t data_len, size_t max_read);

    /** number of bytes readed since last call to `get_decoded` */
    size_t m_bytes_read;
    bool m_eof_reached;
    /// Decoded IPFIX messages
    std::vector<ByteVector> m_messages;
    /// IPFIX message currently being read
    ByteVector m_current_message;
};

} // namespace tcp_in

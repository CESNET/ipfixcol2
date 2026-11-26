/**
 * \file
 * \author Jakub Antonín Štigler <xstigl00@stud.fit.vutbr.cz>
 * \brief Buffer for managing decoded IPFIX data (source file)
 * \date 2024
 *
 * Copyright: (C) 2023 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "DecodeBuffer.hpp"

#include <cstdint>   // uint8_t
#include <algorithm> // copy_n
#include <stdexcept> // runtime_error
#include <cstddef>   // size_t

#include <ipfixcol2.h> // fds_ipfix_msg_hdr

namespace tcp_in {

static constexpr size_t IPFIX_MSG_HDR_LEN = sizeof(fds_ipfix_msg_hdr);

void DecodeBuffer::add(ByteVector&& data)
{
    m_bytes_read += data.size();
    m_messages.push_back(std::move(data));
}

void DecodeBuffer::read_from(const uint8_t *data, size_t size)
{
    while (size > 0) {
        // Read IPFIX message header
        if (m_current_message.size() < IPFIX_MSG_HDR_LEN) {
            size_t n_bytes_read = read_at_most(data, size, IPFIX_MSG_HDR_LEN - m_current_message.size());
            data += n_bytes_read;
            size -= n_bytes_read;

            if (m_current_message.size() < IPFIX_MSG_HDR_LEN) {
                return; // Still need more data for header.
            }
        }

        // Read IPFIX message body
        const auto* msg_hdr = reinterpret_cast<const struct fds_ipfix_msg_hdr*>(m_current_message.data());
        const uint16_t full_msg_len = ntohs(msg_hdr->length);

        if (full_msg_len < IPFIX_MSG_HDR_LEN) {
            throw std::runtime_error("DecodeBuffer: invalid IPFIX message size (less than 16 bytes)");
        }

        if (m_current_message.size() < full_msg_len) {
            // Allocate the whole message at once (no-op if already reserved)
            m_current_message.reserve(full_msg_len);
            size_t n_bytes_read = read_at_most(data, size, full_msg_len - m_current_message.size());
            data += n_bytes_read;
            size -= n_bytes_read;
        }

        // Message fully read
        if (m_current_message.size() == full_msg_len) {
            m_messages.emplace_back(std::move(m_current_message));
            m_current_message = {};
        }
    }
}

void DecodeBuffer::read_from(
    const uint8_t *buffer,
    size_t buffer_size,
    size_t data_size,
    size_t position
) {
    const auto *data_start = buffer + position;
    const auto bytes_till_buffer_end = buffer_size - position;

    if (data_size <= bytes_till_buffer_end) {
        read_from(data_start, data_size);
    } else {
        read_from(data_start, bytes_till_buffer_end);
        read_from(buffer, data_size - bytes_till_buffer_end);
    }
}

void DecodeBuffer::signal_eof()
{
    if (m_current_message.size() != 0) {
        throw std::runtime_error("Received incomplete message.");
    }
    m_eof_reached = true;
}


size_t DecodeBuffer::read_at_most(const uint8_t *data, size_t data_len, size_t max_read)
{
    const auto read_amount = std::min(data_len, max_read);
    const auto old_size = m_current_message.size();
    m_current_message.resize(old_size + read_amount);
    std::copy_n(data, read_amount, m_current_message.data() + old_size);
    m_bytes_read += read_amount;
    return read_amount;
}

} // namespace tcp_in


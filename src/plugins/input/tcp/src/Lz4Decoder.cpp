/**
 * \file
 * \author Jakub Antonín Štigler <xstigl00@stud.fit.vutbr.cz>
 * \brief IPFIX decoder for tcp plugin (source file)
 * \date 2024
 *
 * Copyright: (C) 2023 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "Lz4Decoder.hpp"

#include <cstdint>   // uint32_t, uint16_t
#include <stdexcept> // runtime_error
#include <cstddef>   // size_t
#include <cerrno>    // errno, EWOULDBLOCK, EAGAIN
#include <cstring>
#include <string>    // string

#include <netinet/in.h> // ntohl, ntohs
#include <lz4.h>        // LZ4_decompress_safe_continue, LZ4_createStreamDecode, LZ4_setStreamDecode
#include <unistd.h>     // read

#include <ipfixcol2.h> // ipx_strerror

#include "DecodeBuffer.hpp" // DecodeBuffer
#include "read_until_n.hpp" // read_until_n

namespace tcp_in {

struct __attribute__((__packed__)) ipfix_compress_header {
    uint16_t decompressed_size;
    uint16_t compressed_size;
};

struct __attribute__((__packed__)) ipfix_start_compress_header {
    uint32_t magic;
    uint32_t buffer_size;
};

/**
 * @brief Creates reader that reads from circullar buffer. The reader will return WAIT when all data
 * is read.
 * @param buf Circullar buffer to read from.
 * @param buf_len Length of the circullar buffer.
 * @param data_len Length of data in the circullar buffer.
 * @param data_pos Position of first byte of data in the circullar buffer.
 * @return Reader that reads from circullar buffer.
 */
Reader circullar_buffer_reader(
    std::uint8_t *buf,
    std::size_t buf_len,
    std::size_t data_len,
    std::size_t data_pos
) {
    // Split it into the two buffers.
    std::uint8_t *buf1, *buf2;
    std::size_t len1, len2;
    if (data_pos + data_len > buf_len) {
        buf1 = buf + data_pos;
        len1 = buf_len - data_pos;
        buf2 = buf;
        len2 = data_len - len1;
    } else {
        buf1 = buf;
        len1 = 0;
        buf2 = buf + data_pos;
        len2 = data_len;
    }

    return [=](std::uint8_t *data, std::size_t &length) mutable -> ReadResult {
        if (len1 == 0 && len2 == 0) {
            return ReadResult::WAIT;
        }

        auto r1 = std::min(len1, length);
        std::memcpy(data, buf1, r1);
        buf1 += r1;
        len1 -= r1;

        auto r2 = std::min(len2, length - r1);
        std::memcpy(data + r1, buf2, r2);
        buf2 += r2;
        len2 += r2;

        length = r1 + r2;
        return ReadResult::READ;
    };
}

Lz4Decoder::Lz4Decoder(int fd) :
    m_fd(fd),
    m_decoded(),
    m_decoder(LZ4_createStreamDecode()),
    m_decompressed(),
    m_decompressed_pos(0),
    m_compressed(),
    m_compressed_size(0),
    m_decompressed_size(0)
{
    if (!m_decoder) {
        throw std::runtime_error("LZ4 Decoder: Failed to create stream decoder");
    }
}

DecodeBuffer &Lz4Decoder::decode() {
    while (!m_decoded.enough_data()) {
        // Read the header.
        if (!read_header()) {
            // There is not enough data to read the whole header.
            break;
        }

        // Read the body of the message.
        if (!read_body()) {
            // There is not enough data to read the whole body.
            break;
        }

        // Decompress the readed message and add it to the decode buffer.
        decompress();
    }

    if (m_decoded.is_eof_reached() && m_compressed.size() != 0) {
        throw std::runtime_error("Incomplete compressed message received");
    }

    return m_decoded;
}

bool Lz4Decoder::read_header() {
    /** compress header size */
    constexpr size_t CH_SIZE = sizeof(ipfix_compress_header);

    if (m_compressed_size != 0) {
        // The header is already read, but the message body is incomplete.
        return true;
    }

    if (m_decompressed.size() == 0) {
        // The size of the ring buffer is unknown; this is the first message the decoder decodes.
        // Read the recommended ring buffer size from the start header.
        if (!read_start_header()) {
            // There is not enough data to read the whole start header.
            return false;
        }
    }

    // Read the header.
    if (!read_until_n(CH_SIZE)) {
        // There is not enough data to read the whole header.
        return false;
    }

    // Get the message sizes from the header.
    auto hdr = reinterpret_cast<ipfix_compress_header *>(m_compressed.data());
    m_compressed_size = ntohs(hdr->compressed_size);
    m_decompressed_size = ntohs(hdr->decompressed_size);

    // The header is not part of the message that should be decompressed.
    m_compressed.clear();
    return true;
}

bool Lz4Decoder::read_start_header() {
    /** start compress header size */
    constexpr size_t SCH_SIZE = sizeof(ipfix_start_compress_header);

    // Read the header.
    if (!read_until_n(SCH_SIZE)) {
        // There is not enough data to read the whole start header.
        return false;
    }

    // Get the ring buffer size from the header.
    auto hdr = reinterpret_cast<ipfix_start_compress_header *>(m_compressed.data());
    auto new_buffer_size = ntohl(hdr->buffer_size);

    // The start header is not part of the message that should be decompressed.
    m_compressed.clear();

    reset_stream(new_buffer_size);
    return true;
}

bool Lz4Decoder::read_body() {
    return read_until_n(m_compressed_size);
}

void Lz4Decoder::decompress() {
    // The message is never on buffer boundaries.
    if (m_decompressed.size() - m_decompressed_pos < m_decompressed_size) {
        m_decompressed_pos = 0;
    }

    int res = LZ4_decompress_safe_continue(
        m_decoder.get(),
        reinterpret_cast<char *>(m_compressed.data()),
        reinterpret_cast<char *>(m_decompressed.data()) + m_decompressed_pos,
        m_compressed_size,
        m_decompressed_size
    );

    if (res < 0) {
        throw std::runtime_error("LZ4 Decoder: decompression failed");
    }

    if (static_cast<size_t>(res) != m_decompressed_size) {
        // this shouldn't happen, but it is not error
        m_decompressed_size = res;
    }

    // Copy the decompressed data into the decode buffer
    auto reader = circullar_buffer_reader(
        m_decompressed.data(),
        m_decompressed.size(),
        m_decompressed_size,
        m_decompressed_pos
    );
    m_decoded.read_from(reader, m_decompressed_size);

    m_decompressed_pos += m_decompressed_size;
    if (m_decompressed_pos >= m_decompressed.size()) {
        m_decompressed_pos -= m_decompressed.size();
    }

    m_compressed.clear();
    m_compressed_size = 0;
}

bool Lz4Decoder::read_until_n(size_t n) {
    auto reader = tcp_reader(m_fd);
    return ::read_until_n(n, reader, m_compressed, m_decoded);
}

void Lz4Decoder::reset_stream(size_t buffer_size) {
    m_decompressed.resize(buffer_size);
    m_decompressed_pos = 0;

    int res = LZ4_setStreamDecode(m_decoder.get(), nullptr, 0);
    if (res == 0) {
        throw std::runtime_error("LZ4 Decoder: Failed to reset stream decoder.");
    }
}

} // namespace tcp_in


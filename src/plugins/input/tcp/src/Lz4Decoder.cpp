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
        if (!read_header()) {
            break;
        }

        if (!read_body()) {
            break;
        }

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
        return true;
    }

    if (!read_until_n(CH_SIZE)) {
        return false;
    }

    if (m_decompressed.size() == 0) {
        if (!read_start_header()) {
            return false;
        }
    }

    if (!read_until_n(CH_SIZE)) {
        return false;
    }

    auto hdr = reinterpret_cast<ipfix_compress_header *>(m_compressed.data());
    m_compressed_size = ntohs(hdr->compressed_size);
    m_decompressed_size = ntohs(hdr->decompressed_size);

    m_compressed.clear();
    return true;
}

bool Lz4Decoder::read_start_header() {
    /** start compress header size */
    constexpr size_t SCH_SIZE = sizeof(ipfix_start_compress_header);

    if (!read_until_n(SCH_SIZE)) {
        return false;
    }

    auto hdr = reinterpret_cast<ipfix_start_compress_header *>(m_compressed.data());
    auto new_buffer_size = ntohl(hdr->buffer_size);

    m_compressed.clear();

    reset_stream(new_buffer_size);
    return true;
}

bool Lz4Decoder::read_body() {
    return read_until_n(m_compressed_size);
}

void Lz4Decoder::decompress() {
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
    m_decoded.read_from(
        m_decompressed.data(),
        m_decompressed.size(),
        m_decompressed_size,
        m_decompressed_pos
    );

    m_decompressed_pos += m_decompressed_size;
    if (m_decompressed_pos >= m_decompressed.size()) {
        m_decompressed_pos -= m_decompressed.size();
    }

    m_compressed.clear();
    m_compressed_size = 0;
}

bool Lz4Decoder::read_until_n(size_t n) {
    return ::read_until_n(n, m_fd, m_compressed, m_decoded);
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


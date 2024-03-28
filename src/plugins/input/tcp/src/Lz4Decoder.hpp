/**
 * \file
 * \author Jakub Antonín Štigler <xstigl00@stud.fit.vutbr.cz>
 * \brief IPFIX decoder for tcp plugin (header file)
 * \date 2024
 *
 * Copyright: (C) 2023 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <memory>  // std::unique_ptr
#include <cstdint> // uint8_t, uint32_t
#include <vector>  // std::vector
#include <cstddef> // size_t

#include <lz4.h> // LZ4_streamDecode_t, LZ4_freeStreamDecode

#include "Decoder.hpp"      // Decoder
#include "DecodeBuffer.hpp" // DecodeBuffer

namespace tcp_in {

/** Byte sequence at the start of lz4 stream. Used to determine what decoder should be used */
constexpr uint32_t LZ4_MAGIC = 0x4c5a3463;

struct Lz4DecodeDestructor {
    inline void operator()(void *p) noexcept {
        LZ4_freeStreamDecode(reinterpret_cast<LZ4_streamDecode_t *>(p));
    }
};

/** Decoder for LZ4 stream compression */
class Lz4Decoder : public Decoder {
public:
    /**
     * @brief Creates ipfix decoder.
     * @param fd TCP connection file descriptor.
     * @throws when fails to create stream decoder
     */
    Lz4Decoder(int fd);

    /**
     * @brief Decodes the next blocks of data until there is no data or when enough data is readed
     * @return decoded data
     * @throws when fails to read from file descriptor
     * @throws when fails to decompress the data
     */
    virtual DecodeBuffer &decode() override;

    virtual const char *get_name() const override {
        return "LZ4";
    }

private:
    /**
     * @brief reads header, returns true if whole header is readed
     * @return true if whole header is readed
     * @throws when fails to read from file descriptor
     */
    bool read_header();
    /**
     * @brief reads the start header and resets stream, returns true if whole header is readed
     * @return true if whole header is readed
     * @throws when fails to read from file descriptor
     */
    bool read_start_header();
    /**
     * @brief reads body, returns true if whole body is readed
     * @return true if whole body is readed
     * @throws when fails to read from file descriptor
     */
    bool read_body();
    /**
     * @brief decompresses the next block of memory and write it to the decode buffer
     * @throws when decompression fails
     */
    void decompress();
    /**
     * @brief read while `m_compressed.size() < n`
     * @param n number of bytes required to be in `m_compressed`
     * @return true if after call `m_compressed` has at least `n` bytes.
     * @throws when fails to read from file descriptor
     */
    bool read_until_n(size_t n);
    /**
     * @brief resets the lz4 stream
     * @param buffer_size new decompressed buffer size
     * @throws when fails to reset LZ4 stream decoder
     */
    void reset_stream(size_t buffer_size);

    int m_fd;
    DecodeBuffer m_decoded;

    std::unique_ptr<LZ4_streamDecode_t, Lz4DecodeDestructor> m_decoder;

    /** internal circular buffer */
    std::vector<uint8_t> m_decompressed;
    /** current position in the circular buffer (to the first unused index) */
    size_t m_decompressed_pos;

    /** When m_compressed_size == 0, this may contain incomplete header */
    std::vector<uint8_t> m_compressed;

    /** size of full compressed block, 0 when unknown */
    size_t m_compressed_size;
    /** size of full decompressed block */
    size_t m_decompressed_size;
};

} // namespace tcp_in

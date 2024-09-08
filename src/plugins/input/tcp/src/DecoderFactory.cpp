/**
 * \file
 * \author Jakub Antonín Štigler <xstigl00@stud.fit.vutbr.cz>
 * \brief Factory for creating decoders (source file)
 * \date 2024
 *
 * Copyright: (C) 2023 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "DecoderFactory.hpp"

#include <memory>    // unique_ptr
#include <array>     // array
#include <cstdint>   // uint8_t, uint16_t
#include <stdexcept> // runtime_error
#include <cerrno>    // errno
#include <string>    // string
#include <cstddef>   // size_t

#include <sys/socket.h> // recv, MSG_PEEK, MSG_WAITALL
#include <netinet/in.h> // ntohl, ntohs

#include <ipfixcol2.h> // ipx_strerror

#include "Decoder.hpp"      // Decoder
#include "Lz4Decoder.hpp"   // LZ4_MAGIC, Lz4Decoder
#include "IpfixDecoder.hpp" // IPFIX_MAGIC, IpfixDecoder

#include <iostream>

namespace tcp_in {

DecoderFactory::DecoderFactory() {};

std::unique_ptr<Decoder> DecoderFactory::detect_decoder(int fd) {
    // number of bytes neaded to detect the decoder
    constexpr size_t MAX_MAGIC_LEN = 4;

    std::array<uint8_t, MAX_MAGIC_LEN> buf{};

    auto res = recv(fd, buf.begin(), buf.size(), MSG_PEEK | MSG_DONTWAIT);
    if (res == EAGAIN || res == EWOULDBLOCK) {
        // Not enough data yet
        return nullptr;
    }

    if (res == -1) {
        const char *err_msg;
        ipx_strerror(errno, err_msg);
        throw std::runtime_error("Failed to receive start of first message: " + std::string(err_msg));
    }

    constexpr const char *not_enough_data_err =
        "Failed to read enough bytes to recognize the decoder";

    // check decoders in order from shortest magic number to longest

    if (res < 2) {
        throw std::runtime_error(not_enough_data_err);
    }

    // IPFIX decoder
    auto magic_u16 = ntohs(*reinterpret_cast<uint16_t *>(buf.begin()));
    if (magic_u16 == IPFIX_MAGIC) {
        return create_ipfix_decoder(fd);
    }

    if (res < 4) {
        throw std::runtime_error(not_enough_data_err);
    }

    // LZ4 decoder
    auto magic_u32 = ntohl(*reinterpret_cast<uint32_t *>(buf.begin()));
    if (magic_u32 == LZ4_MAGIC) {
        return create_lz4_decoder(fd);
    }

    throw std::runtime_error("Failed to recognize the decoder.");
}

std::unique_ptr<Decoder> DecoderFactory::create_ipfix_decoder(int fd) {
    return std::unique_ptr<Decoder>(new IpfixDecoder(fd));
}

std::unique_ptr<Decoder> DecoderFactory::create_lz4_decoder(int fd) {
    return std::unique_ptr<Decoder>(new Lz4Decoder(fd));
}

} // namespace tcp_in


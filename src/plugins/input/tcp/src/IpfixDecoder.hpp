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

#include <cstdint> // uint16_t
#include <cstddef> // size_t

#include "Decoder.hpp"      // Decoder
#include "DecodeBuffer.hpp" // DecodeBuffer
#include "ByteVector.hpp"   // ByteVector

namespace tcp_in {

/** Identifies data for which this decoder should be used. */
constexpr uint16_t IPFIX_MAGIC = 10;

/** Decoder for basic IPFX data. */
class IpfixDecoder : public Decoder {
public:
    /**
     * @brief Creates ipfix decoder.
     * @param fd TCP connection file descriptor.
     */
    IpfixDecoder(int fd) : m_fd(fd), m_decoded(), m_part_readed(), m_msg_size(0) {}

    virtual DecodeBuffer &decode() override;

    virtual const char *get_name() const override {
        return "IPFIX";
    }

private:
    /** returns true if there was enough data to read the header or if the header is already read */
    bool read_header();
    /** returns true if there was enough data to read the body */
    bool read_body();
    /** returns true if there was enough data to read to the given amount */
    bool read_until_n(size_t n);

    int m_fd;
    DecodeBuffer m_decoded;

    ByteVector m_part_readed;
    /** Final size of the whole IPFIX message. When 0 IPIFX header is not fully read. */
    size_t m_msg_size;
};

} // namespace tcp_in

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
    IpfixDecoder(int fd) : m_fd(fd), m_decoded() {}

    virtual DecodeBuffer &decode() override;

    virtual const char *get_name() const override {
        return "IPFIX";
    }

private:
    int m_fd;
    DecodeBuffer m_decoded;
};

} // namespace tcp_in

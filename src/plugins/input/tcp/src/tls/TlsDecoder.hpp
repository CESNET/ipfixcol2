/**
 * \file
 * \author Jakub Antonín Štigler <xstigl00@cesnet.cz>
 * \brief TLS decoder for IPFIX plugin. (header file)
 * \date 2025
 *
 * Copyright: (C) 2023 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <vector>

#include "../Decoder.hpp"
#include "../UniqueFd.hpp"
#include "SslCtx.hpp"
#include "Ssl.hpp"

namespace tcp_in {
namespace tls {

// TLS content type handshake
/** Identifies data for which `TlsDecoder` should be used. */
constexpr std::uint8_t TLS_MAGIC = 22;

/** Decoder for TLS connections. */
class TlsDecoder : public Decoder {
public:
    TlsDecoder(SslCtx &ctx, int fd);

    virtual DecodeBuffer &decode() override;

    virtual const char *get_name() const override { return "TLS"; }

private:
    Ssl m_ssl;
    bool m_handshake_complete = false;
    DecodeBuffer m_decoded;
    std::vector<std::uint8_t> m_buffer;
};

} // namespace tls
} // namespace tcp_in

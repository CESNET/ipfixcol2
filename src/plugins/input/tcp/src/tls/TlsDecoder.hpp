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
    /**
     * @brief Create new TLS decoder with shared context.
     * @param ctx Initialized shared context. Its configuration is not modified.
     * @param fd Stream for TLS communication (Usually TCP).
     */
    TlsDecoder(SslCtx &ctx, int fd);

    /**
     * @brief Reads all available data from TLS stream and returns buffer with decoded messages.
     * @returns Buffer with decoded messages.
     */
    virtual DecodeBuffer &decode() override;

    /**
     * @brief Gets the name of the decoder.
     * @return returns `"TLS"`.
     */
    virtual const char *get_name() const override { return "TLS"; }

private:
    Ssl m_ssl;
    bool m_handshake_complete = false;
    DecodeBuffer m_decoded;
    std::vector<std::uint8_t> m_buffer;
};

} // namespace tls
} // namespace tcp_in

/**
 * \file
 * \author Jakub Antonín Štigler <xstigl00@cesnet.cz>
 * \brief Factory for tls connections. (header file)
 * \date 2025
 *
 * Copyright: (C) 2023 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "../Config.hpp"
#include "../Decoder.hpp"
#include "SslCtx.hpp"

namespace tcp_in {
namespace tls {

/** Factory for `TlsDecoder`s. Holds shared data. */
class DecoderFactory {
public:
    /**
     * @brief Create new tls decoder factory using certificate and private key in the given file.
     * Note that this may prompt the user for password.
     * @throws `std::runtime_error` on failure.
     */
    DecoderFactory(const Config &conf);

    /**
     * @brief Create new tls decoder.
     * @param fd Stream for tls communication. (Usually TCP).
     * @return TLS decoder.
     * @throws `std::runtime_exception` if initialization of the decoder fails.
     */
    std::unique_ptr<Decoder> create(int fd);

private:
    SslCtx m_ctx;
};

} // namespace tls
} // namespace tcp_in


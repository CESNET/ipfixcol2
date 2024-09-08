/**
 * \file
 * \author Jakub Antonín Štigler <xstigl00@stud.fit.vutbr.cz>
 * \brief Factory for creating decoders (header file)
 * \date 2024
 *
 * Copyright: (C) 2023 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <memory> // std::unique_ptr

#include "Decoder.hpp" // Decoder

namespace tcp_in {

/** Factory for TCP decoders. */
class DecoderFactory {
public:
    DecoderFactory();

    /**
     * @brief Detects the type of decoder that should be used to decode the given stream and
     * constructs it. This function may block if the decoder cannot be determined without recieving
     * more data.
     * @param fd TCP stream file descriptor
     * @throws std::runtime_error on socket error or if no decoder matches the data
     * @return Instance of the correct decoder, nullptr if there is not enough data to decide the decoder yet
     */
    std::unique_ptr<Decoder> detect_decoder(int fd);

private:
    std::unique_ptr<Decoder> create_ipfix_decoder(int fd);
    std::unique_ptr<Decoder> create_lz4_decoder(int fd);
};

} // namespace tcp_in

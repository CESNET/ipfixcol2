/**
 * \file
 * \author Jakub Antonín Štigler <xstigl00@stud.fit.vutbr.cz>
 * \brief Interface for decoders (header file)
 * \date 2024
 *
 * Copyright: (C) 2023 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "DecodeBuffer.hpp" // DecodeBuffer

namespace tcp_in {

/** Interface for IPFIX message decoders. */
class Decoder {
public:
    /**
     * @brief Reads all available data from TCP stream and returns buffer with decoded messages.
     * @returns Buffer with decoded messages.
     */
    virtual DecodeBuffer &decode() = 0;

    /** Gets the name of the decoder. */
    virtual const char *get_name() const = 0;

    virtual ~Decoder() {}
};

} // namespace tcp_in

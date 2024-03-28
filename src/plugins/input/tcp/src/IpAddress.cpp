/**
 * \file
 * \author Jakub Antonín Štigler <xstigl00@stud.fit.vutbr.cz>
 * \brief IpAddress type (source file)
 * \date 2024
 *
 * Copyright: (C) 2023 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "IpAddress.hpp"

#include <stdexcept>    // invalid_argument
#include <string>       // string
#include <sys/socket.h> // AF_INET, AF_INET6
#include <arpa/inet.h>  // inet_pton

namespace tcp_in {

IpAddress::IpAddress(const char *adr) {
    if (inet_pton(AF_INET, adr, &v4) == 1) {
        version = IpVersion::IP4;
        return;
    }

    if (inet_pton(AF_INET6, adr, &v6) == 1) {
        version = IpVersion::IP6;
        return;
    }

    throw std::invalid_argument("Invalid ip address string: " + std::string(adr));
}

} // namespace tcp_in


/**
 * \file
 * \author Jakub Antonín Štigler <xstigl00@stud.fit.vutbr.cz>
 * \brief IpAddress type (header file)
 * \date 2024
 *
 * Copyright: (C) 2023 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <netinet/in.h> // in_addr, in6_addr
#include <sys/socket.h> // AF_INET, AF_INET6

namespace tcp_in {

/** IP address version (IPv4 or IPv6) */
enum class IpVersion {
    /** Ip address version 4 (IP4 == AF_INET) */
    IP4 = AF_INET,
    /** Ip address version 6 (IP6 == AF_INET6) */
    IP6 = AF_INET6,
};

struct IpAddress {
    /**
     * @brief Tries to parse the IP address from the given C string
     * @param adr string with the IP address to parse
     * @throws std::invalid_argument if the string isn't valid IP address
     */
    IpAddress(const char *adr);

    /** Creates IPv4 address */
    IpAddress(in_addr v4) : version(IpVersion::IP4), v4(v4) {}

    /** Creates IPv6 address */
    IpAddress(in6_addr v6) : version(IpVersion::IP6), v6(v6) {}

    /** Version of ip address, specifies where is the valid value (`.v4` or `.v6`) */
    IpVersion version;
    union {
        struct in_addr v4;
        struct in6_addr v6;
    };
};

} // namespace tcp_in

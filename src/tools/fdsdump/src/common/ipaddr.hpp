
#pragma once

#include <arpa/inet.h>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include <iostream>

namespace fdsdump {

/**
 * @brief IPv4/IPv6 address representation
 */
union IPAddr {
    uint8_t  u8[16];
    uint16_t u16[8];
    uint32_t u32[4];
    uint64_t u64[2];

    /**
     * @brief Default constructor (uninitialized)
     */
    IPAddr() = default;

    /**
     * @brief Zeroes constructor
     */
    static IPAddr zero()
    {
        IPAddr ip;
        memset(&ip, 0, sizeof(ip));
        return ip;
    }

    /**
     * @brief Create an IPv4/IPv6 address from a string
     * @param[in] str  IP address in text form
     * @throw std::invalid_argument if the IP address is not valid
     */
    explicit IPAddr(const std::string &str);

    /**
     * @brief Create an IPv4 address (from 4 bytes array)
     */
    static IPAddr ip4(const uint8_t ip4[4])
    {
        IPAddr ip;
        ip.u64[0] = 0;
        ip.u32[2] = htonl(0x0000FFFF);
        std::memcpy(&ip.u8[12], &ip4[0], 4U);
        return ip;
    }

    /**
     * @brief Create an IPv6 address (from 16 bytes array)
     */
    static IPAddr ip6(const uint8_t ip6[16])
    {
        IPAddr ip;
        std::memcpy(&ip.u8[0], &ip6[0], 16U);
        return ip;
    }

    /**
     * @brief Check if the address is IPv4
     */
    bool
    is_ip4() const
    {
        return u64[0] == 0 && u32[2] == htonl(0x0000FFFF);
    };

    /**
     * @brief Check if the address is IPv6
     */
    bool
    is_ip6() const
    {
        return !is_ip4();
    };

    /**
     * @brief Get IPv4 address as one 32b number
     * @returns Address in network byte order.
     */
    uint32_t get_ip4_as_uint() const
    {
        return u32[3];
    };

    /**
     * @brief Get pointer to byte array of IPv4 address
     * @note Address is in network byte order!
     * @returns Pointer to the first byte of IP address data structure.
     */
    uint8_t *
    get_ip4_as_bytes()
    {
        return &u8[12];
    };

    /**
     * @brief Get pointer to byte array of IPv6 address
     * @note Address is in network byte order!
     * @returns Pointer to the first byte of IP address data structure.
     */
    uint8_t *
    get_ip6_as_bytes()
    {
        return &u8[0];
    };

    /**
     * @brief Convert IP address to string
     */
    std::string to_string() const;

    // Standard comparison operators
    inline bool
    operator==(const IPAddr &other) const
    {
        return memcmp(this, &other, sizeof(*this)) == 0;
    };

    inline bool
    operator!=(const IPAddr &other) const
    {
        return memcmp(this, &other, sizeof(*this)) != 0;
    };

    inline bool
    operator<(const IPAddr &other) const
    {
        return memcmp(this, &other, sizeof(*this)) < 0;
    };

    inline bool
    operator>(const IPAddr &other) const
    {
        return memcmp(this, &other, sizeof(*this)) > 0;
    };

    inline bool
    operator<=(const IPAddr &other) const
    {
        return memcmp(this, &other, sizeof(*this)) <= 0;
    };

    inline bool
    operator>=(const IPAddr &other) const
    {
        return memcmp(this, &other, sizeof(*this)) >= 0;
    };
};

static_assert(sizeof(IPAddr) == 16U, "Unexpected union size!");

} // fdsdump

/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Aggregator field value representation
 *
 * Copyright: (C) 2024 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <common/ipaddr.hpp>

#include <cstdint>
#include <cstring>
#include <string>

namespace fdsdump {
namespace aggregator {

/** @brief The possible data types a view value can be. */
enum class DataType {
    Unassigned,
    IPAddress,
    IPv4Address,
    IPv6Address,
    MacAddress,
    Unsigned8,
    Signed8,
    Unsigned16,
    Signed16,
    Unsigned32,
    Signed32,
    Unsigned64,
    Signed64,
    DateTime,
    String128B,
    Octets128B,
    VarString,
};

/**
 * @brief Get a string representation of the enum value
 */
std::string
data_type_to_str(DataType data_type);

/**
 * @brief Representation of a variable-length string
 */
struct __attribute__((packed)) VarString {
    uint32_t len;
    char text[1];

    friend bool operator==(const VarString& a, const VarString& b) {
        return a.len == b.len && std::memcmp(a.text, b.text, a.len);
    }

    friend bool operator<(const VarString& a, const VarString& b) {
        int res = std::memcmp(a.text, b.text, std::min(a.len, b.len));
        if (res < 0) {
            return true;
        } else if (res > 0) {
            return false;
        } else {
            return a.len < b.len;
        }
    }

    friend bool operator>(const VarString& a, const VarString& b) {
        int res = std::memcmp(a.text, b.text, std::min(a.len, b.len));
        if (res > 0) {
            return true;
        } else if (res < 0) {
            return false;
        } else {
            return a.len > b.len;
        }
    }
};

/** @brief The possible view value forms. */
union Value {
    IPAddr ip;
    uint8_t ipv4[4];
    uint8_t ipv6[16];
    uint8_t mac[6];
    uint8_t u8;
    uint16_t u16;
    uint32_t u32;
    uint64_t u64;
    uint64_t ts_millisecs;
    int8_t i8;
    int16_t i16;
    int32_t i32;
    int64_t i64;
    char str[128];
    VarString varstr;
};

/**
 * @brief A view providing accessors to the value of the Value union
 */
class ValueView {
public:
    /**
     * @brief Construct a view of the aggregator value
     *
     * @param data_type  The data type of the value
     * @param value  Reference to the value
     */
    ValueView(DataType data_type, Value &value);

    /**
     * @brief Retrieve a reference to the viewed value
     */
    Value &value() const { return m_value; }

    /**
     * @brief View the value as an unsigned 8-bit integer
     */
    uint8_t
    as_u8() const;

    /**
     * @brief View the value as an unsigned 16-bit integer
     */
    uint16_t
    as_u16() const;

    /**
     * @brief View the value as an unsigned 32-bit integer
     */
    uint32_t
    as_u32() const;

    /**
     * @brief View the value as an unsigned 64-bit integer
     */
    uint64_t
    as_u64() const;

    /**
     * @brief View the value as a signed 8-bit integer
     */
    int8_t
    as_i8() const;

    /**
     * @brief View the value as a signed 16-bit integer
     */
    int16_t
    as_i16() const;

    /**
     * @brief View the value as a signed 32-bit integer
     */
    int32_t
    as_i32() const;

    /**
     * @brief View the value as a signed 64-bit integer
     */
    int64_t
    as_i64() const;

    /**
     * @brief View the value as an IP address
     */
    IPAddr
    as_ip();

private:
    DataType m_data_type;
    Value &m_value;
};

} // aggregator
} // fdsdump

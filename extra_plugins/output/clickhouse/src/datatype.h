/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Functions specific to individual data types
 * @date 2025
 *
 * Copyright(c) 2025 CESNET z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "clickhouse.h"

#include <fmt/core.h>
#include <libfds.h>

#include <stdexcept>
#include <string>
#include <variant>


/** An intermediary data type used for conversions between IPFIX and ClickHouse types */
enum class DataType {
    Invalid,

    // Lowest to highest precision. Keep it that way!
    // This is used to simplify upcasting in the unify_type function.
    // e.g. std::max(Int8, Int16) = Int16
    Int8,
    Int16,
    Int32,
    Int64,

    UInt8,
    UInt16,
    UInt32,
    UInt64,

    Float32,
    Float64,

    IP, // IP address regardless of version
    IPv4,
    IPv6,

    String,
    OctetArray,

    DatetimeSecs,
    DatetimeMillisecs,
    DatetimeMicrosecs,
    DatetimeNanosecs,

    Mac,
};

/**
 * @brief Simple in_addr wrapper
 */
struct IP4Addr : public in_addr {
    IP4Addr() {
        s_addr = 0;
    }

    IP4Addr(in_addr addr) : in_addr(addr) {}

    friend bool operator==(const IP4Addr &a, const IP4Addr &b) {
        return a.s_addr == b.s_addr;
    }

    friend bool operator!=(const IP4Addr &a, const IP4Addr &b) {
        return !(a == b);
    }
};

/**
 * @brief Simple in6_addr wrapper
 */
struct IP6Addr : public in6_addr {
    IP6Addr() {
        std::memset(s6_addr, 0, sizeof(s6_addr));
    }

    IP6Addr(in6_addr addr) : in6_addr(addr) {}

    friend bool operator==(const IP6Addr &a, const IP6Addr &b) {
        return std::memcmp(&a.s6_addr, &b.s6_addr, sizeof(a.s6_addr)) == 0;
    }

    friend bool operator!=(const IP6Addr &a, const IP6Addr &b) {
        return !(a == b);
    }
};


/** Formatter for the DataType enum */
template <> struct fmt::formatter<DataType> : formatter<string_view> {
    auto format(DataType type, format_context& ctx) const -> format_context::iterator {
        const std::string_view name = [=]() {
            switch (type) {
            case DataType::Invalid: return "Invalid";
            case DataType::Int8: return "Int8";
            case DataType::Int16: return "Int16";
            case DataType::Int32: return "Int32";
            case DataType::Int64: return "Int64";
            case DataType::UInt8: return "UInt8";
            case DataType::UInt16: return "UInt16";
            case DataType::UInt32: return "UInt32";
            case DataType::UInt64: return "UInt64";
            case DataType::IP: return "IP";
            case DataType::IPv4: return "IPv4";
            case DataType::IPv6: return "IPv6";
            case DataType::String: return "String";
            case DataType::DatetimeNanosecs: return "DatetimeNanosecs";
            case DataType::DatetimeMicrosecs: return "DatetimeMicrosecs";
            case DataType::DatetimeMillisecs: return "DatetimeMillisecs";
            case DataType::DatetimeSecs: return "DatetimeSecs";
            case DataType::Mac: return "Mac";
            case DataType::Float32: return "Float32";
            case DataType::Float64: return "Float64";
            case DataType::OctetArray: return "OctetArray";
            }
            return "???";
        }();
        return formatter<string_view>::format(name, ctx);
    }
};

/** Formatter for the iemgr element type */
template <> struct fmt::formatter<fds_iemgr_element_type> : formatter<string_view> {
    auto format(fds_iemgr_element_type type, format_context& ctx) const -> format_context::iterator {
        const char *name = fds_iemgr_type2str(type);
        if (!name) {
            name = "???";
        }
        return formatter<string_view>::format(name, ctx);
    }
};

/**
 * @brief Get intermediary data type for a corresponding IPFIX element type
 *
 * @param type The IPFIX type
 * @return The intermediary data type
 */
DataType type_from_ipfix(fds_iemgr_element_type type);

/**
 * @brief Get ClickHouse data type for the intermediary data type
 *
 * @param type The intermediary data type
 * @param nullable Whether the type is nullable or not
 * @return The ClickHouse data type
 */
std::string type_to_clickhouse(DataType type, bool nullable);

/**
 * @brief Find an intermediary data type that can be used to store all the possible data types of the alias
 *
 * @param alias  The iemgr alias
 * @return The common data type
 */
DataType find_common_type(const fds_iemgr_alias &alias);


using ValueVariant = std::variant<
    std::monostate,
    uint8_t,
    uint16_t,
    uint32_t,
    uint64_t,
    int8_t,
    int16_t,
    int32_t,
    int64_t,
    IP4Addr,
    IP6Addr,
    float,
    double,
    std::string_view
>;

/**
 * @brief Make a ClickHouse column that is able to store values of the supplied data type
 *
 * @param type The data type
 * @param nullable Whether the type is nullable or not
 * @return The ClickHouse column object
 */
std::shared_ptr<clickhouse::Column> make_column(DataType type, bool nullable);

/**
 * @brief An error thrown when the conversion from IPFIX to ClickHouse representation fails.
 */
class ConversionError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

/**
 * Retrieves the value of a specific data type from the given field.
 *
 * @param type The data type to retrieve.
 * @param field The field from which the value is extracted.
 * @return A variant containing the value of the specified data type.
 *
 * @throws ConversionError if the conversion fails.
 */
ValueVariant get_value(DataType type, fds_drec_field& field);

/**
 * Writes a value to a ClickHouse column.
 *
 * @param type The data type of the value.
 * @param nullable Indicates whether the column allows null values.
 * @param column The ClickHouse column to which the value is written.
 * @param value A pointer to the value to be written.
 */
void write_to_column(DataType type, bool nullable, clickhouse::Column& column, ValueVariant* value);

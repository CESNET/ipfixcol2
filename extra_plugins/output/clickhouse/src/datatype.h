/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Functions specific to individual data types
 * @date 2024
 *
 * Copyright(c) 2024 CESNET z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "clickhouse.h"

#include <fmt/core.h>
#include <libfds.h>

#include <functional>
#include <string>
#include <variant>


/** An intermediary data type used for conversions between IPFIX and Clickhouse types */
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

    IP, // IP address regardless of version
    IPv4,
    IPv6,

    String,

    DatetimeSecs,
    DatetimeMillisecs,
    DatetimeMicrosecs,
    DatetimeNanosecs,

    Uuid,
};

enum class DataTypeNullable {
    Nonnullable,
    Nullable,
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
            case DataType::Uuid: return "Uuid";
            }
            return "???";
        }();
        return formatter<string_view>::format(name, ctx);
    }
};

/** Formatter for the iemgr element type */
template <> struct fmt::formatter<fds_iemgr_element_type> : formatter<string_view> {
    auto format(fds_iemgr_element_type type, format_context& ctx) const -> format_context::iterator {
        const std::string_view name = [=]() {
            switch (type) {
            case FDS_ET_OCTET_ARRAY: return "OCTET_ARRAY";
            case FDS_ET_UNSIGNED_8: return "UNSIGNED_8";
            case FDS_ET_UNSIGNED_16: return "UNSIGNED_16";
            case FDS_ET_UNSIGNED_32: return "UNSIGNED_32";
            case FDS_ET_UNSIGNED_64: return "UNSIGNED_64";
            case FDS_ET_SIGNED_8: return "SIGNED_8";
            case FDS_ET_SIGNED_16: return "SIGNED_16";
            case FDS_ET_SIGNED_32: return "SIGNED_32";
            case FDS_ET_SIGNED_64: return "SIGNED_64";
            case FDS_ET_FLOAT_32: return "FLOAT_32";
            case FDS_ET_FLOAT_64: return "FLOAT_64";
            case FDS_ET_BOOLEAN: return "BOOLEAN";
            case FDS_ET_MAC_ADDRESS: return "MAC_ADDRESS";
            case FDS_ET_STRING: return "STRING";
            case FDS_ET_DATE_TIME_SECONDS: return "DATE_TIME_SECONDS";
            case FDS_ET_DATE_TIME_MILLISECONDS: return "DATE_TIME_MILLISECONDS";
            case FDS_ET_DATE_TIME_MICROSECONDS: return "DATE_TIME_MICROSECONDS";
            case FDS_ET_DATE_TIME_NANOSECONDS: return "DATE_TIME_NANOSECONDS";
            case FDS_ET_IPV4_ADDRESS: return "IPV4_ADDRESS";
            case FDS_ET_IPV6_ADDRESS: return "IPV6_ADDRESS";
            case FDS_ET_BASIC_LIST: return "BASIC_LIST";
            case FDS_ET_SUB_TEMPLATE_LIST: return "SUB_TEMPLATE_LIST";
            case FDS_ET_SUB_TEMPLATE_MULTILIST: return "SUB_TEMPLATE_MULTILIST";
            case FDS_ET_UNASSIGNED: return "UNASSIGNED";
            default: return "???";
            }
        }();
        return formatter<string_view>::format(name, ctx);
    }
};

/**
 * @brief Get intermediary data type for a corresponding IPFIX element
 *
 * @param elem The IPFIX element definition
 * @return The intermediary data type
 */
DataType type_from_ipfix(const fds_iemgr_elem *elem);

/**
 * @brief Get Clickhouse data type for the intermediary data type
 *
 * @param type The intermediary data type
 * @param nullable Whether the type is nullable or not
 * @return The Clickhouse data type
 */
std::string type_to_clickhouse(DataType type, DataTypeNullable nullable);

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
    std::string,
    std::pair<uint64_t, uint64_t>
>;

using GetterFn = std::function<void (fds_drec_field field, ValueVariant &value)>;
using ColumnWriterFn = std::function<void (ValueVariant *value, clickhouse::Column &column)>;

GetterFn make_getter(DataType type);

ColumnWriterFn make_columnwriter(DataType type, DataTypeNullable nullable);

/**
 * @brief Make a Clickhouse column that is able to store values of the supplied data type
 *
 * @param type The data type
 * @param nullable Whether the type is nullable or not
 * @return The Clickhouse column object
 */
std::shared_ptr<clickhouse::Column> make_column(DataType type, DataTypeNullable nullable);

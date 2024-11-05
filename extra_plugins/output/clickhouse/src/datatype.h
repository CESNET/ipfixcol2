/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief
 * @date 2024
 *
 * Copyright(c) 2024 CESNET z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <clickhouse/client.h>
#include <functional>
#include <libfds.h>
#include <fmt/core.h>
#include <optional>


/** An intermediary data type used for conversions between IPFIX and Clickhouse types */
enum class DataType {
    Invalid,

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

    DatetimeNanosecs,
    DatetimeMicrosecs,
    DatetimeMillisecs,
    DatetimeSecs,
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
            }
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
            }
        }();
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
 * @brief Get Clickhouse data type for the intermediary data type
 *
 * @param type The intermediary data type
 * @return The Clickhouse data type
 */
std::string type_to_clickhouse(DataType type);

/**
 * @brief Find an intermediary data type that can be used to store all the possible data types of the alias
 *
 * @param alias  The iemgr alias
 * @return The common data type
 */
DataType find_common_type(const fds_iemgr_alias &alias);


using WriterFn = std::function<void (std::optional<fds_drec_field> field, clickhouse::Column &column)>;

/**
 * @brief Make a writer function that is able to write an IPFIX field of the
 * provided data type into a Clickhouse column of a corresponding data type
 *
 * @param type The data type
 * @return The writer function
 */
WriterFn make_writer(DataType type);

/**
 * @brief Make a column factory function that is able to create Clickhouse
 * columns that are able to store values of the supplied data type
 *
 * @param type The data type
 * @return The Clickhouse column object
 */
std::shared_ptr<clickhouse::Column> make_column(DataType type);

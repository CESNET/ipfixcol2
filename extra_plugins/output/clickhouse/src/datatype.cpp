/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief
 * @date 2024
 *
 * Copyright(c) 2024 CESNET z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "datatype.h"
#include "common.h"

DataType type_from_ipfix(fds_iemgr_element_type type)
{
    switch (type) {
    case FDS_ET_STRING: return DataType::String;
    case FDS_ET_SIGNED_8: return DataType::Int8;
    case FDS_ET_SIGNED_16: return DataType::Int16;
    case FDS_ET_SIGNED_32: return DataType::Int32;
    case FDS_ET_SIGNED_64: return DataType::Int64;
    case FDS_ET_UNSIGNED_8: return DataType::UInt8;
    case FDS_ET_UNSIGNED_16: return DataType::UInt16;
    case FDS_ET_UNSIGNED_32: return DataType::UInt32;
    case FDS_ET_UNSIGNED_64: return DataType::UInt64;
    case FDS_ET_IPV4_ADDRESS: return DataType::IPv4;
    case FDS_ET_IPV6_ADDRESS: return DataType::IPv6;
    case FDS_ET_DATE_TIME_SECONDS: return DataType::DatetimeSecs;
    case FDS_ET_DATE_TIME_MILLISECONDS: return DataType::DatetimeMillisecs;
    case FDS_ET_DATE_TIME_MICROSECONDS: return DataType::DatetimeMicrosecs;
    case FDS_ET_DATE_TIME_NANOSECONDS: return DataType::DatetimeNanosecs;
    default: throw Error("Unsupported IPFIX data type {}", type);
    }
}


std::string type_to_clickhouse(DataType type)
{
    switch (type) {
        case DataType::Int8: return "Int8";
        case DataType::Int16: return "Int16";
        case DataType::Int32: return "Int32";
        case DataType::Int64: return "Int64";
        case DataType::UInt8: return "UInt8";
        case DataType::UInt16: return "UInt16";
        case DataType::UInt32: return "UInt32";
        case DataType::UInt64: return "UInt64";
        case DataType::IP: return "IPv6";
        case DataType::IPv4: return "IPv4";
        case DataType::IPv6: return "IPv6";
        case DataType::String: return "String";
        case DataType::DatetimeNanosecs: return "DateTime64(9)";
        case DataType::DatetimeMicrosecs: return "DateTime64(6)";
        case DataType::DatetimeMillisecs: return "DateTime64(3)";
        case DataType::DatetimeSecs: return "Datetime";
        case DataType::Invalid: throw Error("Cannot convert {} to Clickhouse type", type);
    }
}

static DataType unify_type(DataType a, DataType b)
{
    auto is_int = [](DataType t) { return t == DataType::Int8 || t == DataType::Int16 || t == DataType::Int32 || t == DataType::Int64; };
    auto is_uint = [](DataType t) { return t == DataType::UInt8 || t == DataType::UInt16 || t == DataType::UInt32 || t == DataType::UInt64; };
    auto is_ip = [](DataType t) { return t == DataType::IPv4 || t == DataType::IPv6 || t == DataType::IP; };

    if (a == b) {
        return a;
    }
    if (is_int(a) && is_int(b)) {
        return std::max(a, b);
    }
    if (is_uint(a) && is_uint(b)) {
        return std::max(a, b);
    }
    if (is_ip(a) && is_ip(b)) {
        return DataType::IP;
    }

    throw Error("Cannot unify types {} and {}", a, b);
}

DataType find_common_type(const fds_iemgr_alias &alias)
{
    if (alias.sources_cnt == 0) {
        throw Error("Alias \"{}\" has no sources", alias.name);
    }

    DataType common_type = type_from_ipfix(alias.sources[0]->data_type);
    for (size_t i = 1; i < alias.sources_cnt; i++) {
        common_type = unify_type(common_type, type_from_ipfix(alias.sources[i]->data_type));
    }
    return common_type;
}


template <typename UIntType, typename ColumnType>
static void write_uint(std::optional<fds_drec_field> field, clickhouse::Column &column)
{
    uint64_t value = 0;
    if (field) {
        int ret = fds_get_uint_be(field->data, field->size, &value);
        if (ret != FDS_OK) {
            throw Error("fds_get_uint_be() has failed: {}", ret);
        }
    }
    dynamic_cast<ColumnType *>(&column)->Append(static_cast<UIntType>(value));
}

template <typename IntType, typename ColumnType>
static void write_int(std::optional<fds_drec_field> field, clickhouse::Column &column)
{
    int64_t value = 0;
    if (field) {
        int ret = fds_get_int_be(field->data, field->size, &value);
        if (ret != FDS_OK) {
            throw Error("fds_get_int_be() has failed: {}", ret);
        }
    }
    dynamic_cast<ColumnType *>(&column)->Append(static_cast<IntType>(value));
}

static void write_ipv4(std::optional<fds_drec_field> field, clickhouse::Column &column)
{
    return write_uint<uint32_t, clickhouse::ColumnIPv4>(field, column);
}

static void write_ipv6(std::optional<fds_drec_field> field, clickhouse::Column &column)
{
    in6_addr value;
    memset(&value, 0, sizeof(value));
    if (field) {
        int ret = fds_get_ip(field->data, field->size, &value);
        if (ret != FDS_OK) {
            throw Error("fds_get_ip() has failed: {}", ret);
        }
    }
    dynamic_cast<clickhouse::ColumnIPv6 *>(&column)->Append(value);
}

static void write_ip(std::optional<fds_drec_field> field, clickhouse::Column &column)
{
    in6_addr value;
    memset(&value, 0, sizeof(value));
    if (field) {
        int ret;
        if (field->size == 4) {
            value.__in6_u.__u6_addr32[0] = 0;
            value.__in6_u.__u6_addr32[1] = 0;
            value.__in6_u.__u6_addr32[2] = 0;
            ret = fds_get_ip(field->data, field->size, &value.__in6_u.__u6_addr32[3]);
        } else /* if (field->size == 6) */ {
            ret = fds_get_ip(field->data, field->size, &value);
        }
        if (ret != FDS_OK) {
            throw Error("fds_get_ip() has failed: {}", ret);
        }
    }
    dynamic_cast<clickhouse::ColumnIPv6 *>(&column)->Append(value);
}

static void write_string(std::optional<fds_drec_field> field, clickhouse::Column &column)
{
    std::string value;
    if (field) {
        value.resize(field->size);
        int ret = fds_get_string(field->data, field->size, &value[0]);
        if (ret != FDS_OK) {
            throw Error("fds_get_string() has failed: {}", ret);
        }
    }
    dynamic_cast<clickhouse::ColumnString *>(&column)->Append(value);
}

static void write_datetime(std::optional<fds_drec_field> field, clickhouse::Column &column)
{
    uint64_t value = 0;
    if (field) {
        int ret = fds_get_datetime_lp_be(field->data, field->size, field->info->def->data_type, &value);
        if (ret != FDS_OK) {
            throw Error("fds_get_datetime_lp_be() has failed: {}", ret);
        }
        value /= 1000;
    }
    dynamic_cast<clickhouse::ColumnDateTime *>(&column)->Append(value);
}

template <int64_t Divisor = 1>
static void write_datetime64(std::optional<fds_drec_field> field, clickhouse::Column &column)
{
    int64_t value = 0;
    timespec ts;
    if (field) {
        int ret = fds_get_datetime_hp_be(field->data, field->size, field->info->def->data_type, &ts);
        if (ret != FDS_OK) {
            throw Error("fds_get_datetime_hp_be() has failed: {}", ret);
        }
        value = (static_cast<int64_t>(ts.tv_sec) * 1'000'000'000 + static_cast<int64_t>(ts.tv_nsec)) / Divisor;
    }
    dynamic_cast<clickhouse::ColumnDateTime64 *>(&column)->Append(value);
}

WriterFn make_writer(DataType type)
{
    switch (type) {
        case DataType::Invalid: throw Error("Invalid data type");
        case DataType::UInt8: return &write_uint<uint8_t, clickhouse::ColumnUInt8>;
        case DataType::UInt16: return &write_uint<uint16_t, clickhouse::ColumnUInt16>;
        case DataType::UInt32: return &write_uint<uint32_t, clickhouse::ColumnUInt32>;
        case DataType::UInt64: return &write_uint<uint64_t, clickhouse::ColumnUInt64>;
        case DataType::Int8: return &write_int<int8_t, clickhouse::ColumnInt8>;
        case DataType::Int16: return &write_int<int16_t, clickhouse::ColumnInt16>;
        case DataType::Int32: return &write_int<int32_t, clickhouse::ColumnInt32>;
        case DataType::Int64: return &write_int<int64_t, clickhouse::ColumnInt64>;
        case DataType::String: return &write_string;
        case DataType::DatetimeMillisecs: return &write_datetime64<1'000'000>;
        case DataType::DatetimeMicrosecs: return &write_datetime64<1'000>;
        case DataType::DatetimeNanosecs: return &write_datetime64<1>;
        case DataType::DatetimeSecs: return &write_datetime;
        case DataType::IPv4: return &write_ipv4;
        case DataType::IPv6: return &write_ipv6;
        case DataType::IP: return &write_ip;
    }
}

std::shared_ptr<clickhouse::Column> make_column(DataType type)
{
    switch (type) {
        case DataType::Invalid: throw Error("Invalid data type");
        case DataType::UInt8: return std::make_shared<clickhouse::ColumnUInt8>();
        case DataType::UInt16: return std::make_shared<clickhouse::ColumnUInt16>();
        case DataType::UInt32: return std::make_shared<clickhouse::ColumnUInt32>();
        case DataType::UInt64: return std::make_shared<clickhouse::ColumnUInt64>();
        case DataType::Int8: return std::make_shared<clickhouse::ColumnInt8>();
        case DataType::Int16: return std::make_shared<clickhouse::ColumnInt16>();
        case DataType::Int32: return std::make_shared<clickhouse::ColumnInt32>();
        case DataType::Int64: return std::make_shared<clickhouse::ColumnInt64>();
        case DataType::String: return std::make_shared<clickhouse::ColumnString>();
        case DataType::DatetimeMillisecs: return std::make_shared<clickhouse::ColumnDateTime64>(3);
        case DataType::DatetimeMicrosecs: return std::make_shared<clickhouse::ColumnDateTime64>(6);
        case DataType::DatetimeNanosecs: return std::make_shared<clickhouse::ColumnDateTime64>(9);
        case DataType::DatetimeSecs: return std::make_shared<clickhouse::ColumnDateTime>();
        case DataType::IPv4: return std::make_shared<clickhouse::ColumnIPv4>();
        case DataType::IPv6: return std::make_shared<clickhouse::ColumnIPv6>();
        case DataType::IP: return std::make_shared<clickhouse::ColumnIPv6>();
    }
}

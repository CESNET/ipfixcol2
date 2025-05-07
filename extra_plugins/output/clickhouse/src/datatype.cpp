/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Functions specific to individual data types
 * @date 2025
 *
 * Copyright(c) 2025 CESNET z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "datatype.h"
#include "common.h"

template <unsigned Precision> class ColumnDateTime64 : public clickhouse::ColumnDateTime64 {
public:
    ColumnDateTime64() : clickhouse::ColumnDateTime64(Precision) {}
};

DataType type_from_ipfix(fds_iemgr_element_type type)
{
    switch (type) {
    case FDS_ET_STRING:                 return DataType::String;
    case FDS_ET_SIGNED_8:               return DataType::Int8;
    case FDS_ET_SIGNED_16:              return DataType::Int16;
    case FDS_ET_SIGNED_32:              return DataType::Int32;
    case FDS_ET_SIGNED_64:              return DataType::Int64;
    case FDS_ET_UNSIGNED_8:             return DataType::UInt8;
    case FDS_ET_UNSIGNED_16:            return DataType::UInt16;
    case FDS_ET_UNSIGNED_32:            return DataType::UInt32;
    case FDS_ET_UNSIGNED_64:            return DataType::UInt64;
    case FDS_ET_IPV4_ADDRESS:           return DataType::IPv4;
    case FDS_ET_IPV6_ADDRESS:           return DataType::IPv6;
    case FDS_ET_DATE_TIME_SECONDS:      return DataType::DatetimeSecs;
    case FDS_ET_DATE_TIME_MILLISECONDS: return DataType::DatetimeMillisecs;
    case FDS_ET_DATE_TIME_MICROSECONDS: return DataType::DatetimeMicrosecs;
    case FDS_ET_DATE_TIME_NANOSECONDS:  return DataType::DatetimeNanosecs;
    case FDS_ET_MAC_ADDRESS:            return DataType::Mac;
    case FDS_ET_FLOAT_32:               return DataType::Float32;
    case FDS_ET_FLOAT_64:               return DataType::Float64;
    case FDS_ET_OCTET_ARRAY:            return DataType::OctetArray;
    default:                            throw Error("unsupported IPFIX data type {}", type);
    }
}

static DataType unify_type(DataType a, DataType b)
{
    auto is_int = [](DataType t) {
        return t == DataType::Int8
            || t == DataType::Int16
            || t == DataType::Int32
            || t == DataType::Int64;
    };
    auto is_uint = [](DataType t) {
        return t == DataType::UInt8
            || t == DataType::UInt16
            || t == DataType::UInt32
            || t == DataType::UInt64;
    };
    auto is_ip = [](DataType t) {
        return t == DataType::IPv4
            || t == DataType::IPv6
            || t == DataType::IP;
    };
    auto is_datetime = [](DataType t) {
        return t == DataType::DatetimeSecs
            || t == DataType::DatetimeMillisecs
            || t == DataType::DatetimeMicrosecs
            || t == DataType::DatetimeNanosecs;
    };
    auto is_float = [](DataType t) {
        return t == DataType::Float32
            || t == DataType::Float64;
    };

    if (a == b) {
        return a;
    }
    if (is_int(a) && is_int(b)) {
        return std::max(a, b);
    }
    if (is_uint(a) && is_uint(b)) {
        return std::max(a, b);
    }
    if (is_datetime(a) && is_datetime(b)) {
        return std::max(a, b);
    }
    if (is_float(a) && is_float(b)) {
        return std::max(a, b);
    }
    if (is_ip(a) && is_ip(b)) {
        return DataType::IP;
    }

    throw Error("cannot unify types {} and {}", a, b);
}

DataType find_common_type(const fds_iemgr_alias &alias)
{
    if (alias.sources_cnt == 0) {
        throw Error("alias \"{}\" has no sources", alias.name);
    }

    DataType common_type = type_from_ipfix(alias.sources[0]->data_type);
    for (size_t i = 1; i < alias.sources_cnt; i++) {
        common_type = unify_type(common_type, type_from_ipfix(alias.sources[i]->data_type));
    }
    return common_type;
}

namespace getters {

template <typename UIntType>
static UIntType get_uint(fds_drec_field field)
{
    uint64_t value = 0;
    int ret = fds_get_uint_be(field.data, field.size, &value);
    if (ret != FDS_OK) {
        throw Error("fds_get_uint_be() has failed: {}", ret);
    }
    return static_cast<UIntType>(value);
}

template <typename IntType>
static IntType get_int(fds_drec_field field)
{
    int64_t value = 0;
    int ret = fds_get_int_be(field.data, field.size, &value);
    if (ret != FDS_OK) {
        throw Error("fds_get_int_be() has failed: {}", ret);
    }
    return static_cast<IntType>(value);
}

static IP4Addr get_ipv4(fds_drec_field field)
{
    IP4Addr value;
    int ret = fds_get_ip(field.data, field.size, &value.s_addr);
    if (ret != FDS_OK) {
        throw Error("fds_get_ip() has failed: {}", ret);
    }
    return value;
}

static IP6Addr get_ipv6(fds_drec_field field)
{
    IP6Addr value;
    int ret = fds_get_ip(field.data, field.size, &value.s6_addr);
    if (ret != FDS_OK) {
        throw Error("fds_get_ip() has failed: {}", ret);
    }
    return value;
}

static IP6Addr get_ip(fds_drec_field field)
{
    IP6Addr value;
    int ret;
    if (field.size == 4) {
        static constexpr uint8_t IPV4_MAPPED_IPV6_PREFIX[]{
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0xFF, 0xFF};
        std::memcpy(
            reinterpret_cast<uint8_t *>(&value.s6_addr),
            IPV4_MAPPED_IPV6_PREFIX,
            sizeof(IPV4_MAPPED_IPV6_PREFIX));
        ret = fds_get_ip(field.data, field.size, &reinterpret_cast<uint8_t *>(&value.s6_addr)[12]);
    } else {
        ret = fds_get_ip(field.data, field.size, &value.s6_addr);
    }
    if (ret != FDS_OK) {
        throw Error("fds_get_ip() has failed: {}", ret);
    }
    return value;
}

static std::string_view get_string(fds_drec_field field)
{
    return std::string_view(reinterpret_cast<const char *>(field.data), field.size);
}

static std::string_view get_octetarray(fds_drec_field field)
{
    return std::string_view(reinterpret_cast<const char *>(field.data), field.size);
}

static uint64_t get_datetime(fds_drec_field field)
{
    uint64_t value = 0;
    int ret = fds_get_datetime_lp_be(field.data, field.size, field.info->def->data_type, &value);
    if (ret != FDS_OK) {
        throw Error("fds_get_datetime_lp_be() has failed: {}", ret);
    }
    value /= 1000;
    return value;
}

template <int64_t Divisor = 1>
static int64_t get_datetime64(fds_drec_field field)
{
    int64_t value = 0;
    timespec ts;
    int ret = fds_get_datetime_hp_be(field.data, field.size, field.info->def->data_type, &ts);
    if (ret != FDS_OK) {
        throw Error("fds_get_datetime_hp_be() has failed: {}", ret);
    }
    value = (static_cast<int64_t>(ts.tv_sec) * 1'000'000'000 + static_cast<int64_t>(ts.tv_nsec)) / Divisor;
    return value;
}

static uint64_t get_mac(fds_drec_field field)
{
    uint64_t value = 0;
    fds_get_mac(field.data, field.size, reinterpret_cast<uint8_t *>(&value));
    return value;
}

template <typename FloatType>
static FloatType get_float(fds_drec_field field)
{
    double value = 0;
    int ret = fds_get_float_be(field.data, field.size, &value);
    if (ret != FDS_OK) {
        throw Error("fds_get_float_be() has failed: {}", ret);
    }
    return static_cast<FloatType>(value);
}
}

std::string type_to_clickhouse(DataType type, bool nullable)
{
    if (nullable) {
        return "Nullable(" + type_to_clickhouse(type, false) + ")";
    }

    switch (type) {
    case DataType::UInt8:              return "UInt8";
    case DataType::UInt16:             return "UInt16";
    case DataType::UInt32:             return "UInt32";
    case DataType::UInt64:             return "UInt64";
    case DataType::Int8:               return "Int8";
    case DataType::Int16:              return "Int16";
    case DataType::Int32:              return "Int32";
    case DataType::Int64:              return "Int64";
    case DataType::IP:                 return "IPv6";
    case DataType::IPv4:               return "IPv4";
    case DataType::IPv6:               return "IPv6";
    case DataType::String:             return "String";
    case DataType::DatetimeSecs:       return "DateTime";
    case DataType::DatetimeMillisecs:  return "DateTime64(3)";
    case DataType::DatetimeMicrosecs:  return "DateTime64(6)";
    case DataType::DatetimeNanosecs:   return "DateTime64(9)";
    case DataType::Mac:                return "UInt64";
    case DataType::Float32:            return "Float32";
    case DataType::Float64:            return "Float64";
    case DataType::OctetArray:         return "String";
    case DataType::Invalid:            throw std::logic_error("invalid data type");
    }

    throw std::logic_error("unexpected datatype value");
}

std::shared_ptr<clickhouse::Column> make_column(DataType type, bool nullable)
{
    if (nullable) {
        switch (type) {
        case DataType::UInt8:             return std::make_shared<clickhouse::ColumnNullableT<clickhouse::ColumnUInt8>>();
        case DataType::UInt16:            return std::make_shared<clickhouse::ColumnNullableT<clickhouse::ColumnUInt16>>();
        case DataType::UInt32:            return std::make_shared<clickhouse::ColumnNullableT<clickhouse::ColumnUInt32>>();
        case DataType::UInt64:            return std::make_shared<clickhouse::ColumnNullableT<clickhouse::ColumnUInt64>>();
        case DataType::Int8:              return std::make_shared<clickhouse::ColumnNullableT<clickhouse::ColumnInt8>>();
        case DataType::Int16:             return std::make_shared<clickhouse::ColumnNullableT<clickhouse::ColumnInt16>>();
        case DataType::Int32:             return std::make_shared<clickhouse::ColumnNullableT<clickhouse::ColumnInt32>>();
        case DataType::Int64:             return std::make_shared<clickhouse::ColumnNullableT<clickhouse::ColumnInt64>>();
        case DataType::IP:                return std::make_shared<clickhouse::ColumnNullableT<clickhouse::ColumnIPv6>>();
        case DataType::IPv4:              return std::make_shared<clickhouse::ColumnNullableT<clickhouse::ColumnIPv4>>();
        case DataType::IPv6:              return std::make_shared<clickhouse::ColumnNullableT<clickhouse::ColumnIPv6>>();
        case DataType::String:            return std::make_shared<clickhouse::ColumnNullableT<clickhouse::ColumnString>>();
        case DataType::DatetimeSecs:      return std::make_shared<clickhouse::ColumnNullableT<clickhouse::ColumnDateTime>>();
        case DataType::DatetimeMillisecs: return std::make_shared<clickhouse::ColumnNullableT<ColumnDateTime64<3>>>();
        case DataType::DatetimeMicrosecs: return std::make_shared<clickhouse::ColumnNullableT<ColumnDateTime64<6>>>();
        case DataType::DatetimeNanosecs:  return std::make_shared<clickhouse::ColumnNullableT<ColumnDateTime64<9>>>();
        case DataType::Mac:               return std::make_shared<clickhouse::ColumnNullableT<clickhouse::ColumnUInt64>>();
        case DataType::Float32:           return std::make_shared<clickhouse::ColumnNullableT<clickhouse::ColumnFloat32>>();
        case DataType::Float64:           return std::make_shared<clickhouse::ColumnNullableT<clickhouse::ColumnFloat64>>();
        case DataType::OctetArray:        return std::make_shared<clickhouse::ColumnNullableT<clickhouse::ColumnString>>();
        case DataType::Invalid:           throw std::logic_error("invalid data type");
        }
    } else {
        switch (type) {
        case DataType::UInt8:             return std::make_shared<clickhouse::ColumnUInt8>();
        case DataType::UInt16:            return std::make_shared<clickhouse::ColumnUInt16>();
        case DataType::UInt32:            return std::make_shared<clickhouse::ColumnUInt32>();
        case DataType::UInt64:            return std::make_shared<clickhouse::ColumnUInt64>();
        case DataType::Int8:              return std::make_shared<clickhouse::ColumnInt8>();
        case DataType::Int16:             return std::make_shared<clickhouse::ColumnInt16>();
        case DataType::Int32:             return std::make_shared<clickhouse::ColumnInt32>();
        case DataType::Int64:             return std::make_shared<clickhouse::ColumnInt64>();
        case DataType::IP:                return std::make_shared<clickhouse::ColumnIPv6>();
        case DataType::IPv4:              return std::make_shared<clickhouse::ColumnIPv4>();
        case DataType::IPv6:              return std::make_shared<clickhouse::ColumnIPv6>();
        case DataType::String:            return std::make_shared<clickhouse::ColumnString>();
        case DataType::DatetimeSecs:      return std::make_shared<clickhouse::ColumnDateTime>();
        case DataType::DatetimeMillisecs: return std::make_shared<ColumnDateTime64<3>>();
        case DataType::DatetimeMicrosecs: return std::make_shared<ColumnDateTime64<6>>();
        case DataType::DatetimeNanosecs:  return std::make_shared<ColumnDateTime64<9>>();
        case DataType::Mac:               return std::make_shared<clickhouse::ColumnUInt64>();
        case DataType::Float32:           return std::make_shared<clickhouse::ColumnFloat32>();
        case DataType::Float64:           return std::make_shared<clickhouse::ColumnFloat64>();
        case DataType::OctetArray:        return std::make_shared<clickhouse::ColumnString>();
        case DataType::Invalid:           throw std::logic_error("invalid data type");
        }
    }

    throw std::logic_error("unexpected datatype value");
}

ValueVariant get_value(DataType type, fds_drec_field& field)
{
    switch (type) {
    case DataType::UInt8:             return getters::get_uint<uint8_t>(field);
    case DataType::UInt16:            return getters::get_uint<uint16_t>(field);
    case DataType::UInt32:            return getters::get_uint<uint32_t>(field);
    case DataType::UInt64:            return getters::get_uint<uint64_t>(field);
    case DataType::Int8:              return getters::get_int<int8_t>(field);
    case DataType::Int16:             return getters::get_int<int16_t>(field);
    case DataType::Int32:             return getters::get_int<int32_t>(field);
    case DataType::Int64:             return getters::get_int<int64_t>(field);
    case DataType::IP:                return getters::get_ip(field);
    case DataType::IPv4:              return getters::get_ipv4(field);
    case DataType::IPv6:              return getters::get_ipv6(field);
    case DataType::String:            return getters::get_string(field);
    case DataType::DatetimeSecs:      return getters::get_datetime(field);
    case DataType::DatetimeMillisecs: return getters::get_datetime64<1'000'000>(field);
    case DataType::DatetimeMicrosecs: return getters::get_datetime64<1'000>(field);
    case DataType::DatetimeNanosecs:  return getters::get_datetime64<1>(field);
    case DataType::Mac:               return getters::get_mac(field);
    case DataType::Float32:           return getters::get_float<float>(field);
    case DataType::Float64:           return getters::get_float<double>(field);
    case DataType::OctetArray:        return getters::get_octetarray(field);
    case DataType::Invalid:           throw std::logic_error("invalid data type");
    }

    throw std::logic_error("unexpected datatype value");
}

template <typename ColumnT, typename ValueT>
static inline void column_append_nullable(clickhouse::Column& column, ValueVariant* value)
{
    auto *concrete_col = reinterpret_cast<clickhouse::ColumnNullableT<ColumnT> *>(&column);
    concrete_col->Append(value ? std::optional<ValueT>{std::get<ValueT>(*value)} : std::nullopt);
}

template <typename ColumnT, typename ValueT>
static inline void column_append(clickhouse::Column& column, ValueVariant* value)
{
    auto *concrete_col = reinterpret_cast<ColumnT *>(&column);
    concrete_col->Append(value ? std::get<ValueT>(*value) : ValueT{});
}

void write_to_column(DataType type, bool nullable, clickhouse::Column& column, ValueVariant* value)
{
    if (nullable) {
        switch (type) {
        case DataType::UInt8:             column_append_nullable<clickhouse::ColumnUInt8, uint8_t>(column, value); return;
        case DataType::UInt16:            column_append_nullable<clickhouse::ColumnUInt16, uint16_t>(column, value); return;
        case DataType::UInt32:            column_append_nullable<clickhouse::ColumnUInt32, uint32_t>(column, value); return;
        case DataType::UInt64:            column_append_nullable<clickhouse::ColumnUInt64, uint64_t>(column, value); return;
        case DataType::Int8:              column_append_nullable<clickhouse::ColumnInt8, int8_t>(column, value); return;
        case DataType::Int16:             column_append_nullable<clickhouse::ColumnInt16, int16_t>(column, value); return;
        case DataType::Int32:             column_append_nullable<clickhouse::ColumnInt32, int32_t>(column, value); return;
        case DataType::Int64:             column_append_nullable<clickhouse::ColumnInt64, int64_t>(column, value); return;
        case DataType::IP:                column_append_nullable<clickhouse::ColumnIPv6, IP6Addr>(column, value); return;
        case DataType::IPv4:              column_append_nullable<clickhouse::ColumnIPv4, IP4Addr>(column, value); return;
        case DataType::IPv6:              column_append_nullable<clickhouse::ColumnIPv6, IP6Addr>(column, value); return;
        case DataType::String:            column_append_nullable<clickhouse::ColumnString, std::string_view>(column, value); return;
        case DataType::DatetimeSecs:      column_append_nullable<clickhouse::ColumnDateTime, uint64_t>(column, value); return;
        case DataType::DatetimeMillisecs: column_append_nullable<ColumnDateTime64<3>, int64_t>(column, value); return;
        case DataType::DatetimeMicrosecs: column_append_nullable<ColumnDateTime64<6>, int64_t>(column, value); return;
        case DataType::DatetimeNanosecs:  column_append_nullable<ColumnDateTime64<9>, int64_t>(column, value); return;
        case DataType::Mac:               column_append_nullable<clickhouse::ColumnUInt64, uint64_t>(column, value); return;
        case DataType::Float32:           column_append_nullable<clickhouse::ColumnFloat32, float>(column, value); return;
        case DataType::Float64:           column_append_nullable<clickhouse::ColumnFloat64, double>(column, value); return;
        case DataType::OctetArray:        column_append_nullable<clickhouse::ColumnString, std::string_view>(column, value); return;
        case DataType::Invalid:           throw std::logic_error("invalid data type");
        }
    } else {
        switch (type) {
        case DataType::UInt8:             column_append<clickhouse::ColumnUInt8, uint8_t>(column, value); return;
        case DataType::UInt16:            column_append<clickhouse::ColumnUInt16, uint16_t>(column, value); return;
        case DataType::UInt32:            column_append<clickhouse::ColumnUInt32, uint32_t>(column, value); return;
        case DataType::UInt64:            column_append<clickhouse::ColumnUInt64, uint64_t>(column, value); return;
        case DataType::Int8:              column_append<clickhouse::ColumnInt8, int8_t>(column, value); return;
        case DataType::Int16:             column_append<clickhouse::ColumnInt16, int16_t>(column, value); return;
        case DataType::Int32:             column_append<clickhouse::ColumnInt32, int32_t>(column, value); return;
        case DataType::Int64:             column_append<clickhouse::ColumnInt64, int64_t>(column, value); return;
        case DataType::IP:                column_append<clickhouse::ColumnIPv6, IP6Addr>(column, value); return;
        case DataType::IPv4:              column_append<clickhouse::ColumnIPv4, IP4Addr>(column, value); return;
        case DataType::IPv6:              column_append<clickhouse::ColumnIPv6, IP6Addr>(column, value); return;
        case DataType::String:            column_append<clickhouse::ColumnString, std::string_view>(column, value); return;
        case DataType::DatetimeSecs:      column_append<clickhouse::ColumnDateTime, uint64_t>(column, value); return;
        case DataType::DatetimeMillisecs: column_append<ColumnDateTime64<3>, int64_t>(column, value); return;
        case DataType::DatetimeMicrosecs: column_append<ColumnDateTime64<6>, int64_t>(column, value); return;
        case DataType::DatetimeNanosecs:  column_append<ColumnDateTime64<9>, int64_t>(column, value); return;
        case DataType::Mac:               column_append<clickhouse::ColumnUInt64, uint64_t>(column, value); return;
        case DataType::Float32:           column_append<clickhouse::ColumnFloat32, float>(column, value); return;
        case DataType::Float64:           column_append<clickhouse::ColumnFloat64, double>(column, value); return;
        case DataType::OctetArray:        column_append<clickhouse::ColumnString, std::string_view>(column, value); return;
        case DataType::Invalid:           throw std::logic_error("invalid data type");
        }
    }

    throw std::logic_error("unexpected datatype value");
}

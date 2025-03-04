/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Functions specific to individual data types
 * @date 2024
 *
 * Copyright(c) 2024 CESNET z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <type_traits>

#include "datatype.h"
#include "common.h"

template <unsigned Precision> class ColumnDateTime64 : public clickhouse::ColumnDateTime64 {
public:
    ColumnDateTime64() : clickhouse::ColumnDateTime64(Precision) {}
};

DataType type_from_ipfix(const fds_iemgr_elem *elem)
{
    // Special cases
    static constexpr uint32_t PEN_CESNET = 8057;
    static constexpr uint16_t ID_FLOWUUID = 1300;
    if (elem->scope->pen == PEN_CESNET && elem->id == ID_FLOWUUID) {
        return DataType::Uuid;
    }

    // Otherwise
    switch (elem->data_type) {
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
    default:                            throw Error("unsupported IPFIX data type {}", elem->data_type);
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

    DataType common_type = type_from_ipfix(alias.sources[0]);
    for (size_t i = 1; i < alias.sources_cnt; i++) {
        common_type = unify_type(common_type, type_from_ipfix(alias.sources[i]));
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
    int ret = fds_get_ip(field.data, field.size, &value);
    if (ret != FDS_OK) {
        throw Error("fds_get_ip() has failed: {}", ret);
    }
    return value;
}

static IP6Addr get_ipv6(fds_drec_field field)
{
    IP6Addr value;
    int ret = fds_get_ip(field.data, field.size, &value);
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
            reinterpret_cast<uint8_t *>(&value),
            IPV4_MAPPED_IPV6_PREFIX,
            sizeof(IPV4_MAPPED_IPV6_PREFIX));
        ret = fds_get_ip(field.data, field.size, &reinterpret_cast<uint8_t *>(&value)[12]);
    } else {
        ret = fds_get_ip(field.data, field.size, &value);
    }
    if (ret != FDS_OK) {
        throw Error("fds_get_ip() has failed: {}", ret);
    }
    return value;
}

static std::string get_string(fds_drec_field field)
{
    std::string value;
    value.resize(field.size);
    int ret = fds_get_string(field.data, field.size, &value[0]);
    if (ret != FDS_OK) {
        throw Error("fds_get_string() has failed: {}", ret);
    }
    return value;
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

static std::pair<uint64_t, uint64_t> get_uuid(fds_drec_field field)
{
    if (field.size != 16) {
        throw Error("invalid uuid field size. expected 16, got {}", field.size);
    }

    return {
        *reinterpret_cast<uint64_t *>(&field.data[0]),
        *reinterpret_cast<uint64_t *>(&field.data[8])
    };
}

}

template<DataType> struct DataTypeTraits {};

template<> struct DataTypeTraits<DataType::UInt8> {
    using ColumnType = clickhouse::ColumnUInt8;
    static constexpr std::string_view ClickhouseTypeName = "UInt8";
    static constexpr auto Getter = &getters::get_uint<uint8_t>;
};

template<> struct DataTypeTraits<DataType::UInt16> {
    using ColumnType = clickhouse::ColumnUInt16;
    static constexpr std::string_view ClickhouseTypeName = "UInt16";
    static constexpr auto Getter = &getters::get_uint<uint16_t>;
};

template<> struct DataTypeTraits<DataType::UInt32> {
    using ColumnType = clickhouse::ColumnUInt32;
    static constexpr std::string_view ClickhouseTypeName = "UInt32";
    static constexpr auto Getter = &getters::get_uint<uint32_t>;
};

template<> struct DataTypeTraits<DataType::UInt64> {
    using ColumnType = clickhouse::ColumnUInt64;
    static constexpr std::string_view ClickhouseTypeName = "UInt64";
    static constexpr auto Getter = &getters::get_uint<uint64_t>;
};

template<> struct DataTypeTraits<DataType::Int8> {
    using ColumnType = clickhouse::ColumnInt8;
    static constexpr std::string_view ClickhouseTypeName = "Int8";
    static constexpr auto Getter = &getters::get_int<int8_t>;
};

template<> struct DataTypeTraits<DataType::Int16> {
    using ColumnType = clickhouse::ColumnInt16;
    static constexpr std::string_view ClickhouseTypeName = "Int16";
    static constexpr auto Getter = &getters::get_int<int16_t>;
};

template<> struct DataTypeTraits<DataType::Int32> {
    using ColumnType = clickhouse::ColumnInt32;
    static constexpr std::string_view ClickhouseTypeName = "Int32";
    static constexpr auto Getter = &getters::get_int<int32_t>;
};

template<> struct DataTypeTraits<DataType::Int64> {
    using ColumnType = clickhouse::ColumnInt64;
    static constexpr std::string_view ClickhouseTypeName = "Int64";
    static constexpr auto Getter = &getters::get_int<int64_t>;
};

template<> struct DataTypeTraits<DataType::IP> {
    using ColumnType = clickhouse::ColumnIPv6;
    static constexpr std::string_view ClickhouseTypeName = "IPv6";
    static constexpr auto Getter = &getters::get_ip;
};

template<> struct DataTypeTraits<DataType::IPv4> {
    using ColumnType = clickhouse::ColumnIPv4;
    static constexpr std::string_view ClickhouseTypeName = "IPv4";
    static constexpr auto Getter = &getters::get_ipv4;
};

template<> struct DataTypeTraits<DataType::IPv6> {
    using ColumnType = clickhouse::ColumnIPv6;
    static constexpr std::string_view ClickhouseTypeName = "IPv6";
    static constexpr auto Getter = &getters::get_ipv6;
};

template<> struct DataTypeTraits<DataType::String> {
    using ColumnType = clickhouse::ColumnString;
    static constexpr std::string_view ClickhouseTypeName = "String";
    static constexpr auto Getter = &getters::get_string;
};

template<> struct DataTypeTraits<DataType::DatetimeSecs> {
    using ColumnType = clickhouse::ColumnDateTime;
    static constexpr std::string_view ClickhouseTypeName = "DateTime";
    static constexpr auto Getter = &getters::get_datetime;
};

template<> struct DataTypeTraits<DataType::DatetimeMillisecs> {
    using ColumnType = ColumnDateTime64<3>;
    static constexpr std::string_view ClickhouseTypeName = "DateTime64(3)";
    static constexpr auto Getter = &getters::get_datetime64<1'000'000>;
};

template<> struct DataTypeTraits<DataType::DatetimeMicrosecs> {
    using ColumnType = ColumnDateTime64<6>;
    static constexpr std::string_view ClickhouseTypeName = "DateTime64(6)";
    static constexpr auto Getter = &getters::get_datetime64<1'000>;
};

template<> struct DataTypeTraits<DataType::DatetimeNanosecs> {
    using ColumnType = ColumnDateTime64<9>;
    static constexpr std::string_view ClickhouseTypeName = "DateTime64(9)";
    static constexpr auto Getter = &getters::get_datetime64<1>;
};

template<> struct DataTypeTraits<DataType::Uuid> {
    using ColumnType = clickhouse::ColumnUUID;
    static constexpr std::string_view ClickhouseTypeName = "UUID";
    static constexpr auto Getter = &getters::get_uuid;
};

template <typename Func>
static void visit(DataType type, Func func)
{
    switch (type) {
    case DataType::UInt8:             func(DataTypeTraits<DataType::UInt8>{});             break;
    case DataType::UInt16:            func(DataTypeTraits<DataType::UInt16>{});            break;
    case DataType::UInt32:            func(DataTypeTraits<DataType::UInt32>{});            break;
    case DataType::UInt64:            func(DataTypeTraits<DataType::UInt64>{});            break;
    case DataType::Int8:              func(DataTypeTraits<DataType::Int8>{});              break;
    case DataType::Int16:             func(DataTypeTraits<DataType::Int16>{});             break;
    case DataType::Int32:             func(DataTypeTraits<DataType::Int32>{});             break;
    case DataType::Int64:             func(DataTypeTraits<DataType::Int64>{});             break;
    case DataType::String:            func(DataTypeTraits<DataType::String>{});            break;
    case DataType::DatetimeMillisecs: func(DataTypeTraits<DataType::DatetimeMillisecs>{}); break;
    case DataType::DatetimeMicrosecs: func(DataTypeTraits<DataType::DatetimeMicrosecs>{}); break;
    case DataType::DatetimeNanosecs:  func(DataTypeTraits<DataType::DatetimeNanosecs>{});  break;
    case DataType::DatetimeSecs:      func(DataTypeTraits<DataType::DatetimeSecs>{});      break;
    case DataType::IPv4:              func(DataTypeTraits<DataType::IPv4>{});              break;
    case DataType::IPv6:              func(DataTypeTraits<DataType::IPv6>{});              break;
    case DataType::IP:                func(DataTypeTraits<DataType::IP>{});                break;
    case DataType::Uuid:              func(DataTypeTraits<DataType::Uuid>{});              break;
    case DataType::Invalid:           throw std::runtime_error("invalid data type");
    }
}

std::shared_ptr<clickhouse::Column> make_column(DataType type, DataTypeNullable nullable) {
    std::shared_ptr<clickhouse::Column> column;
    visit(type, [&](auto traits) {
        if (nullable == DataTypeNullable::Nullable) {
            using ColType = clickhouse::ColumnNullableT<typename decltype(traits)::ColumnType>;
            column = std::make_shared<ColType>();
        } else /* (nullable == DataTypeNullable::Nonnullable) */ {
            using ColType = typename decltype(traits)::ColumnType;
            column = std::make_shared<ColType>();
        }
    });
    return column;
}

GetterFn make_getter(DataType type) {
    GetterFn getter;
    visit(type, [&](auto traits) {
        getter = [](fds_drec_field field, ValueVariant &value) { value = decltype(traits)::Getter(field); };
    });
    return getter;
}

ColumnWriterFn make_columnwriter(DataType type, DataTypeNullable nullable) {
    ColumnWriterFn columnwriter;

    if (nullable == DataTypeNullable::Nullable) {
        visit(type, [&](auto traits) {
            columnwriter = [](ValueVariant *value, clickhouse::Column &column) {
                using ColumnType = clickhouse::ColumnNullableT<typename decltype(traits)::ColumnType>;
                using ValueType = std::invoke_result_t<decltype(decltype(traits)::Getter), fds_drec_field>;
                auto *col = dynamic_cast<ColumnType*>(&column);
                if (!value) {
                    col->Append(std::nullopt);
                } else {
                    col->Append(std::get<ValueType>(*value));
                }
            };
        });

    } else {
        visit(type, [&](auto traits) {
            columnwriter = [](ValueVariant *value, clickhouse::Column &column) {
                using ColumnType = typename decltype(traits)::ColumnType;
                using ValueType = std::invoke_result_t<decltype(decltype(traits)::Getter), fds_drec_field>;
                static const auto ZeroValue = ValueType{};
                auto *col = dynamic_cast<ColumnType*>(&column);
                if (!value) {
                    col->Append(ZeroValue);
                } else {
                    col->Append(std::get<ValueType>(*value));
                }
            };
        });
    }

    return columnwriter;
}

std::string type_to_clickhouse(DataType type, DataTypeNullable nullable) {
    std::string result;
    visit(type, [&](auto traits) {
        result = traits.ClickhouseTypeName;
        if (nullable == DataTypeNullable::Nullable) {
            result = "Nullable(" + result + ")";
        }
    });
    return result;
}

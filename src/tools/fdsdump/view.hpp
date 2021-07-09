#pragma once

#include <string>
#include <vector>
#include <cstdint>

struct IPAddress
{
    uint8_t length;
    uint8_t address[16];
};

enum class DataType
{
    Unassigned,
    IPAddress,
    IPv4Address,
    IPv6Address,
    Unsigned8,
    Signed8,
    Unsigned16,
    Signed16,
    Unsigned32,
    Signed32,
    Unsigned64,
    Signed64,
    DateTime,
    String128B
};

union ViewValue
{
    IPAddress ip;
    uint8_t ipv4[4];
    uint8_t ipv6[16];
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
};

enum class ViewFieldKind
{
    Unassigned,
    VerbatimKey,
    IPv4SubnetKey,
    IPv6SubnetKey,
    BidirectionalIPv4SubnetKey,
    BidirectionalIPv6SubnetKey,
    SourceIPAddressKey,
    DestinationIPAddressKey,
    BidirectionalIPAddressKey,
    BidirectionalPortKey,
    SumAggregate,
    MinAggregate,
    MaxAggregate,
    CountAggregate
};

enum class Direction
{
    Unassigned,
    In,
    Out
};

struct ViewField
{
    std::size_t size;
    std::string name;
    uint32_t pen;
    uint16_t id;
    DataType data_type;
    ViewFieldKind kind;
    Direction direction;
    struct
    {
        uint8_t prefix_length;
    } extra;
};

struct ViewDefinition
{
    bool bidirectional;

    std::vector<ViewField> key_fields;
    std::vector<ViewField> value_fields;
    std::size_t keys_size;
    std::size_t values_size;
};

static inline void
advance_value_ptr(ViewValue *&value, std::size_t value_size)
{
    value = reinterpret_cast<ViewValue *>(reinterpret_cast<uint8_t *>(value) + value_size);
}

ViewValue *
get_value_by_name(ViewDefinition &view_definition, uint8_t *values, const std::string &name);
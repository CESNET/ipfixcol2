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
    Signed64
};

union ViewValue
{
    IPAddress ip;
    uint8_t u8;
    uint16_t u16;
    uint32_t u32;
    uint64_t u64;
    int8_t i8;
    int16_t i16;
    int32_t i32;
    int64_t i64;
};

enum class ViewFieldKind
{
    VerbatimKey,
    SourceIPAddressKey,
    DestinationIPAddressKey,
    IPAddressKey,
    SumAggregate,
    MinAggregate,
    MaxAggregate,
    FlowCount
};

enum class FieldDirection
{
    None,
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
    FieldDirection direction; //TODO
};

struct ViewDefinition
{
    bool bidirectional; //TODO

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
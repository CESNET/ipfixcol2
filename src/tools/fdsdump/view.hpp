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
    Unsigned64,
    Signed64
};

union ViewValue
{
    IPAddress ip;
    uint64_t u64;
    int64_t i64;    
};

enum class ViewFieldKind
{
    Sum,
    Min,
    Max,
    FlowCount
};

struct ViewField
{
    std::size_t size;
    std::string name;
    uint32_t pen;
    uint16_t id;
    DataType data_type;
    ViewFieldKind kind;
};

struct ViewDefinition
{
    bool key_src_ip;
    bool key_dst_ip;
    bool key_src_port;
    bool key_dst_port;
    bool key_protocol;

    std::vector<ViewField> value_fields;
    std::size_t values_size;
};

static inline void
advance_value_ptr(ViewValue *&value, std::size_t value_size)
{
    value = reinterpret_cast<ViewValue *>(reinterpret_cast<uint8_t *>(value) + value_size);
}

ViewValue *
get_value_by_name(ViewDefinition &view_definition, uint8_t *values, const std::string &name);
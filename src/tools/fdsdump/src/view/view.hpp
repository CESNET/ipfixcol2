#pragma once

#include <string>
#include <vector>
#include <cstdint>

#include <libfds.h>

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
};

union ViewValue
{
    IPAddress ip;
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
    BiflowDirectionKey,
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
    size_t size;
    size_t offset;
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
    size_t keys_size;
    size_t values_size;
    bool biflow_enabled;
};

ViewDefinition
make_view_def(const std::string &keys, const std::string &values, fds_iemgr_t *iemgr);

ViewField *
find_field(ViewDefinition &def, const std::string &name);

void
add_field_verbatim(ViewDefinition &view_def, const fds_iemgr_elem *elem);

static inline void
advance_value_ptr(ViewValue *&value, size_t value_size)
{
    value = (ViewValue *) ((uint8_t *) value + value_size);
}

static inline IPAddress
make_ipv4_address(uint8_t *address)
{
    IPAddress ip = {};
    ip.length = 4;
    memcpy(ip.address, address, 4);
    return ip;
}

static inline IPAddress
make_ipv6_address(uint8_t *address)
{
    IPAddress ip = {};
    ip.length = 16;
    memcpy(ip.address, address, 16);
    return ip;
}

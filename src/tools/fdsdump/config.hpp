#pragma once

#include <string>
#include <vector>

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

union Value
{
    IPAddress ip;
    uint64_t u64;
    int64_t i64;    
};

enum class AggregateFieldKind
{
    Sum,
    Min,
    Max,
    FlowCount
};

struct AggregateField
{
    std::size_t size;
    std::string name;
    uint32_t pen;
    uint16_t id;
    DataType data_type;
    AggregateFieldKind kind;
};

struct AggregateConfig
{
    bool key_src_ip;
    bool key_dst_ip;
    bool key_src_port;
    bool key_dst_port;
    bool key_protocol;

    std::vector<AggregateField> value_fields;
    std::size_t values_size;
};

struct Config
{
    std::string input_file;
    std::string input_filter;
    std::string output_filter;
    uint64_t max_input_records;
    AggregateConfig aggregate_config;
    std::string sort_field;
    uint64_t max_output_records;
};

int
config_from_args(int argc, char **argv, Config &config);

#pragma once

#include <array>
#include <vector>
#include <cstdint>
#include <libfds.h>
#include "config.hpp"

constexpr std::size_t BUCKETS_COUNT = 4096 * 100;

static inline void
advance_value_ptr(Value *&value, std::size_t value_size)
{
    value = reinterpret_cast<Value *>(reinterpret_cast<uint8_t *>(value) + value_size);
}

static inline Value *
get_value_by_name(AggregateConfig &config, uint8_t *values, const std::string &name)
{
    Value *value = reinterpret_cast<Value *>(values);
    for (const auto &field : config.value_fields) {
        if (field.name == name) {
            return value;
        }
        advance_value_ptr(value, field.size);
    }
    return nullptr;
}

struct AggregateRecord
{
    AggregateRecord *next;

    struct __attribute__((packed)) Key
    {
        uint8_t protocol;
        IPAddress src_ip;
        uint16_t src_port;
        IPAddress dst_ip;
        uint16_t dst_port;
    } key;

    uint8_t values[];
};

class Aggregator
{
public:
    Aggregator(AggregateConfig config);

    void
    process_record(fds_drec &drec);

    std::vector<AggregateRecord *>
    records() { return m_records; }

private:
    AggregateConfig m_config;
    std::array<AggregateRecord *, BUCKETS_COUNT> m_buckets;
    std::vector<AggregateRecord *> m_records;
};
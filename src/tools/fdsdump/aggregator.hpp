#pragma once

#include <array>
#include <vector>
#include <cstdint>
#include <libfds.h>
#include "config.hpp"

constexpr std::size_t BUCKETS_COUNT = 4096 * 100;

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
    Aggregator(ViewDefinition view_def);

    void
    process_record(fds_drec &drec);

    std::vector<AggregateRecord *>
    records() { return m_records; }

private:
    ViewDefinition m_view_def;
    std::array<AggregateRecord *, BUCKETS_COUNT> m_buckets;
    std::vector<AggregateRecord *> m_records;
};
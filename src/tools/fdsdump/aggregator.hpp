#pragma once

#include <array>
#include <vector>
#include <cstdint>
#include <libfds.h>
#include "config.hpp"

struct AggregateRecord
{
    AggregateRecord *next;
    uint64_t hash;
    uint8_t data[];
};

class Aggregator
{
public:
    Aggregator(ViewDefinition view_def);

    void
    process_record(fds_drec &drec);

    std::vector<AggregateRecord *>
    records() { return m_records; }

    void
    print_debug_info();

private:
    ViewDefinition m_view_def;
    std::size_t m_buckets_count = 1024;
    std::size_t m_records_count = 0;
    std::vector<AggregateRecord *> m_buckets;
    std::vector<AggregateRecord *> m_records;

    void
    aggregate(fds_drec &drec, Direction direction);
};
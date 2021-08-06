#pragma once

#include <array>
#include <vector>
#include <cstdint>
#include <libfds.h>
#include "config.hpp"
#include "aggregatetable.hpp"
#include "sorter.hpp"

class Aggregator
{
public:
    Aggregator(ViewDefinition view_def);

    void
    process_record(fds_drec &drec);

    std::vector<AggregateRecord *> &
    records() { return m_table.records(); }

    void
    merge(Aggregator &other);

    void
    make_top_n(size_t n, CompareFn compare_fn);

private:
    ViewDefinition m_view_def;

    AggregateTable m_table;

    std::vector<uint8_t> m_key_buffer;

    void
    aggregate(fds_drec &drec, Direction direction);
};
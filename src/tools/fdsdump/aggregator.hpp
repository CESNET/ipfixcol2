#pragma once

#include <array>
#include <vector>
#include <cstdint>
#include <libfds.h>
#include "config.hpp"
#include "aggregate_table.hpp"

class Aggregator
{
public:
    Aggregator(ViewDefinition view_def);

    void
    process_record(fds_drec &drec);

    std::vector<AggregateRecord *>
    records() { return m_table.records(); }

    // void
    // print_debug_info();

private:
    ViewDefinition m_view_def;

    AggregateTable m_table;

    void
    aggregate(fds_drec &drec, Direction direction);
};
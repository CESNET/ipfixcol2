#pragma once

#include "common.hpp"
#include "aggregator.hpp"

class AggregateFilter
{
public:
    AggregateFilter(const char *filter_expr);

    bool
    record_passes(aggregate_record_s &record);

private:
    unique_fds_filter_opts m_filter_opts;
    unique_fds_filter m_filter;
};
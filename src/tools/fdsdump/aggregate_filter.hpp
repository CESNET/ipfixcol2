#pragma once

#include "common.hpp"
#include "aggregator.hpp"

class AggregateFilter
{
public:
    AggregateFilter(const char *filter_expr, ViewDefinition view_def);

    bool
    record_passes(AggregateRecord &record);

private:
    unique_fds_filter_opts m_filter_opts;
    unique_fds_filter m_filter;
    ViewDefinition m_view_def;
};
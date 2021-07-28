#pragma once

#include "common.hpp"
#include "aggregator.hpp"


class AggregateFilter
{
public:
    AggregateFilter(const char *filter_expr, ViewDefinition view_def);

    AggregateFilter(AggregateFilter &&) = delete;
    AggregateFilter(const AggregateFilter &) = delete;

    bool
    record_passes(AggregateRecord &record);

private:
    unique_fds_filter_opts m_filter_opts;
    unique_fds_filter m_filter;
    ViewDefinition m_view_def;

    struct Mapping {
        DataType data_type;
        unsigned int offset;
    };

    std::vector<Mapping> m_value_map;

    std::exception_ptr m_exception = nullptr;

    friend int
    lookup_callback(void *user_ctx, const char *name, const char *other_name,
                    int *out_id, int *out_datatype, int *out_flags);

    friend int
    data_callback(void *user_ctx, bool reset_ctx, int id, void *data, fds_filter_value_u *out_value);

    int
    lookup_callback(const char *name, const char *other_name, int *out_id, int *out_datatype, int *out_flags);

    int
    data_callback(bool reset_ctx, int id, void *data, fds_filter_value_u *out_value) noexcept;
};
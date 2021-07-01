#include "aggregate_filter.hpp"

enum : int
{
    PACKETS_ID = 1,
    BYTES_ID = 2,
    FLOWS_ID = 3
};

static int
lookup_callback(void *user_ctx, const char *name_, const char *other_name,
                int *out_id, int *out_datatype, int *out_flags)
{
    std::string name = name_;
    if (name == "packets") {
        *out_id = PACKETS_ID;
        *out_datatype = FDS_FDT_UINT;
    } else if (name == "bytes") {
        *out_id = BYTES_ID;
        *out_datatype = FDS_FDT_UINT;
    } else if (name == "flows") {
        *out_id = FLOWS_ID;
        *out_datatype = FDS_FDT_UINT;
    } else {
        return FDS_ERR_NOTFOUND;
    }
    return FDS_OK;
}

static int
data_callback(void *user_ctx, bool reset_ctx, int id, void *data, fds_filter_value_u *out_value)
{
    ViewDefinition &view_def = *static_cast<ViewDefinition *>(user_ctx);
    AggregateRecord &arec = *static_cast<AggregateRecord *>(data);
    switch (id) {
    case FLOWS_ID:
        out_value->u = get_value_by_name(view_def, arec.data, "flows")->u64;
        break;
    case PACKETS_ID:
        out_value->u = get_value_by_name(view_def, arec.data, "packets")->u64;
        break;
    case BYTES_ID:
        out_value->u = get_value_by_name(view_def, arec.data, "bytes")->u64;
        break;
    default:
        return FDS_ERR_NOTFOUND;
    }
    return FDS_OK;
}

AggregateFilter::AggregateFilter(const char *filter_expr, ViewDefinition view_def)
    : m_view_def(view_def)
{
    int rc;

    m_filter_opts.reset(fds_filter_create_default_opts());
    if (!m_filter_opts) {
        throw std::bad_alloc();
    }

    fds_filter_opts_set_user_ctx(m_filter_opts.get(), &m_view_def);
    fds_filter_opts_set_lookup_cb(m_filter_opts.get(), lookup_callback);
    fds_filter_opts_set_data_cb(m_filter_opts.get(), data_callback);

    fds_filter_t *filter;
    rc = fds_filter_create(&filter, filter_expr, m_filter_opts.get());
    m_filter.reset(filter);
    if (rc != FDS_OK) {
        std::string error = fds_filter_get_error(filter)->msg;
        throw std::runtime_error(error);
    }
}

bool
AggregateFilter::record_passes(AggregateRecord &record)
{
    if (fds_filter_eval(m_filter.get(), &record)) {
        return true;
    } else {
        return false;
    }
}
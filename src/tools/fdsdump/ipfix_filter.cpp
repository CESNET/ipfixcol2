#include "ipfix_filter.hpp"

IPFIXFilter::IPFIXFilter(const char *filter_expr, fds_iemgr_t &iemgr)
    : m_iemgr(iemgr)
{
    int rc;
    fds_ipfix_filter_t *filter;

    rc = fds_ipfix_filter_create(&filter, &iemgr, filter_expr);
    m_filter.reset(filter);
    if (rc != FDS_OK) {
        std::string error{fds_ipfix_filter_get_error(filter)};
        throw std::runtime_error(error);
    }
}

bool
IPFIXFilter::record_passes(fds_drec &drec)
{
    if (fds_ipfix_filter_eval(m_filter.get(), &drec)) {
        return true;
    } else {
        return false;
    }
}
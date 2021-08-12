#pragma once

#include "ipfix/util.hpp"

class IPFIXFilter
{
public:
    IPFIXFilter() {}

    IPFIXFilter(const char *filter_expr, fds_iemgr_t *iemgr);

    bool
    record_passes(fds_drec &drec);

private:
    unique_fds_ipfix_filter m_filter;
};

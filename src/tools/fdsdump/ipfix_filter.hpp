#pragma once

#include "common.hpp"

class IPFIXFilter
{
public:
    IPFIXFilter(const char *filter_expr, fds_iemgr_t &iemgr);

    bool
    record_passes(fds_drec &drec);

private:
    fds_iemgr_t &m_iemgr;
    unique_fds_ipfix_filter m_filter;
};
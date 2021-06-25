#pragma once

#include "common.hpp"

class Reader {
public:
    Reader(const char *filename, fds_iemgr_t &m_iemgr);

    int
    read_record(fds_drec &drec);

    uint64_t
    total_number_of_records();

private:
    fds_iemgr_t &m_iemgr;
    unique_fds_file m_file;
    fds_file_read_ctx m_read_ctx;
};
#pragma once

#include "common.hpp"

class Reader {
public:
    Reader(const char *filename);

    int
    read_record(fds_drec &drec);

private:
    unique_fds_iemgr m_iemgr;
    unique_fds_file m_file;
    fds_file_read_ctx m_read_ctx;
};
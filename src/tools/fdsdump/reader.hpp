#pragma once

#include "common.hpp"

class Reader {
public:
    Reader(fds_iemgr_t &m_iemgr);

    void
    set_file(std::string filename);

    bool
    read_record(fds_drec &drec);

    uint64_t
    total_number_of_records();

private:
    fds_iemgr_t &m_iemgr;

    std::string m_filename;

    unique_fds_file m_file;

    fds_file_read_ctx m_read_ctx;
};
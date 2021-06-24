#include "reader.hpp"
#include <string>

Reader::Reader(const char *filename)
{
    int rc;

    m_iemgr.reset(fds_iemgr_create());
    if (!m_iemgr) {
        throw std::bad_alloc();
    }

    rc = fds_iemgr_read_dir(m_iemgr.get(), fds_api_cfg_dir());
    if (rc != FDS_OK) {
        throw std::runtime_error("cannot read iemgr definitions");
    }

    m_file.reset(fds_file_init());
    if (!m_file) {
        throw std::bad_alloc();
    }

    rc = fds_file_open(m_file.get(), filename, FDS_FILE_READ);
    if (rc != FDS_OK) {
        throw std::runtime_error("cannot open file \"" + std::string(filename) + "\"");
    }

    rc = fds_file_set_iemgr(m_file.get(), m_iemgr.get());
    if (rc != FDS_OK) {
        throw std::runtime_error("cannot set file iemgr");
    }
}

int
Reader::read_record(fds_drec &drec)
{
    int rc;
    rc = fds_file_read_rec(m_file.get(), &drec, &m_read_ctx);
    return rc;
}
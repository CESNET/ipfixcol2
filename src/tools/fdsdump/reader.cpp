#include "reader.hpp"
#include <string>

Reader::Reader(const char *filename, fds_iemgr_t &iemgr)
    : m_iemgr(iemgr)
{
    int rc;

    m_file.reset(fds_file_init());
    if (!m_file) {
        throw std::bad_alloc();
    }

    rc = fds_file_open(m_file.get(), filename, FDS_FILE_READ);
    if (rc != FDS_OK) {
        throw std::runtime_error("cannot open file \"" + std::string(filename) + "\"");
    }

    rc = fds_file_set_iemgr(m_file.get(), &m_iemgr);
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

uint64_t
Reader::total_number_of_records()
{
    const fds_file_stats *stats = fds_file_stats_get(m_file.get());
    return stats->recs_total;
}
#include "reader.hpp"
#include <string>
#include <stdexcept>

Reader::Reader(fds_iemgr_t &iemgr)
    : m_iemgr(iemgr)
{
}

void
Reader::set_file(std::string filename)
{
    int rc;

    m_read_ctx = {};
    m_filename = filename;

    m_file.reset(fds_file_init());
    if (!m_file) {
        throw std::bad_alloc();
    }

    rc = fds_file_open(m_file.get(), m_filename.c_str(), FDS_FILE_READ);
    if (rc != FDS_OK) {
        throw std::runtime_error("cannot open file \"" + m_filename + "\"");
    }

    rc = fds_file_set_iemgr(m_file.get(), &m_iemgr);
    if (rc != FDS_OK) {
        throw std::runtime_error("cannot set file iemgr");
    }
}

bool
Reader::read_record(fds_drec &drec)
{
    if (!m_file) {
        return false;
    }

    int rc = fds_file_read_rec(m_file.get(), &drec, &m_read_ctx);

    if (rc == FDS_OK) {
        return true;

    } else if (rc == FDS_EOC) {
        return false;

    } else {
        throw std::runtime_error("error reading record from file " + m_filename);
    }
}

uint64_t
Reader::records_count()
{
    const fds_file_stats *stats = fds_file_stats_get(m_file.get());
    return stats->recs_total;
}
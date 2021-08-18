/**
 * \file src/ipfix/fdsreader.cpp
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * \brief FDS file reader wrapper
 *
 * Copyright (C) 2021 CESNET, z.s.p.o.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of the Company nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * ALTERNATIVELY, provided that this notice is retained in full, this
 * product may be distributed under the terms of the GNU General Public
 * License (GPL) version 2 or later, in which case the provisions
 * of the GPL apply INSTEAD OF those given above.
 *
 * This software is provided ``as is, and any express or implied
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose are disclaimed.
 * In no event shall the company or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even
 * if advised of the possibility of such damage.
 *
 */
#include "ipfix/fdsreader.hpp"

#include <string>
#include <stdexcept>

FDSReader::FDSReader(fds_iemgr_t *iemgr)
    : m_iemgr(iemgr)
{
}

void
FDSReader::set_file(std::string filename)
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

    rc = fds_file_set_iemgr(m_file.get(), m_iemgr);
    if (rc != FDS_OK) {
        throw std::runtime_error("cannot set file iemgr");
    }
}

bool
FDSReader::read_record(fds_drec &drec)
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
FDSReader::records_count()
{
    const fds_file_stats *stats = fds_file_stats_get(m_file.get());
    return stats->recs_total;
}

/**
 * \file src/ipfix/fdsreader.hpp
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
#pragma once

#include "ipfix/util.hpp"

/**
 * \brief      A light wrapper around fds_file to hold the necessary read state and provide RAII.
 */
class FDSReader {
public:

    /**
     * \brief      Constructs a new instance.
     *
     * \param      iemgr  The iemgr
     */
    FDSReader(fds_iemgr_t *iemgr);

    /**
     * \brief      Sets the current file.
     *
     * \param[in]  filename  The filename
     */
    void
    set_file(std::string filename);

    /**
     * \brief      Reads a data record.
     *
     * \param[out] drec  The data record to be read
     *
     * \return     true if a record was read, false if end of file was reached
     */
    bool
    read_record(fds_drec &drec);

    /**
     * \brief      Get the number of records in the current file.
     *
     * \return     The total number of records
     */
    uint64_t
    records_count();

private:
    fds_iemgr_t *m_iemgr;

    std::string m_filename;

    unique_fds_file m_file;

    fds_file_read_ctx m_read_ctx;
};

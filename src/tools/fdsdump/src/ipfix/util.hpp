/**
 * \file src/ipfix/util.hpp
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * \brief IPFIX utility functions
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

#include <memory>
#include <stdexcept>
#include <libfds.h>

struct fds_file_closer {
    void operator()(fds_file_t *file) const { fds_file_close(file); }
};

struct fds_iemgr_destroyer {
    void operator()(fds_iemgr_t *iemgr) const { fds_iemgr_destroy(iemgr); }
};

struct fds_filter_destroyer {
    void operator()(fds_filter_t *filter) const { fds_filter_destroy(filter); }
};

struct fds_filter_opts_destroyer {
    void operator()(fds_filter_opts_t *opts) const { fds_filter_destroy_opts(opts); }
};

struct fds_ipfix_filter_destroyer {
    void operator()(fds_ipfix_filter_t *filter) const { fds_ipfix_filter_destroy(filter); }
};

using unique_fds_file = std::unique_ptr<fds_file_t, fds_file_closer>;
using unique_fds_iemgr = std::unique_ptr<fds_iemgr_t, fds_iemgr_destroyer>;
using unique_fds_filter = std::unique_ptr<fds_filter_t, fds_filter_destroyer>;
using unique_fds_filter_opts = std::unique_ptr<fds_filter_opts_t, fds_filter_opts_destroyer>;
using unique_fds_ipfix_filter = std::unique_ptr<fds_ipfix_filter_t, fds_ipfix_filter_destroyer>;

/**
 * \brief      Get uint from a drec field
 *
 * \param      field  The field
 *
 * \return     The uint.
 */
uint64_t
get_uint(fds_drec_field &field);

/**
 * \brief      Get int from a drec field
 *
 * \param      field  The field
 *
 * \return     The int.
 */
int64_t
get_int(fds_drec_field &field);

/**
 * \brief      Get datetime from a drec field
 *
 * \param      field  The field
 *
 * \return     The datetime.
 */
uint64_t
get_datetime(fds_drec_field &field);

/**
 * \brief      Make an iemgr with RAII.
 *
 * \return     The iemgr
 */
unique_fds_iemgr
make_iemgr();

/**
 * \brief      Same as fds_drec_find, but accepting flags parameter.
 *
 * \param[in]  drec   The drec
 * \param[in]  pen    The pen
 * \param[in]  id     The identifier
 * \param[in]  flags  The flags
 * \param[out] field  The field
 *
 * \return     The return code.
 */
int
fds_drec_find(fds_drec *drec, uint32_t pen, uint16_t id, uint16_t flags, fds_drec_field *field);
/**
 * \file src/ipfix/util.cpp
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
#include "ipfix/util.hpp"

uint64_t
get_uint(fds_drec_field &field)
{
    uint64_t tmp;
    int rc = fds_get_uint_be(field.data, field.size, &tmp);
    assert(rc == FDS_OK);
    return tmp;
}

int64_t
get_int(fds_drec_field &field)
{
    int64_t tmp;
    int rc = fds_get_int_be(field.data, field.size, &tmp);
    assert(rc == FDS_OK);
    return tmp;
}

uint64_t
get_datetime(fds_drec_field &field)
{
    uint64_t tmp;
    int rc = fds_get_datetime_lp_be(field.data, field.size, field.info->def->data_type, &tmp);
    assert(rc == FDS_OK);
    return tmp;
}

unique_fds_iemgr
make_iemgr()
{
    int rc;

    unique_fds_iemgr iemgr;
    iemgr.reset(fds_iemgr_create());
    if (!iemgr) {
        throw std::bad_alloc();
    }

    rc = fds_iemgr_read_dir(iemgr.get(), fds_api_cfg_dir());
    if (rc != FDS_OK) {
        throw std::runtime_error("cannot read iemgr definitions");
    }

    return iemgr;
}

int
fds_drec_find(fds_drec *drec, uint32_t pen, uint16_t id, uint16_t flags, fds_drec_field *field)
{
    if (flags == 0) {
        return fds_drec_find(drec, pen, id, field);

    } else {
        fds_drec_iter iter;
        fds_drec_iter_init(&iter, drec, flags);

        int ret = fds_drec_iter_find(&iter, pen, id);
        if (ret != FDS_EOC) {
            *field = iter.field;
        }

        assert(ret == FDS_EOC || field->data != nullptr);

        return ret;
    }
}
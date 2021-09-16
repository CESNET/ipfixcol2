/**
 * \file src/plugins/output/forwarder/src/common.cpp
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * \brief Common use functions and structures
 * \date 2021
 */

/* Copyright (C) 2021 CESNET, z.s.p.o.
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
 * This software is provided ``as is'', and any express or implied
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

#include "common.h"
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <cassert>

#include <ipfixcol2.h>

void
tsnapshot_for_each(const fds_tsnapshot_t *tsnap, std::function<void(const fds_template *)> callback)
{
    struct CallbackData {
        std::function<void(const fds_template *)> callback;
        std::exception_ptr ex;
    };

    CallbackData cb_data;
    cb_data.callback = callback;
    cb_data.ex = nullptr;

    fds_tsnapshot_for(tsnap,
        [](const fds_template *tmplt, void *data) -> bool {
            CallbackData &cb_data = *(CallbackData *) data;

            try {
                cb_data.callback(tmplt);
                return true;

            } catch (...) {
                cb_data.ex = std::current_exception();
                return false;
            }

        }, &cb_data);

    if (cb_data.ex) {
        std::rethrow_exception(cb_data.ex);
    }
}

time_t
get_monotonic_time()
{
    timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC_COARSE, &ts) != 0) {
        throw errno_runtime_error(errno, "clock_gettime");
    }
    return ts.tv_sec;
}

std::runtime_error
errno_runtime_error(int errno_, const std::string &func_name)
{
    char *errbuf;
    ipx_strerror(errno_, errbuf);
    return std::runtime_error(func_name + "() failed: " + std::string(errbuf));
}

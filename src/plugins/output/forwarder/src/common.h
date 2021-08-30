/**
 * \file src/plugins/output/forwarder/src/common.h
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

#pragma once

#include <string>
#include <functional>
#include <cstdint>
#include <libfds.h>
#include <stdexcept>

enum class Protocol {
    UNASSIGNED,
    TCP,
    UDP
};

struct ConnectionParams {
    /// The IP address or hostname
    std::string address;
    /// The port
    uint16_t port;
    /// The transport protocol
    Protocol protocol;
};

/**
 * \brief A C++ wrapper for the fds_tsnapshot_for function that can handle exceptions
 * \param tsnap     The template snapshot
 * \param callback  The callback to be called for each of the templates in the snapshot
 * \throw Whatever the callback might throw
 */
void
tsnapshot_for_each(const fds_tsnapshot_t *tsnap, std::function<void(const fds_template *)> callback);

/**
 * \brief Get monotonic time to be used e.g. for calculating elapsed seconds
 * \return The monotonic time
 * \throw std::runtime_error on failure
 */
time_t
get_monotonic_time();

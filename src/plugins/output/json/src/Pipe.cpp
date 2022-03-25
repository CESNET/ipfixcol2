/**
 * \file
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Printer to standard output (source file)
 * \date 2022
 */

/* Copyright (C) 2018 CESNET, z.s.p.o.
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

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <iostream>

#include "Pipe.hpp"

Pipe::Pipe(const struct cfg_pipe &cfg, ipx_ctx_t *ctx)
    : Output(cfg.name, ctx), m_path(cfg.path), m_fd(-1)
{
    int open_flags = O_WRONLY;
    int ret;

    if (!cfg.blocking) {
        open_flags |= O_NONBLOCK;
    }

    ret = mkfifo(m_path.c_str(), cfg.permissions);
    if (ret < 0) {
        const char *err_str;
        ipx_strerror(errno, err_str);

        throw std::runtime_error("(Pipe output) mkfifo('" + m_path + "', "
            + std::to_string(cfg.permissions) + ") has failed: " + err_str);
    }

    m_fd = open(m_path.c_str(), open_flags);
    if (m_fd < 0) {
        const char *err_str;
        ipx_strerror(errno, err_str);

        unlink(m_path.c_str());

        throw std::runtime_error("(Pipe output) open('" + m_path + "') "
            + "has failed: " + err_str);
    }
}

Pipe::~Pipe()
{
    if (m_fd >= 0) {
        close(m_fd);
    }

    unlink(m_path.c_str());
}

int
Pipe::process(const char *str, size_t len)
{


    return IPX_OK;
}

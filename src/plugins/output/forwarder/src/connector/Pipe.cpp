/**
 * \file src/plugins/output/forwarder/src/connector/Pipe.cpp
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * \brief Pipe class
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
#include "connector/Pipe.h"

#include <unistd.h>
#include <fcntl.h>

Pipe::Pipe()
{
    int fd[2];
    if (pipe(fd) != 0) {
        throw errno_runtime_error(errno, "pipe");
    }

    m_readfd.reset(fd[0]);
    m_writefd.reset(fd[1]);

    int flags;

    if ((flags = fcntl(m_readfd.get(), F_GETFL)) == -1) {
        throw errno_runtime_error(errno, "fcntl");
    }
    if (fcntl(m_readfd.get(), F_SETFL, flags | O_NONBLOCK) == -1) {
        throw errno_runtime_error(errno, "fcntl");
    }

    if ((flags = fcntl(m_writefd.get(), F_GETFL)) == -1) {
        throw errno_runtime_error(errno, "fcntl");
    }
    if (fcntl(m_writefd.get(), F_SETFL, flags | O_NONBLOCK) == -1) {
        throw errno_runtime_error(errno, "fcntl");
    }
}

void
Pipe::poke(bool ignore_error)
{
    ssize_t ret = write(m_writefd.get(), "\x00", 1);
    if (ret < 0 && !ignore_error) {
        throw errno_runtime_error(errno, "write");
    }
}

void
Pipe::clear()
{
    char throwaway[16];
    while (read(m_readfd.get(), throwaway, 16) > 0) {}
}
/**
 * \file src/plugins/output/forwarder/src/connector/Pipe.h
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

#pragma once

#include "common.h"

/**
 * \brief  Simple utility class around pipe used for interrupting poll
 */
class Pipe {
public:
    /**
     * \brief  The constructor
     */
    Pipe();

    Pipe(const Pipe &) = delete;
    Pipe(Pipe &&) = delete;

    /**
     * \brief  Write to the pipe to trigger its write event
     * \param ignore_error  Do not throw on write error
     * \throw std::runtime_erorr on write error if ignore_error is false
     */
    void
    poke(bool ignore_error = false);

    /**
     * \brief  Read everything from the pipe and throw it away
     */
    void
    clear();

    /**
     * \brief  Get the read file descriptor
     */
    int readfd() const { return m_readfd.get(); }

private:
    UniqueFd m_readfd;
    UniqueFd m_writefd;
};

/**
 * \file src/utils/filelist.cpp
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * \brief Thread safe file list
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
#include "utils/filelist.hpp"
#include <glob.h>
#include <cassert>

void
FileList::add_files(const std::string &pattern)
{
    std::lock_guard<std::mutex> guard(m_mutex);

    glob_t globbuf = {};

    int ret = glob(pattern.c_str(), GLOB_MARK, NULL, &globbuf);

    if (ret == GLOB_NOSPACE) {
        throw std::bad_alloc();
    }

    if (ret == GLOB_ABORTED) {
        throw std::runtime_error("glob failed");
    }

    if (ret == GLOB_NOMATCH) {
        // This is probably fine?
        return;
    }

    assert(ret == 0);

    for (size_t i = 0; i < globbuf.gl_pathc; i++) {
        std::string filename = globbuf.gl_pathv[i];

        if (filename[filename.size() - 1] == '/') {
            // Skip directories
            continue;
        }

        m_files.push_back(std::move(filename));
    }
}

bool
FileList::pop(std::string &filename)
{
    std::lock_guard<std::mutex> guard(m_mutex);

    if (empty()) {
        return false;
    }

    filename = std::move(*m_files.begin());
    m_files.erase(m_files.begin());
    return true;
}

/**
 * \file src/utils/filelist.hpp
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
#pragma once

#include <string>
#include <mutex>
#include <vector>

/**
 * \brief      A file list that allows thread safe retrieval of items;
 */
class FileList {
public:
    using Iterator = std::vector<std::string>::const_iterator;

	/**
     * \brief      Add files matching the specified pattern onto the list.
     *
     * \param[in]  pattern  The file path glob pattern.
     */
    void
	add_files(const std::string &pattern);

    /**
     * \brief      A thread safe method to retrieve a filename off the list.
     *
     * \param[out] filename  The filename
     *
     * \return     true if the filename was retrieved, false if the list was empty
     */
    bool
    pop(std::string &filename);

    /**
     * \brief      Get the length of the list
     *
     * \return     The length of the list
     */
    size_t length() const { return m_files.size(); }

    /**
     * \brief      Check whether the list is empty
     *
     * \return     true or false
     */
    bool empty() const { return m_files.empty(); }

    /**
     * \brief      Get an iterator to the beginning of the list
     *
     * \return     The iterator
     */
    Iterator begin() const { return m_files.cbegin(); }

    /**
     * \brief      Get an iterator representing the end of the list
     *
     * \return     The iterator
     */
    Iterator end() const { return m_files.cend(); }

private:
    std::mutex m_mutex;
	std::vector<std::string> m_files;
};
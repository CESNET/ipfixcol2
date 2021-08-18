/**
 * \file src/view/aggregator.hpp
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * \brief Aggregator
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

#include <array>
#include <vector>
#include <cstdint>

#include <libfds.h>

#include "utils/hashtable.hpp"
#include "view/sort.hpp"


/**
 * \brief      A class performing aggregation of fds data records based on a view definition
 */
class Aggregator
{
public:
    /**
     * \brief      Constructs a new instance.
     *
     * \param[in]  view_def  The view definition
     */
    Aggregator(ViewDefinition view_def);

    /**
     * \brief      Process a data record
     *
     * \param      drec  The data record
     */
    void
    process_record(fds_drec &drec);

    /**
     * \brief      Merge other aggregator into this one
     *
     * \param      other  The other aggregator
     */
    void
    merge(Aggregator &other);

    /**
     * The underlying hash table
     *
     * \warning If modified from outside, behavior of further calls to process_record and merge are undefined!
     */
    HashTable m_table;

    /**
     * \brief      Get the aggregated records
     *
     * \return     Reference to the aggregated record
     *
     * \warning    If modified from outside, behavior of further calls to process_record and merge
     *             are undefined!
     */
    std::vector<uint8_t *> &items() { return m_table.items(); }

private:

    ViewDefinition m_view_def;

    std::vector<uint8_t> m_key_buffer;

    void
    aggregate(fds_drec &drec, Direction direction, uint16_t drec_find_flags);
};

std::vector<uint8_t *>
make_top_n(const ViewDefinition &def, std::vector<Aggregator *> &aggregators, size_t n, const std::vector<SortField> &sort_fields);

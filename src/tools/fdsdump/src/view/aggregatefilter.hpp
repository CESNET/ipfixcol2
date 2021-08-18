/**
 * \file src/view/aggregatefilter.hpp
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * \brief Aggregate filter
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

#include <vector>

#include "ipfix/util.hpp"
#include "view/aggregator.hpp"

/**
 * \brief      A specialization of fds_filter for filtering view aggregates.
 */
class AggregateFilter
{
public:

    /**
     * \brief      Default constructor, the filter has to be initialized with the other constructor before use.
     */
    AggregateFilter() {}

    /**
     * \brief      Constructs a new instance.
     *
     * \param[in]  filter_expr  The filter expression
     * \param[in]  view_def     The view definition
     */
    AggregateFilter(const char *filter_expr, ViewDefinition view_def);

    /**
     * \brief      Disallow move as the underlying fds_filter points to a state within the instance.
     */
    AggregateFilter(AggregateFilter &&) = delete;

    /**
     * \brief      Disallow copy as we're holding an instance of a fds_filter.
     */
    AggregateFilter(const AggregateFilter &) = delete;

    /**
     * \brief      Check if a record passes the filter
     *
     * \param      record  The record
     *
     * \return     true if it does, false otherwise
     */
    bool
    record_passes(uint8_t *record);

private:
    unique_fds_filter_opts m_filter_opts;
    unique_fds_filter m_filter;
    ViewDefinition m_view_def;

    struct Mapping {
        DataType data_type;
        unsigned int offset;
    };

    std::vector<Mapping> m_value_map;

    std::exception_ptr m_exception = nullptr;

    friend int
    lookup_callback(void *user_ctx, const char *name, const char *other_name,
                    int *out_id, int *out_datatype, int *out_flags) noexcept;

    friend int
    data_callback(void *user_ctx, bool reset_ctx, int id, void *data, fds_filter_value_u *out_value) noexcept;

    int
    lookup_callback(const char *name, const char *other_name, int *out_id, int *out_datatype, int *out_flags);

    int
    data_callback(bool reset_ctx, int id, void *data, fds_filter_value_u *out_value) noexcept;
};
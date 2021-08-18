/**
 * \file src/view/aggregatefilter.cpp
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
#include "view/aggregatefilter.hpp"
#include <stdexcept>

int
lookup_callback(void *user_ctx, const char *name, const char *other_name,
                int *out_id, int *out_datatype, int *out_flags) noexcept
{
    AggregateFilter *aggregate_filter = (AggregateFilter *) user_ctx;

    try {
        int ret = aggregate_filter->lookup_callback(name, other_name, out_id, out_datatype, out_flags);
        return ret;

    } catch (...) {
        aggregate_filter->m_exception = std::current_exception();
        return FDS_ERR_DENIED;

    }
}

int
data_callback(void *user_ctx, bool reset_ctx, int id, void *data, fds_filter_value_u *out_value) noexcept
{
    AggregateFilter *aggregate_filter = (AggregateFilter *) user_ctx;

    int ret = aggregate_filter->data_callback(reset_ctx, id, data, out_value);
    return ret;
}

AggregateFilter::AggregateFilter(const char *filter_expr, ViewDefinition view_def)
    : m_view_def(view_def)
{
    m_filter_opts.reset(fds_filter_create_default_opts());
    if (!m_filter_opts) {
        throw std::bad_alloc();
    }

    fds_filter_opts_set_user_ctx(m_filter_opts.get(), this);
    fds_filter_opts_set_lookup_cb(m_filter_opts.get(), &::lookup_callback);
    fds_filter_opts_set_data_cb(m_filter_opts.get(), &::data_callback);

    fds_filter_t *filter;
    int rc = fds_filter_create(&filter, filter_expr, m_filter_opts.get());
    m_filter.reset(filter);

    if (m_exception) {
        std::rethrow_exception(m_exception);
    }

    if (rc != FDS_OK) {
        std::string error = fds_filter_get_error(filter)->msg;
        throw std::runtime_error(error);
    }
}

bool
AggregateFilter::record_passes(uint8_t *record)
{
    return fds_filter_eval(m_filter.get(), record);
}

int
AggregateFilter::lookup_callback(const char *name, const char *other_name, int *out_id, int *out_datatype, int *out_flags)
{
    *out_id = m_value_map.size();

    Mapping mapping;
    mapping.offset = m_view_def.keys_size;

    bool found = false;
    for (const auto &field : m_view_def.value_fields) {
        if (field.name == name) {
            found = true;
            mapping.data_type = field.data_type;

            switch (mapping.data_type) {
            case DataType::Signed64:
                *out_datatype = FDS_FDT_INT;
                break;
            case DataType::Unsigned64:
                *out_datatype = FDS_FDT_UINT;
                break;
            default:
                return FDS_ERR_NOTFOUND;
            }

            break;
        }

        mapping.offset += field.size;
    }

    if (!found) {
        return FDS_ERR_NOTFOUND;
    }

    m_value_map.push_back(mapping);

    return FDS_OK;
}

int
AggregateFilter::data_callback(bool reset_ctx, int id, void *data, fds_filter_value_u *out_value) noexcept
{
    assert(id >= 0 && id < m_value_map.size());

    const Mapping &mapping = m_value_map[id];
    const ViewValue *value = (const ViewValue *) ((uint8_t *) data + mapping.offset);

    switch (mapping.data_type) {
    case DataType::Signed64:
        out_value->i = value->i64;
        break;
    case DataType::Unsigned64:
        out_value->u = value->u64;
        break;
    default: assert(0);
    }

    return FDS_OK;
}

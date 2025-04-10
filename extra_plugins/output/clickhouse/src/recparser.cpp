/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Record parser
 * @date 2025
 *
 * Copyright(c) 2025 CESNET z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "recparser.h"

#include <cassert>

static int index_of_elem(const fds_iemgr_elem &elem, const fds_template &tmplt, bool rev)
{
    if (rev && !(tmplt.flags & FDS_TEMPLATE_BIFLOW)) {
        return -1;
    }

    const fds_tfield *fields = !rev ? tmplt.fields : tmplt.fields_rev;
    for (std::size_t i = 0; i < tmplt.fields_cnt_total; i++) {
        if (fields[i].en == elem.scope->pen && fields[i].id == elem.id) {
            return i;
        }
    }
    return -1;
}

static int index_of_alias(const fds_iemgr_alias &alias, const fds_template &tmplt, bool rev)
{
    if (rev && !(tmplt.flags & FDS_TEMPLATE_BIFLOW)) {
        return -1;
    }

    for (std::size_t i = 0; i < alias.sources_cnt; i++) {
        int idx = index_of_elem(*alias.sources[i], tmplt, rev);
        if (idx != -1) {
            return idx;
        }
    }
    return -1;
}

static bool is_skip(fds_drec_field &field, bool rev)
{
    static constexpr uint32_t IANA_EN = 0;
    static constexpr uint32_t IANA_EN_REVERSE = 29305;
    static constexpr uint16_t IANA_OCTET_DELTA_COUNT_ID = 1;
    static constexpr uint16_t IANA_PACKET_DELTA_COUNT_ID = 2;

    uint32_t en = !rev ? IANA_EN : IANA_EN_REVERSE;
    if (field.info->en == en &&
            (field.info->id == IANA_OCTET_DELTA_COUNT_ID || field.info->id == IANA_PACKET_DELTA_COUNT_ID)
    ) {
        uint64_t value;
        if (fds_get_uint_be(field.data, field.size, &value) == FDS_OK && value == 0) {
            return true;
        }
    }
    return false;
}

RecParser::RecParser(const std::vector<Column> &columns, const fds_template *tmplt, bool biflow_autoignore)
{
    m_tmplt.reset(fds_template_copy(tmplt));
    if (!m_tmplt) {
        throw std::bad_alloc{};
    }

    m_biflow = tmplt->flags & FDS_TEMPLATE_BIFLOW;
    m_biflow_autoignore = biflow_autoignore;

    m_fields.resize(columns.size(), fds_drec_field{nullptr, 0, nullptr});
    m_fields_rev.resize(columns.size(), fds_drec_field{nullptr, 0, nullptr});
    m_mapping.resize(tmplt->fields_cnt_total, -1);
    m_mapping_rev.resize(tmplt->fields_cnt_total, -1);

    std::size_t column_idx = 0;

    for (const Column &column : columns) {
        int field_idx = -1;
        int rev_field_idx = -1;

        if (column.elem) {
            field_idx = index_of_elem(*column.elem, *tmplt, false);
            rev_field_idx = index_of_elem(*column.elem, *tmplt, true);
        } else if (column.alias) {
            field_idx = index_of_alias(*column.alias, *tmplt, false);
            rev_field_idx = index_of_alias(*column.alias, *tmplt, true);
        }

        if (field_idx != -1) {
            assert(!column.elem || (tmplt->fields[field_idx].en == column.elem->scope->pen &&
                                    tmplt->fields[field_idx].id == column.elem->id));
            if (m_mapping[field_idx] == -1) {
                m_mapping[field_idx] = column_idx;
            }
        }

        if (rev_field_idx != -1) {
            assert(!column.elem || (tmplt->fields_rev[rev_field_idx].en == column.elem->scope->pen &&
                                    tmplt->fields_rev[rev_field_idx].id == column.elem->id));
            if (m_mapping_rev[rev_field_idx] == -1) {
                m_mapping_rev[rev_field_idx] = column_idx;
            }
        }

        column_idx++;
    }
}

const fds_template *RecParser::tmplt() const
{
    return m_tmplt.get();
}

void RecParser::parse_record(fds_drec &rec)
{
    m_skip_flag_fwd = false;
    m_skip_flag_rev = false;

    if (!m_biflow) {
        m_skip_flag_rev = true;
    }

    for (fds_drec_field &field : m_fields) {
        field = fds_drec_field{nullptr, 0, nullptr};
    }
    for (fds_drec_field &field : m_fields_rev) {
        field = fds_drec_field{nullptr, 0, nullptr};
    }

    fds_drec_iter iter;
    fds_drec_iter_init(&iter, &rec, 0);
    int i;
    while ((i = fds_drec_iter_next(&iter)) != FDS_EOC) {
        if (m_biflow && m_biflow_autoignore) {
            m_skip_flag_fwd |= is_skip(iter.field, false);
            m_skip_flag_rev |= is_skip(iter.field, true);
        }

        int field_idx = m_mapping[i];
        if (field_idx != -1) {
            m_fields[field_idx] = iter.field;
        }

        int rev_field_idx = m_mapping_rev[i];
        if (rev_field_idx != -1) {
            m_fields_rev[rev_field_idx] = iter.field;
        }
    }
}

fds_drec_field &RecParser::get_column(std::size_t idx, bool rev)
{
    assert((!rev && idx < m_fields.size()) || (rev && idx < m_fields_rev.size()));
    assert(!rev || m_biflow);
    return rev ? m_fields_rev[idx] : m_fields[idx];
}

bool RecParser::skip_fwd() const
{
    return m_skip_flag_fwd;
}

bool RecParser::skip_rev() const
{
    return m_skip_flag_rev;
}

RecParserManager::RecParserManager(const std::vector<Column> &columns, bool biflow_autoignore)
    : m_columns(columns)
    , m_biflow_autoignore(biflow_autoignore)
{}

void RecParserManager::select_session(const ipx_session *sess)
{
    m_active_session = &m_sessions[sess];
}

void RecParserManager::select_odid(uint32_t odid)
{
    assert(m_active_session != nullptr);
    m_active_odid = &(*m_active_session)[odid];
}

void RecParserManager::delete_session(const ipx_session *sess)
{
    m_sessions.erase(sess);
    m_active_session = nullptr;
    m_active_odid = nullptr;
}

RecParser &RecParserManager::get_parser(const fds_template *tmplt)
{
    assert(m_active_session != nullptr && m_active_odid != nullptr);
    auto it = m_active_odid->find(tmplt->id);
    if (it == m_active_odid->end() || fds_template_cmp(it->second.tmplt(), tmplt) != 0) {
        auto [it, _] = m_active_odid->insert_or_assign(tmplt->id, RecParser(m_columns, tmplt, m_biflow_autoignore));
        return it->second;
    }
    return it->second;
}

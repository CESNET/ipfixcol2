/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief IPFIX field abstraction
 *
 * Copyright: (C) 2024 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <cassert>
#include <stdexcept>

#include <common/field.hpp>
#include <common/ieMgr.hpp>

namespace fdsdump {

Field::Field(std::string name)
    : m_name(name)
{
    string_trim(m_name);

    auto *iemgr = IEMgr::instance().ptr();

    m_alias = fds_iemgr_alias_find(iemgr, m_name.c_str());
    if (m_alias) {
        // Success
        m_type = get_type_of_alias(m_alias);
        return;
    }

    m_elem = fds_iemgr_elem_find_name(iemgr, m_name.c_str());
    if (m_elem) {
        // Success
        m_type = get_type_of_element(m_elem);
        return;
    }

    throw std::invalid_argument("unknown field '" + name + "'");
}

FieldType
Field::get_type_of_element(const struct fds_iemgr_elem *elem) const
{
    switch (elem->data_type) {
    case FDS_ET_OCTET_ARRAY:
        return FieldType::bytes;

    case FDS_ET_UNSIGNED_8:
    case FDS_ET_UNSIGNED_16:
    case FDS_ET_UNSIGNED_32:
    case FDS_ET_UNSIGNED_64:
        return FieldType::num_unsigned;

    case FDS_ET_SIGNED_8:
    case FDS_ET_SIGNED_16:
    case FDS_ET_SIGNED_32:
    case FDS_ET_SIGNED_64:
        return FieldType::num_signed;

    case FDS_ET_FLOAT_32:
    case FDS_ET_FLOAT_64:
        return FieldType::num_float;

    case FDS_ET_BOOLEAN:
        return FieldType::boolean;

    case FDS_ET_MAC_ADDRESS:
        return FieldType::macaddr;

    case FDS_ET_STRING:
        return FieldType::string;

    case FDS_ET_DATE_TIME_SECONDS:
    case FDS_ET_DATE_TIME_MILLISECONDS:
    case FDS_ET_DATE_TIME_MICROSECONDS:
    case FDS_ET_DATE_TIME_NANOSECONDS:
        return FieldType::datetime;

    case FDS_ET_IPV4_ADDRESS:
    case FDS_ET_IPV6_ADDRESS:
        return FieldType::ipaddr;

    case FDS_ET_BASIC_LIST:
    case FDS_ET_SUB_TEMPLATE_LIST:
    case FDS_ET_SUB_TEMPLATE_MULTILIST:
        return FieldType::list;

    default:
        return FieldType::none;
    }
}

FieldType
Field::get_type_of_alias(const struct fds_iemgr_alias *alias) const
{
    FieldType result;

    if (alias->sources_cnt == 0) {
        return FieldType::none;
    }

    result = get_type_of_element(alias->sources[0]);

    for (size_t i = 1; i < alias->sources_cnt; ++i) {
        FieldType type = get_type_of_element(alias->sources[i]);

        if (type != result) {
            // Unable to determine common type
            return FieldType::none;
        }
    }

    return result;
}

unsigned int
Field::for_each(
    struct fds_drec *rec,
    std::function<void(fds_drec_field &)> cb,
    bool reverse)
{
    if (m_alias) {
        return for_each_alias(rec, cb, reverse);
    } else if (m_elem) {
        return for_each_element(rec, cb, reverse);
    } else {
        return 0;
    }
}

unsigned int
Field::for_each_alias(
    struct fds_drec *rec,
    std::function<void(struct fds_drec_field &)> cb,
    bool reverse)
{
    const uint16_t flags = reverse ? FDS_DREC_BIFLOW_REV : 0;
    struct fds_drec_iter iter;
    unsigned int count = 0;

    // Currently known modes
    assert(m_alias->mode == FDS_ALIAS_ANY_OF || m_alias->mode == FDS_ALIAS_FIRST_OF);

    fds_drec_iter_init(&iter, rec, flags);

    for (size_t i = 0; i < m_alias->sources_cnt; ++i) {
        const struct fds_iemgr_elem *elem = m_alias->sources[i];
        const uint32_t pen = elem->scope->pen;
        const uint16_t id = elem->id;

        while (fds_drec_iter_find(&iter, pen, id) != FDS_EOC) {
            cb(iter.field);
            count++;
        }

        if (count > 0 && m_alias->mode == FDS_ALIAS_FIRST_OF) {
            return count;
        }

        fds_drec_iter_rewind(&iter);
    }

    return count;
}

unsigned int
Field::for_each_element(
    struct fds_drec *rec,
    std::function<void(struct fds_drec_field &)> cb,
    bool reverse)
{
    const uint16_t flags = reverse ? FDS_DREC_BIFLOW_REV : 0;
    const uint32_t pen = m_elem->scope->pen;
    const uint16_t id = m_elem->id;
    struct fds_drec_iter iter;
    unsigned int count = 0;

    fds_drec_iter_init(&iter, rec, flags);

    while (fds_drec_iter_find(&iter, pen, id) != FDS_EOC) {
        cb(iter.field);
        count++;
    }

    return count;
}

} // fdsdump

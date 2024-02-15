/**
 * @file
 * @author Lukas Hutak <hutak@cesnet.cz>
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Field view
 *
 * Copyright: (C) 2024 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdexcept>

#include <common/fieldView.hpp>

#include <libfds/converters.h>

namespace fdsdump {

uint64_t
FieldView::as_uint() const
{
    uint64_t result;
    int ret;

    ret = fds_get_uint_be(m_field.data, m_field.size, &result);
    if (ret != FDS_OK) {
        throw std::invalid_argument("Field value conversion error (unsinged)");
    }

    return result;
}

int64_t
FieldView::as_int() const
{
    int64_t result;
    int ret;

    ret = fds_get_int_be(m_field.data, m_field.size, &result);
    if (ret != FDS_OK) {
        throw std::invalid_argument("Conversion error (signed)");
    }

    return result;
}

double
FieldView::as_float() const
{
    double result;
    int ret;

    ret = fds_get_float_be(m_field.data, m_field.size, &result);
    if (ret != FDS_OK) {
        throw std::invalid_argument("Conversion error (float)");
    }

    return result;
}

bool
FieldView::as_bool() const
{
    bool result;
    int ret;

    ret = fds_get_bool(m_field.data, m_field.size, &result);
    if (ret != FDS_OK) {
        throw std::invalid_argument("Conversion error (boolean)");
    }

    return result;
}

struct timespec
FieldView::as_datetime() const
{
    const struct fds_iemgr_elem *elem = m_field.info->def;
    struct timespec result;
    int ret;

    if (!elem) {
        throw std::invalid_argument("Conversion error (undefined datetime type)");
    }

    ret = fds_get_datetime_hp_be(m_field.data, m_field.size, elem->data_type, &result);
    if (ret != FDS_OK) {
        throw std::invalid_argument("Conversion error (datetime)");
    }

    return result;
}

uint64_t
FieldView::as_datetime_ms() const
{
    const struct fds_iemgr_elem *elem = m_field.info->def;
    uint64_t result;
    int ret;

    if (!elem) {
        throw std::invalid_argument("Conversion error (undefined datetime type)");
    }

    ret = fds_get_datetime_lp_be(m_field.data, m_field.size, elem->data_type, &result);
    if (ret != FDS_OK) {
        throw std::invalid_argument("Conversion error (datetime)");
    }

    return result;
}

IPAddr
FieldView::as_ipaddr() const
{
    if (m_field.size == 4U) {
        return IPAddr::ip4(m_field.data);
    }

    if (m_field.size == 16U) {
        return IPAddr::ip6(m_field.data);
    }

    throw std::invalid_argument("Conversion error (ipaddr)");
}

std::string
FieldView::as_string() const
{
    std::string value;

    if (m_field.size > 0) {
        // &value[0] doesn't work for zero-sized strings, so we have to handle this specially
        value.resize(m_field.size);
        int ret = fds_get_string(m_field.data, m_field.size, &value[0]);
        if (ret != FDS_OK) {
            throw std::invalid_argument("Conversion error (string)");
        }
    }

    return value;
}

std::vector<uint8_t>
FieldView::as_bytes() const
{
    std::vector<uint8_t> value;

    value.resize(m_field.size);
    int ret = fds_get_octet_array(m_field.data, m_field.size, value.data());
    if (ret != FDS_OK) {
        throw std::invalid_argument("Conversion error (bytes)");
    }

    return value;
}

} // fdsdump

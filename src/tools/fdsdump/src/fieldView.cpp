

#include <stdexcept>

#include "fieldView.hpp"

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

IPAddr
FieldView::as_ipaddr() const
{
    if (m_field.size == 4U) {
        const uint32_t *value = reinterpret_cast<uint32_t *>(m_field.data);
        return IPAddr(*value);
    }

    if (m_field.size == 16U) {
        return IPAddr(m_field.data);
    }

    throw std::invalid_argument("Conversion error (ipaddr)");
}

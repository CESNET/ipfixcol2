/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Aggregate view
 *
 * Copyright: (C) 2024 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <aggregator/value.hpp>
#include <aggregator/view.hpp>

namespace fdsdump {
namespace aggregator {

View::IterSpan
View::iter_keys(uint8_t *ptr) const
{
    return IterSpan(m_fields.begin(), m_fields.begin() + m_key_count, ptr);
}

View::IterSpanPairs
View::iter_keys(uint8_t *ptr1, uint8_t *ptr2) const
{
    return IterSpanPairs(m_fields.begin(), m_fields.begin() + m_key_count, ptr1, ptr2);
}

View::IterSpan
View::iter_values(uint8_t *ptr) const
{
    return IterSpan(m_fields.begin() + m_key_count, m_fields.end(), ptr);
}

View::IterSpanPairs
View::iter_values(uint8_t *ptr1, uint8_t *ptr2) const
{
    return IterSpanPairs(m_fields.begin() + m_key_count, m_fields.end(), ptr1, ptr2);
}

View::IterSpan
View::iter_fields(uint8_t *ptr) const
{
    return IterSpan(m_fields.begin(), m_fields.end(), ptr);
}

View::IterSpanPairs
View::iter_fields(uint8_t *ptr1, uint8_t *ptr2) const
{
    return IterSpanPairs(m_fields.begin(), m_fields.end(), ptr1, ptr2);
}

const Field *
View::find_field(const std::string &name)
{
    for (const auto &field : fields()) {
        if (field->name() == name) {
            return field.get();
        }
    }
    return nullptr;
}

Value &
View::access_field(const Field &field, uint8_t *record_ptr) const
{
    return *reinterpret_cast<Value *>(record_ptr + field.offset());
}

const Value &
View::access_field(const Field &field, const uint8_t *record_ptr) const
{
    return *reinterpret_cast<const Value *>(record_ptr + field.offset());
}

bool
View::ordered_before(uint8_t *key1, uint8_t *key2) const
{
    assert(!m_order_fields.empty());
    for (const auto &item : m_order_fields) {
        Value &value1 = *reinterpret_cast<Value *>(key1 + item.field->offset());
        Value &value2 = *reinterpret_cast<Value *>(key2 + item.field->offset());
        CmpResult res = item.field->compare(value1, value2);
        if (res != CmpResult::Eq) {
            return res == (
                item.dir == OrderDirection::Ascending
                    ? CmpResult::Lt : CmpResult::Gt);
        }
    }
    return false;
}

} // aggregator
} // fdsdump

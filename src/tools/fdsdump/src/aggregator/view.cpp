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
    if (!is_fixed_size()) {
        ptr += sizeof(uint32_t);
    }
    return IterSpan(m_fields.begin(), m_fields.begin() + m_key_count, ptr);
}

View::IterSpanPairs
View::iter_keys(uint8_t *ptr1, uint8_t *ptr2) const
{
    if (!is_fixed_size()) {
        ptr1 += sizeof(uint32_t);
        ptr2 += sizeof(uint32_t);
    }
    return IterSpanPairs(m_fields.begin(), m_fields.begin() + m_key_count, ptr1, ptr2);
}

View::IterSpan
View::iter_values(uint8_t *ptr) const
{
    ptr += key_size(ptr);
    return IterSpan(m_fields.begin() + m_key_count, m_fields.end(), ptr);
}

View::IterSpanPairs
View::iter_values(uint8_t *ptr1, uint8_t *ptr2) const
{
    ptr1 += key_size(ptr1);
    ptr2 += key_size(ptr2);
    return IterSpanPairs(m_fields.begin() + m_key_count, m_fields.end(), ptr1, ptr2);
}

View::IterSpan
View::iter_fields(uint8_t *ptr) const
{
    if (!is_fixed_size()) {
        ptr += sizeof(uint32_t);
    }
    return IterSpan(m_fields.begin(), m_fields.end(), ptr);
}

View::IterSpanPairs
View::iter_fields(uint8_t *ptr1, uint8_t *ptr2) const
{
    if (!is_fixed_size()) {
        ptr1 += sizeof(uint32_t);
        ptr2 += sizeof(uint32_t);
    }
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


void
View::set_output_limit(size_t n)
{
    m_output_limit = n;
}

Value &
View::access_field(const Field &field, uint8_t *record_ptr) const
{
    if (!is_fixed_size()) {
        for (const auto &item : iter_fields(record_ptr)) {
            if (&item.field == &field) {
                return item.value;
            }
        }

        assert(0 && "provided field is not part of this view");

    } else {
        return *reinterpret_cast<Value *>(record_ptr + field.offset());
    }
}

const Value &
View::access_field(const Field &field, const uint8_t *record_ptr) const
{
    if (!is_fixed_size()) {
        for (const auto &item : iter_fields(const_cast<uint8_t *>(record_ptr))) {
            if (&item.field == &field) {
                return item.value;
            }
        }

        assert(0 && "provided field is not part of this view");

    } else {
        return *reinterpret_cast<const Value *>(record_ptr + field.offset());
    }
}

bool
View::ordered_before(uint8_t *key1, uint8_t *key2) const
{
    assert(!m_order_fields.empty());
    for (const auto &item : m_order_fields) {
        Value &value1 = access_field(*item.field, key1);
        Value &value2 = access_field(*item.field, key2);
        CmpResult res = item.field->compare(value1, value2);
        if (res != CmpResult::Eq) {
            return res == (
                item.dir == OrderDirection::Ascending
                    ? CmpResult::Lt : CmpResult::Gt);
        }
    }
    return false;
}

CmpResult
View::compare(const uint8_t *rec1, const uint8_t *rec2) const
{
    assert(!m_order_fields.empty());
    for (const auto &item : m_order_fields) {
        const Value &value1 = access_field(*item.field, rec1);
        const Value &value2 = access_field(*item.field, rec2);
        CmpResult res = item.field->compare(value1, value2);
        if (res != CmpResult::Eq) {
            if (item.dir == OrderDirection::Ascending) {
                return res;
            } else {
                return res == CmpResult::Lt ? CmpResult::Gt : CmpResult::Lt;
            }
        }
    }
    return CmpResult::Eq;
}

} // aggregator
} // fdsdump

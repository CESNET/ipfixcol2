/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Aggregate view
 *
 * Copyright: (C) 2024 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#define XXH_INLINE_ALL

#include <3rd_party/xxhash/xxhash.h>
#include <aggregator/field.hpp>
#include <common/common.hpp>

#include <cstddef>
#include <functional>
#include <memory>
#include <vector>

namespace fdsdump {
namespace aggregator {

/**
 * @brief A view object for accessing fields and their values in an aggregation record
 */
class View {
    friend class ViewFactory;

public:
    /**
     * @brief The ordering/sorting direction
     */
    using KeyHashFnType = std::function<uint64_t(const uint8_t *)>;
    using KeyEqualsFnType = std::function<bool(const uint8_t *, const uint8_t *)>;
    using RecOrdFnType = std::function<bool(const uint8_t *, const uint8_t *)>;

    enum class OrderDirection {
        Ascending,
        Descending
    };

    /**
     * @brief Definition of a field to order/sort by
     */
    struct OrderField {
        const Field *field;
        OrderDirection dir;
    };

    /**
     * @brief A value that the iterator uses to provide individual iteration items
     */
    struct IteratorValue {
        const Field &field;
        Value &value;
    };

    /**
     * @brief An iterator over aggregation record fields
     */
    class Iterator {
    public:
        Iterator(
            std::vector<std::unique_ptr<Field>>::const_iterator begin,
            std::vector<std::unique_ptr<Field>>::const_iterator end,
            uint8_t *ptr
        ) :
            m_iter(begin),
            m_end(end),
            m_ptr(ptr)
        {
        }

        void
        operator++()
        {
            if (m_iter != m_end) {
                m_ptr += (*m_iter)->size(reinterpret_cast<const Value *>(m_ptr));
                ++m_iter;
            }
        }

        IteratorValue
        operator*()
        {
            return {*m_iter->get(), *reinterpret_cast<Value *>(m_ptr)};
        }

        bool
        operator==(const Iterator &other) const
        {
            return m_iter == other.m_iter;
        }

        bool
        operator!=(const Iterator &other) const
        {
            return !(*this == other);
        }

    private:
        std::vector<std::unique_ptr<Field>>::const_iterator m_iter;
        std::vector<std::unique_ptr<Field>>::const_iterator m_end;
        uint8_t *m_ptr;
    };

    /**
     * @brief An object providing iteration over a span of aggregation record fields
     */
    class IterSpan {
    public:
        Iterator
        begin() const
        {
            return Iterator(m_begin, m_end, m_ptr);
        }

        Iterator
        end() const
        {
            return Iterator(m_end, m_end, m_ptr);
        }

    private:
        friend class View;

        std::vector<std::unique_ptr<Field>>::const_iterator m_begin;
        std::vector<std::unique_ptr<Field>>::const_iterator m_end;
        uint8_t *m_ptr;

        IterSpan(
            std::vector<std::unique_ptr<Field>>::const_iterator begin,
            std::vector<std::unique_ptr<Field>>::const_iterator end,
            uint8_t *ptr
        ) :
            m_begin(begin),
            m_end(end),
            m_ptr(ptr)
        {
        }
    };

    /**
     * @brief Same as `View::IteratorValue` but for iterating two records at the same time
     */
    struct IteratorValuePairs {
        const Field &field;
        Value &value1;
        Value &value2;
    };

    /**
     * @brief Same as `View::Iterator` but for iterating two records at the same time
     */
    class IteratorPairs {
    public:
        IteratorPairs(
            std::vector<std::unique_ptr<Field>>::const_iterator begin,
            std::vector<std::unique_ptr<Field>>::const_iterator end,
            uint8_t *ptr1,
            uint8_t *ptr2
        ) :
            m_iter(begin),
            m_end(end),
            m_ptr1(ptr1),
            m_ptr2(ptr2)
        {
        }

        void
        operator++()
        {
            if (m_iter != m_end) {
                m_ptr1 += (*m_iter)->size(reinterpret_cast<const Value *>(m_ptr1));
                m_ptr2 += (*m_iter)->size(reinterpret_cast<const Value *>(m_ptr2));
                ++m_iter;
            }
        }

        IteratorValuePairs
        operator*()
        {
            return {
                *m_iter->get(),
                *reinterpret_cast<Value *>(m_ptr1),
                *reinterpret_cast<Value *>(m_ptr2)
            };
        }

        bool
        operator==(const IteratorPairs &other) const
        {
            return m_iter == other.m_iter;
        }

        bool
        operator!=(const IteratorPairs &other) const
        {
            return !(*this == other);
        }

    private:
        std::vector<std::unique_ptr<Field>>::const_iterator m_iter;
        std::vector<std::unique_ptr<Field>>::const_iterator m_end;
        uint8_t *m_ptr1;
        uint8_t *m_ptr2;
    };

    /**
     * @brief Same as `View::IterSpan` but for iterating two records at the same time
     */
    class IterSpanPairs {
    public:
        IteratorPairs
        begin() const
        {
            return IteratorPairs(m_begin, m_end, m_ptr1, m_ptr2);
        }

        IteratorPairs
        end() const
        {
            return IteratorPairs(m_end, m_end, m_ptr1, m_ptr2);
        }

    private:
        friend class View;

        std::vector<std::unique_ptr<Field>>::const_iterator m_begin;
        std::vector<std::unique_ptr<Field>>::const_iterator m_end;
        uint8_t *m_ptr1;
        uint8_t *m_ptr2;

        IterSpanPairs(
            std::vector<std::unique_ptr<Field>>::const_iterator begin,
            std::vector<std::unique_ptr<Field>>::const_iterator end,
            uint8_t *ptr1,
            uint8_t *ptr2
        ) :
            m_begin(begin),
            m_end(end),
            m_ptr1(ptr1),
            m_ptr2(ptr2)
        {
        }
    };

    /**
     * @brief Iterate over the key fields
     *
     * @param ptr  Pointer to the beginning of the aggregation record
     *
     * @returns A object to iterate over
     */
    IterSpan
    iter_keys(uint8_t *ptr) const;

    /**
     * @brief Iterate over the key fields in pairs
     *
     * @param ptr1  Pointer to the beginning of the aggregation record
     * @param ptr2  Pointer to the beginning of the other aggregation record
     *
     * @returns A object to iterate over
     */
    IterSpanPairs
    iter_keys(uint8_t *ptr1, uint8_t *ptr2) const;

    /**
     * @brief Iterate over the value fields
     *
     * @param ptr  Pointer to the beginning of the aggregation record
     *
     * @returns A object to iterate over
     */
    IterSpan
    iter_values(uint8_t *ptr) const;

    /**
     * @brief Iterate over the value fields in pairs
     *
     * @param ptr1  Pointer to the beginning of the aggregation record
     * @param ptr2  Pointer to the beginning of the other aggregation record
     *
     * @returns A object to iterate over
     */
    IterSpanPairs
    iter_values(uint8_t *ptr1, uint8_t *ptr2) const;

    /**
     * @brief Iterate over all the fields
     *
     * @param ptr  Pointer to the beginning of the aggregation record
     *
     * @returns A object to iterate over
     */
    IterSpan
    iter_fields(uint8_t *ptr) const;

    /**
     * @brief Iterate over all the fields in pairs
     *
     * @param ptr1  Pointer to the beginning of the aggregation record
     * @param ptr2  Pointer to the beginning of the other aggregation record
     *
     * @returns A object to iterate over
     */
    IterSpanPairs
    iter_fields(uint8_t *ptr1, uint8_t *ptr2) const;

    /**
     * @brief Get the number of bytes the key portion of the aggregation record occupies
     *        including the uint32_t size at the beginning of the record if the view is
     *        not fixed size
     *
     * @param ptr  Pointer to the beginning of the record
     *
     * @return The number of bytes
     */
    size_t
    key_size(const uint8_t *ptr) const
    {
        if (m_is_fixed_size) {
            return m_key_size;
        } else {
            return *reinterpret_cast<const uint32_t *>(ptr);
        }
    }

    /**
     * @brief Get the number of bytes the value portion of the aggregation record occupies
     *
     * @param ptr  Pointer to the beginning of the record
     *
     * @return The number of bytes
     */
    size_t value_size() const { return m_value_size; }

    /**
     * @brief Get the field definitions the view consists of
     */
    size_t record_size(const uint8_t *ptr) const { return key_size(ptr) + value_size(); }

    const std::vector<std::unique_ptr<Field>> &fields() const { return m_fields; }

    /**
     * @brief Find a field definition given its name
     *
     * @param name  The name of the field
     *
     * @return Pointer to the field definition if found, else nullptr
     */
    const Field *
    find_field(const std::string &name);


    /**
     * @brief Check whether one record is supposed to be ordered before another
     *
     * @param key1  Pointer to the key of the first aggregation record
     * @param key2  Pointer to the key of the second aggregation record
     *
     * @return true if key1 comes before key2 in an ordered sequence, else false
     */
    void
    set_output_limit(size_t n);

    size_t output_limit() const { return m_output_limit; }

    bool
    ordered_before(uint8_t *key1, uint8_t *key2) const;

    /**
     * @brief Get the definition of the ordering
     */
    const std::vector<OrderField> &order_fields() const { return m_order_fields; }

    /**
     * @brief Check whether the view contains "in/out" fields
     */
    bool has_inout_fields() const { return m_has_inout_fields; }

    /**
     * @brief Access a specific field in the record
     *
     * @param field  The field
     * @param record  The pointer to the beginning of the aggregation record
     *
     * @return The value
     */
    Value &
    access_field(const Field &field, uint8_t *record_ptr) const;

    /**
     * @brief Access a specific field in the record
     *
     * @param field  The field
     * @param record  The pointer to the beginning of the aggregation record
     *
     * @return The value
     */
    const Value &
    access_field(const Field &field, const uint8_t *record_ptr) const;

    /**
     * @brief Check whether the records of this view have a fixed size (as opposed to variable size)
     */
    bool is_fixed_size() const { return m_is_fixed_size; }

    uint64_t key_hash(const uint8_t *key) const
    {
        return XXH3_64bits(key, key_size(key));
    }

    bool key_equals(const uint8_t *key1, const uint8_t *key2) const
    {
        std::size_t key1_size = key_size(key1);
        std::size_t key2_size = key_size(key2);
        return key1_size == key2_size && std::memcmp(key1, key2, key1_size) == 0;
    }

    KeyHashFnType key_hasher() const
    {
        return [this](const uint8_t *key) { return key_hash(key); };
    }

    KeyEqualsFnType key_equaler() const
    {
        return [this](const uint8_t *key1, const uint8_t *key2) { return key_equals(key1, key2); };
    }

    RecOrdFnType rec_orderer() const
    {
        return [this](const uint8_t *rec1, const uint8_t *rec2) {
            return compare(rec1, rec2) == CmpResult::Lt;
        };
    }

    RecOrdFnType rec_reverse_orderer() const
    {
        return [this](const uint8_t *rec1, const uint8_t *rec2) {
            return compare(rec1, rec2) == CmpResult::Gt;
        };
    }

    CmpResult
    compare(const uint8_t *rec1, const uint8_t *rec2) const;

private:
    View() = default;

    std::vector<std::unique_ptr<Field>> m_fields;
    size_t m_key_count = 0;
    size_t m_value_count = 0;
    size_t m_key_size = 0;
    size_t m_value_size = 0;
    size_t m_output_limit = 0; // 0 = no limit
    std::vector<OrderField> m_order_fields;
    bool m_has_inout_fields = false;
    bool m_is_fixed_size = true;
};

} // aggregator
} // fdsdump

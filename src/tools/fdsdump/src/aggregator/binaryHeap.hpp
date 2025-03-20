/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Binary heap implementation
 *
 * Copyright: (C) 2024 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <vector>
#include <algorithm>

namespace fdsdump {
namespace aggregator {

/**
 * @brief A generic binary heap implementation. It can be used for example
 *   as a priority queue.
 * @tparam ItemType  The item type
 * @tparam CompareFn The comparator function type
 */
template <typename ItemType, typename CompareFn>
class BinaryHeap {
public:
    /**
     * @brief Constructs a new instance.
     * @param[in] compare The comparator
     */
    BinaryHeap(CompareFn compare) : m_compare(compare) {}

    /**
     * @brief Push an item onto the heap.
     * @param[in] item The item
     */
    void
    push(ItemType item)
    {
        m_items.push_back(item);
        sift_up(m_items.size() - 1);
    }

    /**
     * @brief Push an item onto the heap and also pop one off.
     * @param[in] item  The item to push
     * @return The popped of item
     */
    ItemType
    push_pop(ItemType item)
    {
        if (m_compare(item, m_items[0])) {
            ItemType result = m_items[0];
            m_items[0] = item;
            sift_down(0);
            return result;

        } else {
            return item;
        }
    }

    /**
     * @brief Pop an item off the top of the heap.
     * @return The item
     */
    ItemType
    pop()
    {
        ItemType result = m_items[0];
        m_items[0] = m_items[m_items.size() - 1];
        m_items.pop_back();
        sift_down(0);
        return result;
    }

    /**
     * @brief Get the item at the top of the heap.
     * @return The item
     */
    ItemType top() const { return m_items[0]; }

    /**
     * @brief Get the size of the heap
     * @return The number of items the heap holds
     */
    size_t size() const { return m_items.size(); }

private:
    std::vector<ItemType> m_items;
    CompareFn m_compare;

    void
    sift_up(size_t idx)
    {
        while (idx > 0) {
            size_t parent = (idx - 1) / 2;
            if (m_compare(m_items[parent], m_items[idx])) {
                std::swap(m_items[parent], m_items[idx]);
                idx = parent;
            } else {
                break;
            }
        }
    }

    void
    sift_down(size_t idx)
    {
        for (;;) {
            size_t left = 2 * idx + 1;
            size_t right = 2 * idx + 2;
            size_t smallest = idx;

            if (left < m_items.size() && m_compare(m_items[smallest], m_items[left])) {
                smallest = left;
            }
            if (right < m_items.size() && m_compare(m_items[smallest], m_items[right])) {
                smallest = right;
            }

            if (smallest == idx) {
                break;
            }

            std::swap(m_items[smallest], m_items[idx]);

            idx = smallest;
        }
    }
};

} // aggregator
} // fdsdump

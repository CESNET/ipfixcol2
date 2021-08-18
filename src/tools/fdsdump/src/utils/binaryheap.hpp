/**
 * \file src/utils/binaryheap.hpp
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * \brief Generic binary heap
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
#include <algorithm>

/**
 * \brief      A generic binary heap implementation (can be used for example as a priority queue)
 *
 * \tparam     ItemType   The item type
 * \tparam     CompareFn  The comparator function type
 */
template <typename ItemType, typename CompareFn>
class BinaryHeap {
public:
    /**
     * \brief      Constructs a new instance.
     *
     * \param[in]  compare  The comparator
     */
    BinaryHeap(CompareFn compare) : m_compare(compare) {}

    /**
     * \brief      Push an item onto the heap
     *
     * \param[in]  item  The item
     */
    void
    push(ItemType item)
    {
        m_items.push_back(item);
        sift_up(m_items.size() - 1);
    }

    /**
     * \brief      Push an item onto the heap and also pop one off
     *
     * \param[in]  item  The item to push
     *
     * \return     The popped of item
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
     * \brief      Pop an item off the top of the heap
     *
     * \return     The item
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
     * \brief      Get the item at the top of the heap
     *
     * \return     The item
     */
    ItemType top() const { return m_items[0]; }

    /**
     * \brief      Get the size of the heap
     *
     * \return     The number of items the heap holds
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

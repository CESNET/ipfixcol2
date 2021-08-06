#pragma once

#include <vector>
#include <algorithm>

template <typename ItemType, typename CompareFn>
class BinaryHeap {
public:
    BinaryHeap(CompareFn compare) : m_compare(compare) {}

    void
    push(ItemType item)
    {
        m_items.push_back(item);
        sift_up(m_items.size() - 1);
    }

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

    ItemType
    pop()
    {
        ItemType result = m_items[0];
        m_items[0] = m_items[m_items.size() - 1];
        m_items.pop_back();
        sift_down(0);
        return result;
    }

    ItemType top() const { return m_items[0]; }

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

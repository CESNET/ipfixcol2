/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Thread-safe queue implementation
 * @date 2025
 *
 * Copyright(c) 2025 CESNET z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>

/**
 * @brief A thread-safe queue
 */
template <typename Item>
class SyncQueue {
public:
    /**
     * @brief Put an item into the queue
     *
     * @param item The item
     */
    void put(Item item)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_items.push(item);
        m_size = m_items.size();
        m_avail_cv.notify_all();
    }

    /**
     * @brief Get an item from the queue, block and wait if there aren't any
     *
     * @return The item
     */
    Item get()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        while (true) {
            if (!m_items.empty()) {
                auto item = m_items.front();
                m_items.pop();
                m_size = m_items.size();
                return item;
            }
            m_avail_cv.wait(lock);
        }
    }

    /**
     * @brief Try to get an item from the queue, return immediately if there aren't any
     *
     * @return The item or std::nullopt
     */
    std::optional<Item> try_get()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (!m_items.empty()) {
            auto item = m_items.front();
            m_items.pop();
            m_size = m_items.size();
            return item;
        } else {
            return std::nullopt;
        }
    }

    /**
     * @brief Get the current size of the queue
     *
     * @return The number of items in the queue
     */
    std::size_t size()
    {
        return m_size;
    }

private:
    std::atomic_size_t m_size = 0;
    std::queue<Item> m_items;
    std::mutex m_mutex;
    std::condition_variable m_avail_cv;
};

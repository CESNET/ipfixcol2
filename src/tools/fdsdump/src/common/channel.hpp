/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Channel implementation for communication between threads
 *
 * Copyright: (C) 2024 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <common/common.hpp>

#include <mutex>
#include <queue>
#include <condition_variable>
#include <chrono>

namespace fdsdump {

template <typename T>
class Channel {
    DISABLE_COPY_AND_MOVE(Channel)

public:
    /**
     * @brief An object that is thrown when the channel closes and a put/get operation is issued
     */
    struct ChannelClosed {};

    /**
     * @brief Create an instance of this channel
     */
    Channel() = default;

    /**
     * @brief Destruct the channel
     */
    ~Channel()
    {
        close();
    }

    /**
     * @brief Put/send a value over the channel
     *
     * @param value  The value
     *
     * @throw ChannelClosed if the channel is closed
     */
    void
    put(const T& value)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (m_closed) {
            throw ChannelClosed {};
        }
        m_queue.emplace(value);
        m_cv.notify_one();
    }

    /**
     * @brief Same as `Channel::put`
     */
    void
    operator<<(const T& value)
    {
        put(value);
    }

    /**
     * @brief Get/receive a value from the channel
     *
     * @param[out] value  Where the value will be stored
     *
     * @throw ChannelClosed if the channel is closed
     */
    void
    get(T& value)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        while (m_queue.empty()) {
            if (m_closed) {
                throw ChannelClosed {};
            }
            m_cv.wait(lock);
        }
        value = m_queue.front();
        m_queue.pop();

    }

    /**
     * @brief Same as `Channel::get(T& value)`
     */
    void
    operator>>(T& value)
    {
        get(value);
    }

    /**
     * @brief Get/receive a value from the channel
     *
     * @param[out] value  Where the value will be stored (if retrieved)
     * @param[in] timeout  Maximum time to wait for a value to be available
     *
     * @return true if we got the value, false if timeout elapsed and no value was present
     *
     * @throw ChannelClosed if the channel is closed
     */
    bool
    get(T& value, std::chrono::milliseconds timeout)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (m_queue.empty()) {
            if (m_closed) {
                throw ChannelClosed {};
            }
            m_cv.wait_for(lock, timeout);
            if (m_closed) {
                throw ChannelClosed {};
            }
            if (m_queue.empty()) {
                return false;
            }
        }
        value = m_queue.front();
        m_queue.pop();
        return true;
    }

    /**
     * @brief Get/receive a value from the channel and throw it away
     *
     * @param[in] timeout  Maximum time to wait for a value to be available
     *
     * @return true if we got the value, false if timeout elapsed and no value was present
     *
     * @throw ChannelClosed if the channel is closed
     */
    bool
    get(std::chrono::milliseconds timeout)
    {
        T throwaway;
        return get(throwaway, timeout);
    }

    /**
     * @brief Get/receive a value from the channel without waiting
     *
     * @param[out] value  Where the value will be stored (if retrieved)
     *
     * @return true if we got the value, false if no value was immediately present
     *
     * @throw ChannelClosed if the channel is closed
     */
    bool
    get_nowait(T& value)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (m_queue.empty()) {
            if (m_closed) {
                throw ChannelClosed {};
            }
            return false;
        }
        value = m_queue.front();
        m_queue.pop();
        return true;
    }

    /**
     * @brief Block till a value becomes available
     *
     * @throw ChannelClosed if the channel is closed
     */
    void
    wait()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        while (m_queue.empty()) {
            if (m_closed) {
                throw ChannelClosed {};
            }
            m_cv.wait(lock);
        }
    }

    /**
     * @brief Block till a value becomes available
     *
     * @param timeout  The maximum time to wait for
     *
     * @return true if there is a value present, false if the timeout elapsed and no value is present
     *
     * @throw ChannelClosed if the channel is closed
     */
    bool
    wait(std::chrono::milliseconds timeout)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (!m_queue.empty()) {
            return true;
        }
        if (m_closed) {
            throw ChannelClosed {};
        }
        m_cv.wait_for(lock, timeout);
        if (!m_queue.empty()) {
            return true;
        }
        if (m_closed) {
            throw ChannelClosed {};
        }
        return false;
    }

    /**
     * @brief Close the channel interrupting all currently blocking operations
     *        and disallowing any further operations
     *
     * After this call, all subsequent and currently blocking will throw ChannelClosed
     */
    void
    close()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_closed = true;
        m_cv.notify_all();
    }

private:
    std::queue<T> m_queue;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    bool m_closed = false;
};

} // fdsdump

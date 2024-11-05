#pragma once

#include <clickhouse/client.h>
#include <fmt/core.h>
#include <ipfixcol2.h>

#include <condition_variable>
#include <mutex>
#include <queue>
#include <stdexcept>

struct Nonmoveable {
    Nonmoveable() = default;
    Nonmoveable(Nonmoveable&&) = delete;
    Nonmoveable& operator=(Nonmoveable&&) = delete;
};

struct Noncopyable {
    Noncopyable() = default;
    Noncopyable(const Noncopyable&) = delete;
    Noncopyable& operator=(const Noncopyable&) = delete;
};

class Error : public std::runtime_error {
public:
    template <typename ...Args>
    Error(Args&& ...args) : std::runtime_error(fmt::format(args...)) {}
};

class Stats {
public:
    Stats()
    {
        reset();
    }

    void reset()
    {
        m_records_processed = 0;
        m_start_time = std::time(nullptr);
    }

    void add_records(uint64_t num_records)
    {
        m_records_processed += num_records;
    }

    void print()
    {
        uint64_t secs_diff = std::time(nullptr) - m_start_time;
        double recs_per_sec = double(m_records_processed) / secs_diff;
        fmt::println("Avg recs per sec: {:.2f}", recs_per_sec);
    }

private:
    uint64_t m_records_processed;
    std::time_t m_start_time;
};

template <typename Item>
class SyncQueue {
public:
    void put(Item item)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_items.push(item);
        m_avail_cv.notify_all();
    }

    Item get()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        while (true) {
            if (!m_items.empty()) {
                auto item = m_items.front();
                m_items.pop();
                return item;
            }
            m_avail_cv.wait(lock);
        }
    }

private:
    std::queue<Item> m_items;
    std::mutex m_mutex;
    std::condition_variable m_avail_cv;
};


void print_block(const clickhouse::Block& block);

/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Multi-threaded runner
 *
 * Copyright: (C) 2024 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <aggregator/threadedAggregator.hpp>

#include <aggregator/print.hpp>
#include <aggregator/viewFactory.hpp>
#include <common/common.hpp>
#include <common/ieMgr.hpp>

#include <algorithm>

namespace fdsdump {
namespace aggregator {

ThreadedAggregator::ThreadedAggregator(
    const std::string &aggregation_keys,
    const std::string &aggregation_values,
    const std::string &input_filter,
    const std::vector<std::string> &input_file_patterns,
    const std::string &order_by,
    unsigned int num_threads,
    bool biflow_autoignore,
    bool merge_results,
    unsigned int merge_topk,
    Channel<ThreadedAggregator *> &notify_channel
) :
    m_num_threads(num_threads),
    m_input_filter(input_filter),
    m_order_by(order_by),
    m_biflow_autoignore(biflow_autoignore),
    m_merge_results(merge_results),
    m_merge_topk(merge_topk),
    m_aggregators(num_threads),
    m_threadinfo(num_threads),
    m_notify_channel(notify_channel)
{
    m_view = ViewFactory::create_unique_view(aggregation_keys, aggregation_values, order_by, 0);

    for (const auto& pattern : input_file_patterns) {
        for (const auto& file : glob_files(pattern)) {
            FlowProvider provider;
            provider.add_file(file);
            m_total_flows += provider.get_total_flow_count();
            m_total_files++;
            m_files.push(file);
        }
    }
}

void ThreadedAggregator::start()
{
    m_main_thread = std::thread([this]() {
        run();
    });
}

void ThreadedAggregator::run()
{
    m_aggregator_state = AggregatorState::started;

    m_threads.clear();
    for (unsigned int i = 0; i < m_num_threads; i++) {
        std::thread thread([this, i]() {
            thread_worker(i);
        });
        m_threads.push_back(std::move(thread));
    }
    m_aggregator_state = AggregatorState::aggregating;

    while (true) {
        ThreadInfo *info = nullptr;
        m_worker_notify_channel >> info;

        if (info->state == AggregatorState::errored) {
            m_aggregator_state = AggregatorState::errored;
            m_exception = info->exception;
            m_notify_channel << this;
            for (auto &info : m_threadinfo) {
                // Cancel all worker threads
                info.cancelled = true;
            }
            return;

        } else if (info->state == AggregatorState::sorting) {
            bool all_atleast_sorting = std::all_of(m_threadinfo.begin(), m_threadinfo.end(), [](const ThreadInfo &info)
                { return info.state == AggregatorState::sorting || info.state == AggregatorState::finished; });
            if (all_atleast_sorting) {
                m_aggregator_state = AggregatorState::sorting;
            }
        } else if (info->state == AggregatorState::finished) {
            bool all_finished = std::all_of(m_threadinfo.begin(), m_threadinfo.end(), [](const ThreadInfo &info)
                { return info.state == AggregatorState::finished; });
            if (all_finished) {
                break;
            }
        }
    }

    if (m_merge_results) {
        if (m_merge_topk == 0) {
            perform_all_merge();
        } else {
            perform_topk_merge();
        }
    }

    m_aggregator_state = AggregatorState::finished;
    m_notify_channel << this;

    // Wait for the worker threads to gracefully finish
    int i = 0;
    for (auto &thread : m_threads) {
        thread.join();
        i++;
        LOG_DEBUG << "Waiting for worker thread to finish (" << i << "/" << m_threads.size() << ")";
    }
}

std::vector<uint8_t *>& ThreadedAggregator::get_results()
{
    assert(m_merge_results);
    return *m_items;
}

void ThreadedAggregator::thread_worker(unsigned int thread_id)
{
    ThreadInfo &info = m_threadinfo[thread_id];

    try {
        const View& view = *m_view.get();

        FlowProvider flows;
        flows.set_biflow_autoignore(m_biflow_autoignore);
        if (!m_input_filter.empty()) {
            flows.set_filter(m_input_filter);
        }

        m_aggregators[thread_id] = std::unique_ptr<Aggregator>(new Aggregator(view));
        Aggregator &aggregator = *m_aggregators[thread_id].get();

        info.state = AggregatorState::aggregating;
        m_worker_notify_channel << &info;

        while (true) {
            Flow *flow = flows.next_record();

            if (info.cancelled.load(std::memory_order_relaxed)) { // Doesn't matter if we process few extra values
                info.state = AggregatorState::finished;
                m_worker_notify_channel << &info;
                return;
            }

            if (!flow) {
                std::lock_guard<std::mutex> guard(m_files_mutex);
                if (m_files.empty()) {
                    break;
                }
                flows.add_file(m_files.front());
                m_files.pop();
                info.processed_files.fetch_add(1, std::memory_order_relaxed);
                continue;
            }

            info.processed_flows.store(flows.get_processed_flow_count(), std::memory_order_relaxed);
            aggregator.process_record(*flow);
        }


        // Figure out whether the per-thread tables should be sorted, which is if
        // there is something to sort by, and we are not merging or we are merging by top K
        if (!m_order_by.empty() && (!m_merge_results || m_merge_topk > 0)) {
            // Check for cancellation first
            if (info.cancelled) {
                info.state = AggregatorState::finished;
                m_worker_notify_channel << &info;
                return;
            }

            info.state = AggregatorState::sorting;
            m_worker_notify_channel << &info;
            aggregator.sort_items();
        }

        info.state = AggregatorState::finished;
        m_worker_notify_channel << &info;

    } catch (...) {
        info.state = AggregatorState::errored;
        info.exception = std::current_exception();
        m_worker_notify_channel << &info;
    }
}

void
ThreadedAggregator::perform_all_merge()
{
    assert(m_merge_results);
    assert(m_merge_topk == 0);

    m_aggregator_state = AggregatorState::merging;
    Aggregator& main = *m_aggregators[0].get();
    for (size_t i = 1; i < m_aggregators.size(); i++) {
        Aggregator& other = *m_aggregators[i].get();
        main.merge(other);
    }

    m_aggregator_state = AggregatorState::sorting;
    main.sort_items();
    m_items = &main.items();
}

void
ThreadedAggregator::perform_topk_merge()
{
    assert(m_merge_results);
    assert(m_merge_topk > 0);

    m_aggregator_state = AggregatorState::merging;
    auto tables = get_tables();
    m_threshold_algorithm.reset(new ThresholdAlgorithm(
                tables,
                *m_view.get(),
                m_merge_topk));
    while (!m_threshold_algorithm->check_finish_condition() && !m_threshold_algorithm->out_of_items()) {
        m_threshold_algorithm->process_row();
    }

    m_aggregator_state = AggregatorState::sorting;
    auto &items = m_threshold_algorithm->m_result_table->items();
    sort_records(*m_view.get(), items);
    m_items = &items;
}

uint64_t ThreadedAggregator::get_processed_flows() const
{
    uint64_t processed_flows = 0;
    for (const auto& info : m_threadinfo) {
        processed_flows += info.processed_flows;
    }
    return processed_flows;
}

uint64_t ThreadedAggregator::get_processed_files() const
{
    uint64_t processed_files = 0;
    for (const auto& info : m_threadinfo) {
        processed_files += info.processed_files;
    }
    return processed_files;
}

uint64_t ThreadedAggregator::get_total_flows() const
{
    return m_total_flows;
}

uint64_t ThreadedAggregator::get_total_files() const
{
    return m_total_files;
}

AggregatorState ThreadedAggregator::get_aggregator_state() const
{
    return m_aggregator_state;
}

std::vector<HashTable *> ThreadedAggregator::get_tables()
{
    std::vector<HashTable *> tables;
    for (auto &aggregator : m_aggregators) {
        tables.emplace_back(&aggregator->m_table);
    }
    return tables;
}

void ThreadedAggregator::cancel()
{
    for (auto &info : m_threadinfo) {
        // Cancel all worker threads
        info.cancelled = true;
    }
}

void ThreadedAggregator::join()
{
    m_main_thread.join();
}

const char *aggregator_state_to_str(AggregatorState aggregator_state)
{
    switch (aggregator_state) {
    case AggregatorState::none:
        return "none";
    case AggregatorState::errored:
        return "errored";
    case AggregatorState::started:
        return "started";
    case AggregatorState::aggregating:
        return "aggregating";
    case AggregatorState::sorting:
        return "sorting";
    case AggregatorState::merging:
        return "merging";
    case AggregatorState::finished:
        return "finished";
    }

    return "<invalid>";
}

} // aggregator
} // fdsdump

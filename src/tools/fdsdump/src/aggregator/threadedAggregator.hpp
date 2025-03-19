/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Multi-threaded runner
 *
 * Copyright: (C) 2024 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <aggregator/aggregator.hpp>
#include <aggregator/thresholdAlgorithm.hpp>
#include <aggregator/view.hpp>
#include <common/common.hpp>
#include <common/flow.hpp>
#include <common/flowProvider.hpp>
#include <common/logger.hpp>
#include <common/channel.hpp>

#include <atomic>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <exception>

namespace fdsdump {
namespace aggregator {

enum class AggregatorState : uint8_t {
    none,
    errored,
    started,
    aggregating,
    sorting,
    merging,
    finished
};

const char *aggregator_state_to_str(AggregatorState aggregator_state);

class ThreadedAggregator {
    DISABLE_COPY_AND_MOVE(ThreadedAggregator)

    struct ThreadInfo {
        std::atomic<uint64_t> processed_files{0};
        std::atomic<uint64_t> processed_flows{0};
        std::atomic<AggregatorState> state{AggregatorState::none};
        std::exception_ptr exception = nullptr;
        std::atomic<bool> cancelled{false};
    };

public:
    /**
     * @brief Create a new threaded aggregator instance
     *
     * @param aggregation_keys The aggregation keys
     * @param aggregation_values The aggregation values
     * @param input_filter The input record filter
     * @param input_file_patterns The input file patterns (paths w/ glob support)
     * @param order_by The fields to order by
     * @param num_threads The number of threads
     * @param biflow_autoignore Whether the biflow autoignore option should be applied
     * @param merge_results Merge output results or not
     * @param merge_topk If merge results is enabled, how many top K values to obtain (0 = all
     *                   values, i.e. no top K is performed and everything is merged)
     * @param notify_channel The channel to send state notifications through
     */
    ThreadedAggregator(
        const std::string& aggregation_keys,
        const std::string& aggregation_values,
        const std::string& input_filter,
        const std::vector<std::string>& input_file_patterns,
        const std::string& order_by,
        unsigned int num_threads,
        bool biflow_autoignore,
        bool merge_results,
        unsigned int merge_topk,
        Channel<ThreadedAggregator *> &notify_channel
    );

    /**
     * @brief Start the aggregation. This is a non-blocking operation that starts the threads that
     *        will perform the aggregation.
     */
    void start();

    /**
     * @brief Get the exception that occured if the aggregation state is _errored_
     */
    std::exception_ptr get_exception() { return m_exception; }

    /**
     * @brief Get the final results if merging was enabled
     */
    std::vector<uint8_t *>& get_results();

    /**
     * @brief Get the number of processed flows (for progress reporting)
     */
    uint64_t get_processed_flows() const;

    /**
     * @brief Get the number of processed files (for progress reporting)
     */
    uint64_t get_processed_files() const;

    /**
     * @brief Get the total number of flows that will be processed (for progress reporting)
     */
    uint64_t get_total_flows() const;

    /**
     * @brief Get the total number of files that will be processed (for progress reporting)
     */
    uint64_t get_total_files() const;

    /**
     * @brief Get the current aggregator state
     */
    AggregatorState get_aggregator_state() const;

    /**
     * @brief Get the per-thread tables if merging was disabled
     */
    std::vector<HashTable *> get_tables();

    /**
     * @brief Cancel the aggregation
     */
    void cancel();

    /**
     * @brief Block until the execution of all aggregator threads finishes
     */
    void join();

private:
    AggregatorState m_aggregator_state = AggregatorState::none;

    unsigned int m_num_threads;
    std::string m_input_filter;
    std::string m_order_by;
    bool m_biflow_autoignore;
    bool m_merge_results;
    unsigned int m_merge_topk;

    std::thread m_main_thread;

    std::unique_ptr<View> m_view;
    std::vector<std::thread> m_threads;
    std::vector<std::unique_ptr<Aggregator>> m_aggregators;

    std::queue<std::string> m_files;
    std::mutex m_files_mutex;

    std::vector<ThreadInfo> m_threadinfo;
    std::vector<uint8_t *> *m_items;

    uint64_t m_total_flows = 0;
    uint64_t m_total_files = 0;

    Channel<ThreadedAggregator *> &m_notify_channel;

    Channel<ThreadInfo *> m_worker_notify_channel;

    std::exception_ptr m_exception = nullptr;

    std::unique_ptr<ThresholdAlgorithm> m_threshold_algorithm;

    void thread_worker(unsigned int thread_id);

    void run();

    void perform_all_merge();

    void perform_topk_merge();
};

} // aggregator
} // fdsdump

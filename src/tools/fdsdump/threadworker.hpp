#pragma once

#include <memory>
#include <thread>
#include <memory>
#include "common.hpp"
#include "filelist.hpp"
#include "config.hpp"
#include "aggregator.hpp"
#include "sorter.hpp"

class ThreadWorker {
public:
    std::thread m_thread;

    std::unique_ptr<Aggregator> m_aggregator;

    unsigned int m_processed_files = 0;

    unsigned long long m_processed_records = 0;

    bool m_done = false;

    ThreadWorker(const Config &config, FileList &file_list);

    ThreadWorker(const ThreadWorker &) = delete;

    ThreadWorker(ThreadWorker &&) = delete;

private:
    const Config m_config;

    FileList &m_file_list;

    void
    run();
};
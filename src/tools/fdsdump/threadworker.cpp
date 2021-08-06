#include "threadworker.hpp"
#include "reader.hpp"
#include "ipfixfilter.hpp"

ThreadWorker::ThreadWorker(const Config &config, FileList &file_list) :
    m_config(config),
    m_file_list(file_list)
{
    m_thread = std::thread([this]() { this->run(); });
}

void
ThreadWorker::run()
{
    unique_fds_iemgr iemgr = make_iemgr();
    Reader reader(*iemgr.get());
    IPFIXFilter ipfix_filter(m_config.input_filter.c_str(), *iemgr.get());
    m_aggregator = std::unique_ptr<Aggregator>(new Aggregator(m_config.view_def));

    fds_drec drec;

    std::string filename;

    while (m_file_list.pop(filename)) {
        reader.set_file(filename);

        while (reader.read_record(drec)) {
            m_processed_records++;

            if (!ipfix_filter.record_passes(drec)) {
                continue;
            }

            m_aggregator->process_record(drec);
        }

        m_processed_files++;
    }

#if 0
    if (m_config.max_output_records > 0 && !m_config.sort_field.empty()) {
        auto compare_fn = get_compare_fn(m_config.sort_field, m_config.view_def);
        m_aggregator->make_top_n(m_config.max_output_records * m_config.num_threads, compare_fn);
    }
#endif

    m_done = true;
}


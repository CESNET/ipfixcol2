#include <iostream>
#include <memory>
#include <functional>
#include <algorithm>
#include <vector>
#include <queue>
#include <mutex>
#include <thread>
#include "reader.hpp"
#include "aggregator.hpp"
#include "table_printer.hpp"
#include "ipfix_filter.hpp"
#include "aggregate_filter.hpp"
#include "config.hpp"
#include "common.hpp"
#include "sorter.hpp"

std::mutex g_file_list_mutex;
std::queue<std::string> g_file_list;

static bool
take_next_file(std::string &filename)
{
    std::lock_guard<std::mutex> guard(g_file_list_mutex);

    if (g_file_list.empty()) {
        return false;
    }

    filename = std::move(g_file_list.front());
    g_file_list.pop();
    return true;
}


struct WorkerContext {
    std::thread thread;
    std::unique_ptr<Aggregator> aggregator;

    unsigned int processed_files = 0;
    unsigned long long processed_records = 0;

    bool done = false;
};

static void
run_worker(const Config &config, WorkerContext &context)
{
    unique_fds_iemgr iemgr = make_iemgr();

    Reader reader(*iemgr.get());

    IPFIXFilter ipfix_filter(config.input_filter.c_str(), *iemgr.get());

    std::unique_ptr<Aggregator> aggregator(new Aggregator(config.view_def));

    AggregateFilter aggregate_filter(config.output_filter.c_str(), config.view_def);

    fds_drec drec;

    std::string filename;

    while (take_next_file(filename)) {

        reader.set_file(filename);

        while (reader.read_record(drec)) {

            context.processed_records++;

            if (!ipfix_filter.record_passes(drec)) {
                continue;
            }

            aggregator->process_record(drec);
        }

        context.processed_files++;
    }

    context.aggregator = std::move(aggregator);
    context.done = true;
}

int
main(int argc, char *argv[])
{
    int rc;

    unique_fds_iemgr iemgr = make_iemgr();

    Config config;
    rc = config_from_args(argc, argv, config, *iemgr.get());
    if (rc != 0) {
        return rc;
    }

    std::vector<std::string> files = match_files(config.input_file);
    if (files.empty()) {
        fprintf(stderr, "No input files matched!\n");
        return 1;
    }

    Reader reader(*iemgr.get());

    unsigned long long total_records = 0;
    unsigned int total_files = files.size();

    for (const auto &file : files) {
        reader.set_file(file);
        total_records += reader.total_number_of_records();

        g_file_list.push(file);
    }

    int num_threads = 1;
    std::vector<std::unique_ptr<WorkerContext>> workers;

    for (int i = 0; i < num_threads; i++) {
        std::unique_ptr<WorkerContext> holder(new WorkerContext());
        WorkerContext *worker_context = holder.get();
        worker_context->thread = std::thread([&config, worker_context]() {
            run_worker(config, *worker_context);
        });
        workers.push_back(std::move(holder));
    }

    int i = 0;
    while (true) {
        unsigned int processed_files = 0;
        unsigned long long processed_records = 0;

        bool done = true;
        for (auto &wc : workers) {
            if (!wc->done) {
                done = false;
            }

            processed_files += wc->processed_files;
            processed_records += wc->processed_records;
        }

        if (done) {
            break;
        }

        printf("* Aggregating - processed %u/%u files, %llu/%llu records over %d threads",
               processed_files, total_files,
               processed_records, total_records,
               num_threads);

        switch (i % 4) {
        case 0: printf("   "); break;
        case 1: printf(".  "); break;
        case 2: printf(".. "); break;
        case 3: printf("..."); break;
        }

        printf("\r");

        fflush(stdout);
        i++;

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    //TODO: we could start merging earlier

    Aggregator &aggregator = *workers[0]->aggregator.get();

    for (int i = 1; i < num_threads; i++) {
        printf("* Aggregating - merging table %d                                                            \r", i);
        fflush(stdout);
        aggregator.merge(*workers[i]->aggregator.get());
        workers[i]->aggregator.release();
    }


    printf("* Aggregating done                                                                     \r");
    fflush(stdout);

    //aggregator.print_debug_info();

    AggregateFilter aggregate_filter(config.output_filter.c_str(), config.view_def);

    std::unique_ptr<Printer> printer(new TablePrinter(config.view_def));
    if (auto *table_printer = dynamic_cast<TablePrinter *>(printer.get())) {
        table_printer->m_translate_ip_addrs = config.translate_ip_addrs;
    }

    auto records = aggregator.records();

    if (!config.sort_field.empty()) {
        printf("* Sorting %lu records...\r", records.size());
        fflush(stdout);
        sort_records(records, config.sort_field, config.view_def);
        printf("* Sorting done                        \r");
        fflush(stdout);
    }

    printf("                                                                           \r");


    uint64_t output_counter = 0;
    printer->print_prologue();
    for (auto *record : records) {
        AggregateRecord &arec = *record;
        if (aggregate_filter.record_passes(arec)) {
            if (config.max_output_records > 0 && output_counter == config.max_output_records) {
                break;
            }
            printer->print_record(arec);
            output_counter++;
        }
    }
    printer->print_epilogue();


    for (auto &wc : workers) {
        wc->thread.join();
    }


#if 0
    int rc;
    unique_fds_iemgr iemgr = make_iemgr();

    Config config;
    rc = config_from_args(argc, argv, config, *iemgr.get());
    if (rc != 0) {
        return rc;
    }

    //std::cout << "key size: " << config.view_def.keys_size << "\n";
    FileList file_list;
    file_list.add_files(config.input_file);

    if (!file_list.has_next_file()) {
        fprintf(stderr, "no input files\n");
    }


    Reader reader(*iemgr.get());
    reader.set_file(file_list.take_next_file());

    IPFIXFilter ipfix_filter(config.input_filter.c_str(), *iemgr.get());

    Aggregator aggregator(config.view_def);

    AggregateFilter aggregate_filter(config.output_filter.c_str(), config.view_def);

    std::unique_ptr<Printer> printer(new TablePrinter(config.view_def));
    if (auto *table_printer = dynamic_cast<TablePrinter *>(printer.get())) {
        table_printer->m_translate_ip_addrs = config.translate_ip_addrs;
    }

    fds_drec drec;
    uint64_t i = 0;
    uint64_t total_records = 0;
    if (config.max_input_records == 0) {
        total_records = reader.total_number_of_records();
    } else {
        total_records = std::min(reader.total_number_of_records(), config.max_input_records);
    }

    while (true) {

        if (!reader.read_record(drec)) {
            if (file_list.has_next_file()) {
                reader.set_file(file_list.take_next_file());

                if (config.max_input_records == 0) {
                    total_records += reader.total_number_of_records();
                } else {
                    total_records = std::min(total_records + reader.total_number_of_records(), config.max_input_records);
                }

                continue;
            }

            break;
        }

        if (ipfix_filter.record_passes(drec)) {
            aggregator.process_record(drec);
        }

        i++;

        if (i % 100 == 1) {
            printf("\r%lu/%lu %.2lf%%", i, total_records, double{i} / double{total_records} * 100.0);
        }

        if (i == config.max_input_records && config.max_input_records != 0) {
            break;
        }
    }
    printf("                           \r");

    //aggregator.print_debug_info();

    auto records = aggregator.records();

    if (!config.sort_field.empty()) {
        sort_records(records, config.sort_field, config.view_def);
    }

    uint64_t output_counter = 0;
    printer->print_prologue();
    for (auto *record : records) {
        AggregateRecord &arec = *record;
        if (aggregate_filter.record_passes(arec)) {
            if (config.max_output_records > 0 && output_counter == config.max_output_records) {
                break;
            }
            printer->print_record(arec);
            output_counter++;
        }
    }
    printer->print_epilogue();
#endif
}



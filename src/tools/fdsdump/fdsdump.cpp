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
#include "tableprinter.hpp"
#include "ipfixfilter.hpp"
#include "aggregatefilter.hpp"
#include "config.hpp"
#include "common.hpp"
#include "sorter.hpp"
#include "filelist.hpp"
#include "threadworker.hpp"

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
    config.num_threads = 2;

    std::vector<SortField> sort_fields;
    for (auto &field : config.view_def.key_fields) {
        if (field.name == config.sort_field) {
            sort_fields.push_back({ &field, false });
        }
    }

    for (auto &field : config.view_def.value_fields) {
        if (field.name == config.sort_field) {
            sort_fields.push_back({ &field, false });
        }
    }


    //assert(!config.sort_field.empty()); // FIXME

    FileList file_list;
    file_list.add_files(config.input_file);
    if (file_list.empty()) {
        fprintf(stderr, "No input files matched!\n");
        return 1;
    }

    Reader reader(*iemgr.get());

    unsigned long long total_records = 0;
    unsigned int total_files = file_list.length();

    for (const auto &file : file_list) {
        reader.set_file(file);
        total_records += reader.records_count();
    }

    std::vector<std::unique_ptr<ThreadWorker>> workers;

    for (int i = 0; i < config.num_threads; i++) {
        auto worker = std::unique_ptr<ThreadWorker>(new ThreadWorker(config, file_list));
        workers.push_back(std::move(worker));
    }

    int i = 0;
    while (true) {
        unsigned int processed_files = 0;
        unsigned long long processed_records = 0;

        bool done = true;
        for (auto &worker : workers) {
            if (!worker->m_done) {
                done = false;
            }

            processed_files += worker->m_processed_files;
            processed_records += worker->m_processed_records;
        }

        if (done) {
            break;
        }

        printf("* Aggregating - processed %u/%u files, %llu/%llu records over %d threads",
               processed_files, total_files,
               processed_records, total_records,
               config.num_threads);

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
#if 0
    Aggregator &aggregator = * workers[0]->m_aggregator.get();

    for (int i = 1; i < config.num_threads; i++) {
        printf("* Aggregating - merging table %d                                                            \r", i);
        fflush(stdout);
        aggregator.merge(*workers[i]->m_aggregator.get());
        workers[i]->m_aggregator.release();
    }

#endif
    printf("* Aggregating done                                                                     \r");
    fflush(stdout);

#if 0
    if (config.max_output_records > 0 && !config.sort_field.empty()) {
        printf("* Sorting %lu records...\r", aggregator.m_items.size());
        fflush(stdout);
        auto compare_fn = get_compare_fn(config.sort_field, config.view_def);
        aggregator.make_top_n(config.max_output_records, compare_fn);
        printf("* Sorting done                        \r");
        fflush(stdout);
    }
#endif


    std::vector<Aggregator *> aggregators;
    for (auto &worker : workers) {
        aggregators.push_back(worker->m_aggregator.get());
    }

    auto records = make_top_n(config.view_def, aggregators, config.max_output_records, sort_fields);


    // auto records = aggregator.m_items;

#if 0
    if (!config.sort_field.empty()) {
        printf("* Sorting %lu records...\r", records.size());
        fflush(stdout);
        sort_records(records, config.sort_field, config.view_def);
        printf("* Sorting done                        \r");
        fflush(stdout);
    }
#endif

    AggregateFilter aggregate_filter(config.output_filter.c_str(), config.view_def);

    std::unique_ptr<Printer> printer(new TablePrinter(config.view_def));
    if (auto *table_printer = dynamic_cast<TablePrinter *>(printer.get())) {
        table_printer->m_translate_ip_addrs = config.translate_ip_addrs;
    }

    printf("                                                                           \r");


    uint64_t output_counter = 0;
    printer->print_prologue();
    for (uint8_t *record : records) {
        if (aggregate_filter.record_passes(record)) {
            if (config.max_output_records > 0 && output_counter == config.max_output_records) {
                break;
            }
            printer->print_record(record);
            output_counter++;
        }
    }
    printer->print_epilogue();



    for (auto &wc : workers) {
        wc->m_thread.join();
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
        total_records = reader.records_count();
    } else {
        total_records = std::min(reader.records_count(), config.max_input_records);
    }

    while (true) {

        if (!reader.read_record(drec)) {
            if (file_list.has_next_file()) {
                reader.set_file(file_list.take_next_file());

                if (config.max_input_records == 0) {
                    total_records += reader.records_count();
                } else {
                    total_records = std::min(total_records + reader.records_count(), config.max_input_records);
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



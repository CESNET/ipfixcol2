/**
 * \file src/fdsdump.cpp
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * \brief Main file
 *
 * Copyright (C) 2021 CESNET, z.s.p.o.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of the Company nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * ALTERNATIVELY, provided that this notice is retained in full, this
 * product may be distributed under the terms of the GNU General Public
 * License (GPL) version 2 or later, in which case the provisions
 * of the GPL apply INSTEAD OF those given above.
 *
 * This software is provided ``as is, and any express or implied
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose are disclaimed.
 * In no event shall the company or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even
 * if advised of the possibility of such damage.
 *
 */
#include <iostream>
#include <memory>
#include <functional>
#include <algorithm>
#include <vector>
#include <queue>
#include <chrono>
#include <sys/stat.h>

#include "common.hpp"
#include "cmdargs.hpp"
#include "ipfix/fdsreader.hpp"
#include "ipfix/ipfixfilter.hpp"
#include "view/aggregator.hpp"
#include "view/aggregatefilter.hpp"
#include "view/tableprinter.hpp"
#include "view/print.hpp"
#include "utils/util.hpp"
#include "utils/filelist.hpp"
#include "utils/threadrunner.hpp"

static std::string
pretty_size(double size)
{
    const std::vector<const char *> prefixes = { "", "ki", "Mi", "Gi", "Ti", "Pi"};
    size_t i = 0;
    while (i < prefixes.size() - 1 && size >= 1024) {
        size /= 1024.0;
        i++;
    }

    char buf[64];
    snprintf(buf, 64, "%.2lf %sB", size, prefixes[i]);
    return std::string(buf);
}

struct AggregateWorker {
    unique_fds_iemgr iemgr;
    std::unique_ptr<IPFIXFilter> input_filter;
    std::unique_ptr<Aggregator> aggregator;
    FileList *file_list;
    ViewDefinition view_def;
    std::vector<SortField> sort_fields;

    unsigned long processed_files = 0;
    unsigned long processed_records = 0;

    void operator()() {
        FDSReader reader(iemgr.get());
        fds_drec drec;
        std::string filename;

        while (file_list->pop(filename)) {
            reader.set_file(filename);

            while (reader.read_record(drec)) {
                processed_records++;

                if (!input_filter->record_passes(drec)) {
                    continue;
                }

                aggregator->process_record(drec);
            }

            processed_files++;
        }

        sort_records(aggregator->items(), sort_fields, view_def);
    }
};

void
run(int argc, char **argv)
{
    //NOTE: This is still very much a work in progress

    unique_fds_iemgr iemgr = make_iemgr();

    CmdArgs args = parse_cmd_args(argc, argv);

    if (args.print_help) {
        print_usage();
        return;
    }

    if (args.input_filter.empty()) {
        args.input_filter = "true";
    }

    if (args.output_filter.empty()) {
        args.output_filter = "true";
    }

    // Set-up file list
    FileList file_list;

    for (const auto &file_pattern : args.input_file_patterns) {
        file_list.add_files(file_pattern);
    }

    if (file_list.empty()) {
        throw ArgError("No input files matched");
    }


    ViewDefinition view_def = make_view_def(args.aggregate_keys, args.aggregate_values, iemgr.get());
    std::vector<SortField> sort_fields = make_sort_def(view_def, args.sort_fields);

    if (args.num_threads == 0) {
        throw ArgError("Number of threads cannot be zero");
    }

    FDSReader reader(iemgr.get());

    unsigned long total_records = 0;
    unsigned long total_files = file_list.length();
    unsigned long total_file_bytes = 0;
    std::chrono::steady_clock::time_point begin_time = std::chrono::steady_clock::now();

    for (const auto &file : file_list) {
        reader.set_file(file);
        total_records += reader.records_count();

        struct stat statbuf;
        if (stat(file.c_str(), &statbuf) == 0) {
            total_file_bytes += statbuf.st_size;
        }

    }

    // Set up and run workers
    std::vector<AggregateWorker> workers;

    for (int i = 0; i < args.num_threads; i++) {
        AggregateWorker worker;
        worker.iemgr = make_iemgr();
        worker.view_def = view_def;
        worker.sort_fields = sort_fields;
        worker.aggregator = std::unique_ptr<Aggregator>(new Aggregator(view_def));
        worker.input_filter = std::unique_ptr<IPFIXFilter>(new IPFIXFilter(args.input_filter.c_str(), iemgr.get()));
        worker.file_list = &file_list;
        workers.push_back(std::move(worker));
    }

    ThreadRunner<AggregateWorker> runner(workers);

    int i = 0;
    while (!runner.poll()) {
        unsigned long processed_files = 0;
        unsigned long processed_records = 0;

        for (auto &worker : workers) {
            processed_files += worker.processed_files;
            processed_records += worker.processed_records;
        }

        printf("* Aggregating - processed %lu/%lu files, %lu/%lu records over %d threads",
               processed_files, total_files,
               processed_records, total_records,
               args.num_threads);

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

    //runner.join();


    printf("* Aggregating done                                                                     \r");
    fflush(stdout);

    // Collect aggregators
    std::vector<Aggregator *> aggregators;
    for (auto &worker : workers) {
        aggregators.push_back(worker.aggregator.get());
    }

    auto records = make_top_n(view_def, aggregators, args.output_limit, sort_fields);

    std::chrono::steady_clock::time_point end_time = std::chrono::steady_clock::now();


    AggregateFilter aggregate_filter(args.output_filter.c_str(), view_def);

    std::unique_ptr<Printer> printer(new TablePrinter(view_def));
    if (auto *table_printer = dynamic_cast<TablePrinter *>(printer.get())) {
        table_printer->m_translate_ip_addrs = args.translate_ip_addrs;
    }

    printf("                                                                           \r");


    uint64_t output_counter = 0;
    printer->print_prologue();
    for (uint8_t *record : records) {
        if (aggregate_filter.record_passes(record)) {
            if (args.output_limit > 0 && output_counter == args.output_limit) {
                break;
            }
            printer->print_record(record);
            output_counter++;
        }
    }
    printer->print_epilogue();



    double secs = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - begin_time).count() / 1000.0;

    fprintf(stderr, "Performance stats:\n");
    fprintf(stderr, "  Time:         %12.2f s\n", secs);
    fprintf(stderr, "  File:         %12s/s\n", pretty_size(total_file_bytes / secs).c_str());
    fprintf(stderr, "  Records:      %12.2lf/s\n", total_records / secs);

    runner.join();
}

int
main(int argc, char **argv)
{
    try {
        run(argc, argv);
        return 0;

    } catch (const ArgError &ex) {
        fprintf(stderr, "Argument error: %s\n", ex.what());
        return 1;

    } catch (const std::bad_alloc &ex) {
        fprintf(stderr, "Out of memory error\n");
        return 1;
/*
    } catch (const std::exception &ex) {
        fprintf(stderr, "Caught exception %s\n", ex.what());
        return 1;

    } catch (...) {
        fprintf(stderr, "Caught general exception\n");
        return 1;
*/
    }
}

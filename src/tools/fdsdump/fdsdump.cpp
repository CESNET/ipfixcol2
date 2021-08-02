#include <iostream>
#include <memory>
#include <functional>
#include <algorithm>
#include <vector>
#include "reader.hpp"
#include "aggregator.hpp"
#include "table_printer.hpp"
#include "ipfix_filter.hpp"
#include "aggregate_filter.hpp"
#include "config.hpp"
#include "common.hpp"
#include "sorter.hpp"

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

    //std::cout << "key size: " << config.view_def.keys_size << "\n";


    Reader reader{config.input_file.c_str(), *iemgr.get()};
    IPFIXFilter ipfix_filter{config.input_filter.c_str(), *iemgr.get()};
    Aggregator aggregator{config.view_def};
    AggregateFilter aggregate_filter{config.output_filter.c_str(), config.view_def};
    std::unique_ptr<Printer> printer{new TablePrinter{config.view_def}};
    if (auto *table_printer = dynamic_cast<TablePrinter *>(printer.get())) {
        table_printer->m_translate_ip_addrs = config.translate_ip_addrs;
    }

    fds_drec drec;
    uint64_t i = 0;
    uint64_t max_record_count = config.max_input_records == 0 ? reader.total_number_of_records() : config.max_input_records;

    while ((rc = reader.read_record(drec)) == FDS_OK) {
        if (ipfix_filter.record_passes(drec)) {
            aggregator.process_record(drec);
        }
        i++;
        if (i % 100 == 1) {
            printf("\r%lu/%lu %.2lf%%", i, max_record_count, double{i} / double{max_record_count} * 100.0);
        }
        if (i == max_record_count) {
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

}



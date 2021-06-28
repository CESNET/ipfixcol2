#include <iostream>
#include <memory>
#include <functional>
#include <algorithm>
#include "reader.hpp"
#include "aggregator.hpp"
#include "table_printer.hpp"
#include "ipfix_filter.hpp"
#include "aggregate_filter.hpp"
#include "config.hpp"

static unique_fds_iemgr
make_iemgr();

static void
sort_records(std::vector<aggregate_record_s *> &records, const std::string &sort_field);

int
main(int argc, char *argv[])
{
    int rc;

    config_s config;
    rc = config_from_args(argc, argv, config);
    if (rc != 0) {
        return rc;
    }

    unique_fds_iemgr iemgr = make_iemgr();

    Reader reader{config.input_file.c_str(), *iemgr.get()};
    IPFIXFilter ipfix_filter{config.input_filter.c_str(), *iemgr.get()};
    Aggregator aggregator{config.aggregate_config};
    AggregateFilter aggregate_filter{config.output_filter.c_str()};
    std::unique_ptr<Printer> printer{new TablePrinter};

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

    auto records = aggregator.records();

    if (!config.sort_field.empty()) {
        sort_records(records, config.sort_field);
    }

    uint64_t output_counter;
    printer->print_prologue();
    for (auto *record : records) {
        aggregate_record_s &arec = *record;
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

static unique_fds_iemgr
make_iemgr()
{
    int rc;

    unique_fds_iemgr iemgr;
    iemgr.reset(fds_iemgr_create());
    if (!iemgr) {
        throw std::bad_alloc();
    }

    rc = fds_iemgr_read_dir(iemgr.get(), fds_api_cfg_dir());
    if (rc != FDS_OK) {
        throw std::runtime_error("cannot read iemgr definitions");
    }

    return iemgr;
}

static void
sort_records(std::vector<aggregate_record_s *> &records, const std::string &sort_field)
{
    std::function<bool(const aggregate_record_s *, const aggregate_record_s *)> compare_fn;

    if (sort_field == "packets") {
        compare_fn = [](const aggregate_record_s *a, const aggregate_record_s *b) {
            return a->packets > b->packets;
        };
    } else if (sort_field == "bytes") {
        compare_fn = [](const aggregate_record_s *a, const aggregate_record_s *b) {
            return a->bytes > b->bytes;
        };
    } else if (sort_field == "flows") {
        compare_fn = [](const aggregate_record_s *a, const aggregate_record_s *b) {
            return a->flows > b->flows;
        };
    }

    std::sort(records.begin(), records.end(), compare_fn);
}

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
sort_records(std::vector<AggregateRecord *> &records, const std::string &sort_field, ViewDefinition &config);

int
main(int argc, char *argv[])
{
    int rc;

    Config config;
    rc = config_from_args(argc, argv, config);
    if (rc != 0) {
        return rc;
    }

    unique_fds_iemgr iemgr = make_iemgr();

    Reader reader{config.input_file.c_str(), *iemgr.get()};
    IPFIXFilter ipfix_filter{config.input_filter.c_str(), *iemgr.get()};
    Aggregator aggregator{config.view_def};
    AggregateFilter aggregate_filter{config.output_filter.c_str(), config.view_def};
    std::unique_ptr<Printer> printer{new TablePrinter{config.view_def}};

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
        sort_records(records, config.sort_field, config.view_def);
    }

    uint64_t output_counter;
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
sort_records(std::vector<AggregateRecord *> &records, const std::string &sort_field, ViewDefinition &view_def)
{
    std::function<bool(AggregateRecord *, AggregateRecord *)> compare_fn;

    if (sort_field == "packets") {
        compare_fn = [&](AggregateRecord *a, AggregateRecord *b) {
            return get_value_by_name(view_def, a->data, "packets")->u64 > get_value_by_name(view_def, b->data, "packets")->u64;
        };
    } else if (sort_field == "bytes") {
        compare_fn = [&](AggregateRecord *a, AggregateRecord *b) {
            return get_value_by_name(view_def, a->data, "bytes")->u64 > get_value_by_name(view_def, b->data, "bytes")->u64;
        };
    } else if (sort_field == "flows") {
        compare_fn = [&](AggregateRecord *a, AggregateRecord *b) {
            return get_value_by_name(view_def, a->data, "flows")->u64 > get_value_by_name(view_def, b->data, "flows")->u64;
        };
    }

    std::sort(records.begin(), records.end(), compare_fn);
}

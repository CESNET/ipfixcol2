/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Aggregator mode
 *
 * Copyright: (C) 2024 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <vector>

#include <aggregator/aggregator.hpp>
#include <aggregator/mode.hpp>
#include <aggregator/print.hpp>
#include <aggregator/printer.hpp>
#include <aggregator/threadedAggregator.hpp>
#include <aggregator/viewFactory.hpp>
#include <common/channel.hpp>
#include <common/flowProvider.hpp>

#include <memory>
#include <iomanip>

namespace fdsdump {
namespace aggregator {

static void print_results(const Options &opts, std::vector<uint8_t *> items)
{
    View view = ViewFactory::create_view(
            opts.get_aggregation_keys(),
            opts.get_aggregation_values(),
            opts.get_order_by(),
            opts.get_output_limit());

    std::unique_ptr<Printer> printer = printer_factory(
            view,
            opts.get_output_specifier());

    const size_t rec_limit = opts.get_output_limit();
    size_t rec_printed = 0;

    printer->print_prologue();

    for (uint8_t *record : items) {
        if (rec_limit != 0 && rec_printed >= rec_limit) {
            break;
        }

        printer->print_record(record);
        rec_printed++;
    }

    printer->print_epilogue();
}

void
mode_aggregate(const Options &opts)
{
    Channel<ThreadedAggregator *> notify_channel;
    ThreadedAggregator aggregator(
        opts.get_aggregation_keys(),
        opts.get_aggregation_values(),
        opts.get_input_filter(),
        opts.get_input_file_patterns(),
        opts.get_order_by(),
        opts.get_num_threads(),
        opts.get_biflow_autoignore(),
        true,
        opts.get_output_limit(),
        notify_channel);
    aggregator.start();

    while (true) {
        notify_channel.get(std::chrono::milliseconds(1000));
        auto state = aggregator.get_aggregator_state();

        auto state_str = aggregator_state_to_str(state);
        if (state == AggregatorState::aggregating) {
            auto percent = (double(aggregator.get_processed_flows()) / double(aggregator.get_total_flows())) * 100.0;
            LOG_INFO << "Status: " << state_str << " (" << std::fixed << std::setprecision(2) << percent << "%)";
        } else {
            LOG_INFO << "Status: " << state_str;
        }

        if (state == AggregatorState::errored) {
            std::rethrow_exception(aggregator.get_exception());
        }

        if (state == AggregatorState::finished) {
            break;
        }
    }

    aggregator.join();

    print_results(opts, aggregator.get_results());
}

} // aggregator
} // fdsdump

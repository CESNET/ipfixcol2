/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Aggregator main
 */

#include <common/flowProvider.hpp>
#include <aggregator/aggregator.hpp>
#include <aggregator/mode.hpp>
#include <aggregator/printer.hpp>
#include <aggregator/view.hpp>
#include <aggregator/viewFactory.hpp>

#include <memory>

namespace fdsdump {
namespace aggregator {

void
mode_aggregate(const Options &opts)
{
    std::shared_ptr<View> view = ViewFactory::create_view(
            opts.get_aggregation_keys(), 
            opts.get_aggregation_values(), 
            opts.get_order_by());

    std::unique_ptr<Printer> printer = printer_factory(
            view,
            opts.get_output_specifier());
    FlowProvider flows;
    Aggregator aggr(*view.get());

    const size_t rec_limit = opts.get_output_limit();
    size_t rec_printed = 0;

    flows.set_biflow_autoignore(opts.get_biflow_autoignore());

    if (!opts.get_input_filter().empty()) {
        flows.set_filter(opts.get_input_filter());
    }

    for (const auto &it : opts.get_input_files()) {
        flows.add_file(it);
    }

    while (true) {
        Flow *flow = flows.next_record();

        if (!flow) {
            break;
        }

        aggr.process_record(*flow);
    }

    aggr.sort_items();
    printer->print_prologue();

    for (uint8_t *record : aggr.items()) {
        if (rec_limit != 0 && rec_printed >= rec_limit) {
            break;
        }

        printer->print_record(record);
        rec_printed++;
    }

    printer->print_epilogue();
}

} // aggregator
} // fdsdump

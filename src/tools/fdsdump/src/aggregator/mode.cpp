
#include <vector>

#include <common/flowProvider.hpp>
#include <common/ieMgr.hpp>

#include <aggregator/aggregator.hpp>
#include <aggregator/mode.hpp>
#include <aggregator/printer.hpp>
#include <aggregator/viewOld.hpp>
#include <aggregator/sort.hpp>

namespace fdsdump {
namespace aggregator {

void
mode_aggregate(const Options &opts)
{
    ViewDefinition view_def = make_view_def(
            opts.get_aggregation_keys(),
            opts.get_aggregation_values(),
            IEMgr::instance().ptr());
    std::vector<SortField> sort_fields = make_sort_def(
            view_def,
            opts.get_order_by());
    std::unique_ptr<Printer> printer = printer_factory(
            view_def,
            opts.get_output_specifier());
    FlowProvider flows;
    Aggregator aggr(view_def);

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

    sort_records(aggr.items(), sort_fields, view_def);

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

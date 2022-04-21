#include "lister.hpp"
#include "printer.hpp"
#include "storageSorted.hpp"

static void
mode_list_unordered(const shared_iemgr &iemgr, const Options &opts, FlowProvider &flows)
{
    auto printer = printer_factory(iemgr, opts.get_output_specifier());
    const size_t rec_limit = opts.get_output_limit();
    size_t rec_printed = 0;

    printer->print_prologue();

    while (rec_limit == 0 || rec_printed < rec_limit) {
        Flow *flow = flows.next_record();

        if (!flow) {
            break;
        }

        rec_printed += printer->print_record(flow);
    }

    printer->print_epilogue();
}

static void
mode_list_ordered(const shared_iemgr &iemgr, const Options &opts, FlowProvider &flows)
{
    StorageSorter sorter {opts.get_order_by(), iemgr};
    StorageSorted storage {sorter, opts.get_output_limit()};
    auto printer = printer_factory(iemgr, opts.get_output_specifier());

    while (true) {
        Flow *flow = flows.next_record();

        if (!flow) {
            break;
        }

        storage.insert(flow);
    }

    printer->print_prologue();

    for (const auto &rec : storage) {
        const Flow *flow = &rec.get_flow_const();
        printer->print_record(const_cast<Flow *>(flow));
    }

    printer->print_epilogue();
}

void
mode_list(const shared_iemgr &iemgr, const Options &opts)
{
    FlowProvider flows {iemgr};

    flows.set_biflow_autoignore(opts.get_biflow_autoignore());

    if (!opts.get_input_filter().empty()) {
        flows.set_filter(opts.get_input_filter());
    }

    for (const auto &it : opts.get_input_files()) {
        flows.add_file(it);
    }

    if (opts.get_order_by().empty()) {
        mode_list_unordered(iemgr, opts, flows);
    } else {
        mode_list_ordered(iemgr, opts, flows);
    }
}

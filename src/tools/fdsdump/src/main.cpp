
#include <cstdlib>
#include <iostream>
#include <vector>
#include <unistd.h>

#include <libfds.h>

#include "common.hpp"
#include "filelist.hpp"
#include "flowProvider.hpp"
#include "options.hpp"
#include "printer.hpp"
#include "storageSorted.hpp"


static unique_iemgr iemgr_prepare(const std::string &path)
{
    unique_iemgr iemgr {fds_iemgr_create(), &fds_iemgr_destroy};
    int ret;

    if (!iemgr) {
        throw std::runtime_error("fds_iemgr_create() has failed");
    }

    ret = fds_iemgr_read_dir(iemgr.get(), path.c_str());
    if (ret != FDS_OK) {
        const std::string err_msg = fds_iemgr_last_err(iemgr.get());
        throw std::runtime_error("fds_iemgr_read_dir() failed: " + err_msg);
    }

    return iemgr;
}

static void fds_iemgr_deleter(fds_iemgr_t *mgr) {
    if (mgr != nullptr) {
        fds_iemgr_destroy(mgr);
    }
}

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

static void
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

int
main(int argc, char *argv[])
{
    try {
        Options options {argc, argv};
        shared_iemgr iemgr {nullptr, fds_iemgr_deleter};

        iemgr = iemgr_prepare(std::string(fds_api_cfg_dir()));

        switch (options.get_mode()) {
        case Options::Mode::list:
            mode_list(iemgr, options);
            break;

        default:
            throw std::runtime_error("Invalid mode");
        }
    } catch (const std::exception &ex) {
        std::cerr << "ERROR: " << ex.what() << std::endl;
        return EXIT_FAILURE;
    }

    return 0;
}


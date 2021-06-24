#include <iostream>
#include <memory>
#include "reader.hpp"
#include "aggregator.hpp"
#include "table_printer.hpp"

int
main(int argc, char *argv[])
{
    Reader reader{"/home/michal/work/data/flows.20200917000000.fds"};
    Aggregator aggregator;
    std::unique_ptr<Printer> printer{new TablePrinter};

    fds_drec drec;
    int rc;
    int i = 0;
    while ((rc = reader.read_record(drec)) == FDS_OK) {
        aggregator.process_record(drec);
        i++;
        if (i == 10000)
            break;
    }

    printer->print_prologue();
    for (auto *record : aggregator.records()) {
        printer->print_record(*record);
    }
    printer->print_epilogue();

}
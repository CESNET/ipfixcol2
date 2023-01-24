
#pragma once

#include <string>
#include "printer.hpp"

namespace fdsdump {
namespace statistics {

class TablePrinter : public Printer {
public:
    TablePrinter(const std::string &args);

    virtual
    ~TablePrinter();

    virtual void
    print_prologue() override;

    virtual void
    print_stats(const fds_file_stats &stats) override;

    virtual void
    print_epilogue() override;
};

} // statistics
} // fdsdump

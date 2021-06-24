#pragma once

#include "printer.hpp"

class TablePrinter : public Printer
{
public:
    TablePrinter();

    ~TablePrinter() override;

    void
    print_prologue() override;

    void
    print_record(aggregate_record_s &record) override;

    void
    print_epilogue() override;

};

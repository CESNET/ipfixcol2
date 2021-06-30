#pragma once

#include "printer.hpp"

class TablePrinter : public Printer
{
public:
    TablePrinter(AggregateConfig aggregate_config);

    ~TablePrinter() override;

    void
    print_prologue() override;

    void
    print_record(AggregateRecord &record) override;

    void
    print_epilogue() override;

private:
    AggregateConfig m_aggregate_config;
};

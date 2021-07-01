#pragma once

#include "printer.hpp"

class TablePrinter : public Printer
{
public:
    TablePrinter(ViewDefinition aggregate_config);

    ~TablePrinter() override;

    void
    print_prologue() override;

    void
    print_record(AggregateRecord &record) override;

    void
    print_epilogue() override;

private:
    ViewDefinition m_aggregate_config;
};

#pragma once

#include "printer.hpp"

class TablePrinter : public Printer
{
public:
    bool m_translate_ip_addrs;

    TablePrinter(ViewDefinition view_def);

    ~TablePrinter() override;

    void
    print_prologue() override;

    void
    print_record(AggregateRecord &record) override;

    void
    print_epilogue() override;

private:
    ViewDefinition m_view_def;
};

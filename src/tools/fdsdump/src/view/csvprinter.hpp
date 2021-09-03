#pragma once

#include "printer.hpp"
#include "view/view.hpp"
#include "view/print.hpp"

class CSVPrinter : public Printer
{
public:
    CSVPrinter(ViewDefinition view_def);

    ~CSVPrinter() override;

    void
    print_prologue() override;

    void
    print_record(uint8_t *record) override;

    void
    print_epilogue() override;

private:
    ViewDefinition m_view_def;
};
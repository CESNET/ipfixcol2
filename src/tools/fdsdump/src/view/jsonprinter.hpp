#pragma once

#include "printer.hpp"
#include "view/view.hpp"
#include "view/print.hpp"

class JSONPrinter : public Printer
{
public:
    JSONPrinter(ViewDefinition view_def);

    ~JSONPrinter() override;

    void
    print_prologue() override;

    void
    print_record(uint8_t *record) override;

    void
    print_epilogue() override;

private:
    ViewDefinition m_view_def;
    bool m_first_record = true;
};
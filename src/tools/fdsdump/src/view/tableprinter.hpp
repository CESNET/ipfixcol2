#pragma once

#include "printer.hpp"
#include "view/view.hpp"

class TablePrinter : public Printer
{
public:
    bool m_translate_ip_addrs;

    TablePrinter(ViewDefinition view_def);

    ~TablePrinter() override;

    void
    print_prologue() override;

    void
    print_record(uint8_t *record) override;

    void
    print_epilogue() override;

private:
    ViewDefinition m_view_def;
};

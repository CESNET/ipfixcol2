#pragma once

#include <string>

#include "printer.hpp"

namespace fdsdump {
namespace aggregator {

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
    void append_field(const ViewField &field, ViewValue *value);
    void append_value(const ViewField &field, ViewValue *value);

    ViewDefinition m_view_def;
    std::string m_buffer;
    size_t m_rec_printed = 0;
};

} // aggregator
} // fdsdump

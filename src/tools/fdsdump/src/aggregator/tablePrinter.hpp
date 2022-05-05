/**
 * @file
 * @author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * @brief Table printer implementation
 */
#pragma once

#include <string>

#include "printer.hpp"

namespace fdsdump {
namespace aggregator {

class TABLEPrinter : public Printer
{
public:
    bool m_translate_ip_addrs;

    TABLEPrinter(ViewDefinition view_def);

    ~TABLEPrinter() override;

    void
    print_prologue() override;

    void
    print_record(uint8_t *record) override;

    void
    print_epilogue() override;

private:
    ViewDefinition m_view_def;
    std::string m_buffer;
};

} // aggregator
} // fdsdump

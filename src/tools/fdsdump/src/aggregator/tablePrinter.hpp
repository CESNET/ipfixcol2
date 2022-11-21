/**
 * @file
 * @author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * @brief Table printer implementation
 */
#pragma once

#include <string>

#include <aggregator/field.hpp>
#include <aggregator/printer.hpp>
#include <aggregator/value.hpp>
#include <aggregator/view.hpp>

namespace fdsdump {
namespace aggregator {

class TABLEPrinter : public Printer
{
public:
    bool m_translate_ip_addrs = false;

    TABLEPrinter(std::shared_ptr<View> view);

    ~TABLEPrinter() override;

    void
    print_prologue() override;

    void
    print_record(uint8_t *record) override;

    void
    print_epilogue() override;

private:
    std::shared_ptr<View> m_view;
    std::string m_buffer;
};

} // aggregator
} // fdsdump

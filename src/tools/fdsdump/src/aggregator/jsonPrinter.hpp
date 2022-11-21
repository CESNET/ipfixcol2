/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief JSON printer
 */

#pragma once

#include <string>

#include <aggregator/field.hpp>
#include <aggregator/printer.hpp>
#include <aggregator/value.hpp>
#include <aggregator/view.hpp>

namespace fdsdump {
namespace aggregator {

class JSONPrinter : public Printer
{
public:
    JSONPrinter(std::shared_ptr<View> view);

    ~JSONPrinter() override;

    void
    print_prologue() override;

    void
    print_record(uint8_t *record) override;

    void
    print_epilogue() override;

private:
    void append_field(const Field &field, Value *value);
    void append_value(const Field &field, Value *value);
    void append_string_value(const Value *value);
    void append_octet_value(const Value *value);

    std::shared_ptr<View> m_view;
    std::string m_buffer;
    size_t m_rec_printed = 0;
};

} // aggregator
} // fdsdump

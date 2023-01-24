/**
 * @file
 * @author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * @brief Printer interface
 */

#pragma once

#include <cstdint>

#include "common/common.hpp"
#include "view.hpp"

namespace fdsdump {
namespace aggregator {

/**
 * @brief Printer interface
 */
class Printer
{
public:
    virtual
    ~Printer() {}

    virtual void
    print_prologue() = 0;

    virtual void
    print_record(uint8_t *record) = 0;

    virtual void
    print_epilogue() = 0;
};

std::unique_ptr<Printer>
printer_factory(ViewDefinition view_def, const std::string &manual);

} // aggregator
} // fdsdump

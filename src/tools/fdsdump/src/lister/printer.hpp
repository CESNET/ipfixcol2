
#pragma once

#include <functional>
#include <memory>
#include <string>

#include <libfds.h>

#include <common/common.hpp>
#include <common/flow.hpp>

namespace fdsdump {
namespace lister {

/**
 * @brief Interface of an output printer for flow records.
 *
 */
class Printer {
public:
    virtual
    ~Printer() {};

    virtual void
    print_prologue() = 0;

    virtual unsigned int
    print_record(Flow *flow) = 0;

    virtual void
    print_epilogue() = 0;
};

std::unique_ptr<Printer>
printer_factory(const std::string &manual);

} // lister
} // fdsdump

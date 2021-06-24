#pragma once

#include "aggregator.hpp"

class Printer
{
public:
    virtual
    ~Printer() {}

    virtual void
    print_prologue() = 0;

    virtual void
    print_record(aggregate_record_s &record) = 0;

    virtual void
    print_epilogue() = 0;

};
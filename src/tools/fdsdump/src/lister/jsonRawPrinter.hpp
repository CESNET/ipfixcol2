
#pragma once

#include <string>

#include <lister/printer.hpp>

namespace fdsdump {
namespace lister {

class JsonRawPrinter : public Printer {
public:
    JsonRawPrinter(const std::string &args);

    virtual
    ~JsonRawPrinter();

    virtual void
    print_prologue() override {};

    virtual unsigned int
    print_record(Flow *flow) override;

    virtual void
    print_epilogue() override {};

private:
    void print_record(struct fds_drec *rec, uint32_t flags);

    char *m_buffer = nullptr;
    size_t m_buffer_size = 0;

    bool m_biflow_split = true;
    bool m_biflow_hide_reverse = false;
};

} // lister
} // fdsdump

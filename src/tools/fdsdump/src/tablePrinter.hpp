
#pragma once

#include <string>
#include <vector>

#include "common.hpp"
#include "field.hpp"
#include "printer.hpp"

class TablePrinter : public Printer {
public:
    TablePrinter(const shared_iemgr &iemgr, const std::string &args);

    virtual
    ~TablePrinter() = default;

    virtual void
    print_prologue() override;

    virtual unsigned int
    print_record(Flow *flow) override;

    virtual void
    print_epilogue() override {};

private:
    struct FieldInfo {
        Field m_field;
        std::string m_name;
        size_t m_width;

        FieldInfo(Field field, std::string name, size_t width)
            : m_field(field), m_name(name), m_width(width) {};
    };

    void parse_fields(const std::string &str, const shared_iemgr &iemgr);
    void parse_opts(const std::string &str);

    size_t data_length(fds_iemgr_element_type type);

    void print_record(struct fds_drec *rec, bool reverse);
    void print_value(struct fds_drec *rec, FieldInfo &field, bool reverse);
    void buffer_append(const struct fds_drec_field &field);

    std::vector<FieldInfo> m_fields;
    std::string m_buffer;
    bool m_biflow_split = true;
};

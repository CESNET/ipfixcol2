
#pragma once

#include <string>

#include "field.hpp"
#include "printer.hpp"

class JsonPrinter : public Printer {
public:
    JsonPrinter(const shared_iemgr &iemgr, const std::string &args);

    virtual
    ~JsonPrinter();

    virtual void
    print_prologue() override;

    virtual unsigned int
    print_record(Flow *flow) override;

    virtual void
    print_epilogue() override;

private:
    void parse_fields(const std::string &str, const shared_iemgr &iemgr);
    void parse_opts(const std::string &str);

    void print_record(struct fds_drec *rec, bool reverse);

    void append_value(struct fds_drec *rec, Field &field, bool reverse);
    void append_value(const struct fds_drec_field &field);

    void append_octet_array(const fds_drec_field &field);
    void append_uint(const fds_drec_field &field);
    void append_int(const fds_drec_field &field);
    void append_float(const fds_drec_field &field);
    void append_boolean(const fds_drec_field &field);
    void append_timestamp(const fds_drec_field &field);
    void append_string(const fds_drec_field &field);
    void append_mac(const fds_drec_field &field);
    void append_ip(const fds_drec_field &field);

    void append_null();
    void append_invalid();
    void append_unsupported();

    std::vector<Field> m_fields;
    std::string m_buffer;
    unsigned int m_rec_printed = 0;
    bool m_biflow_split = true;
};

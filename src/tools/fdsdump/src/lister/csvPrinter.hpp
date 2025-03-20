/**
 * @file
 * @author Lukas Hutak <hutak@cesnet.cz>
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Lister CSV printer
 *
 * Copyright: (C) 2024 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <string>

#include <common/field.hpp>

#include "printer.hpp"

namespace fdsdump {
namespace lister {

class CsvPrinter : public Printer {
public:
    CsvPrinter(const std::string &args);

    virtual
    ~CsvPrinter();

    virtual void
    print_prologue() override;

    virtual unsigned int
    print_record(Flow *flow) override;

    virtual void
    print_epilogue() override;

private:
    struct FieldInfo {
        Field m_field;
        std::string m_name;

        FieldInfo(Field field, std::string name)
            : m_field(field), m_name(name) {}
    };

    void parse_fields(const std::string &str);
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

    std::vector<FieldInfo> m_fields;
    std::string m_buffer;
    unsigned int m_rec_printed = 0;
    bool m_biflow_split = true;
    bool m_format_timestamp = true;
};

} // lister
} // fdsdump

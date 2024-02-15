/**
 * @file
 * @author Lukas Hutak <hutak@cesnet.cz>
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Lister table printer
 *
 * Copyright: (C) 2024 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <string>
#include <vector>

#include <common/common.hpp>
#include <common/field.hpp>

#include <lister/printer.hpp>

namespace fdsdump {
namespace lister {

class TablePrinter : public Printer {
public:
    TablePrinter(const std::string &args);

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

    void parse_fields(const std::string &str);
    void parse_opts(const std::string &str);

    size_t data_length(fds_iemgr_element_type type);

    void print_record(struct fds_drec *rec, bool reverse);
    void print_value(struct fds_drec *rec, FieldInfo &field, bool reverse);
    void buffer_append(const struct fds_drec_field &field);

    std::vector<FieldInfo> m_fields;
    std::string m_buffer;
    bool m_biflow_split = true;
};

} // lister
} // fdsdump

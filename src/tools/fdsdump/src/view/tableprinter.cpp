/**
 * \file src/view/tableprinter.cpp
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * \brief Table printer implementation
 *
 * Copyright (C) 2021 CESNET, z.s.p.o.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of the Company nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * ALTERNATIVELY, provided that this notice is retained in full, this
 * product may be distributed under the terms of the GNU General Public
 * License (GPL) version 2 or later, in which case the provisions
 * of the GPL apply INSTEAD OF those given above.
 *
 * This software is provided ``as is, and any express or implied
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose are disclaimed.
 * In no event shall the company or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even
 * if advised of the possibility of such damage.
 *
 */
#include "view/tableprinter.hpp"

#include <cstdio>

#include "ipfix/informationelements.hpp"
#include "view/print.hpp"

TablePrinter::TablePrinter(ViewDefinition view_def)
    : m_view_def(view_def)
{
}

TablePrinter::~TablePrinter()
{
}

void
TablePrinter::print_prologue()
{
    for (const auto &field : m_view_def.key_fields) {
        printf("%*s ", get_width(field), field.name.c_str());
    }

    for (const auto &field : m_view_def.value_fields) {
        printf("%*s ", get_width(field), field.name.c_str());
    }

    printf("\n");
}

void
TablePrinter::print_record(uint8_t *record)
{
    char buffer[1024] = {0}; //TODO: We might want to do this some other way
    ViewValue *value = (ViewValue *) record;

    for (const auto &field : m_view_def.key_fields) {
        print_value(field, *value, buffer, m_translate_ip_addrs);
        advance_value_ptr(value, field.size);
        printf("%*s ", get_width(field), buffer);
    }

    for (const auto &field : m_view_def.value_fields) {
        print_value(field, *value, buffer, m_translate_ip_addrs);
        advance_value_ptr(value, field.size);
        printf("%*s ", get_width(field), buffer);
    }

    printf("\n");
}

void
TablePrinter::print_epilogue()
{
}

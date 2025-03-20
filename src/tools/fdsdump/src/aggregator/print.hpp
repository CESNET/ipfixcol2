/**
 * @file
 * @author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * @brief View print functions
 *
 * Copyright: (C) 2024 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <aggregator/field.hpp>
#include <aggregator/value.hpp>
#include <aggregator/view.hpp>

namespace fdsdump {
namespace aggregator {

std::string
datetime_to_str(uint64_t ts_millisecs);

/**
 * @brief Get a width the field should have in a text form.
 * @param field  The field
 * @return The width.
 */
int
get_width(const Field &field);

/**
 * @brief Convert a byte value to hexadecimal value.
 * @param byte Value to convert
 */
std::string
char2hex(const char byte);

/**
 * @brief Transform a view value into a text form
 * @param[in]  field               The view field
 * @param[out] value               The view value
 * @param[out] buffer              The buffer where to append converted value
 */
void
print_value(
    const Field &field,
    Value &value,
    std::string &buffer);

void
debug_print_record(const View &view, uint8_t *record);



} // aggregator
} // fdsdump

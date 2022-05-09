/**
 * @file
 * @author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * @brief View print functions
 */
#pragma once

#include "view.hpp"

namespace fdsdump {
namespace aggregator {

/**
 * @brief Get a width the field should have in a text form.
 * @param field  The field
 * @return The width.
 */
int
get_width(const ViewField &field);

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
    const ViewField &field,
    ViewValue &value,
    std::string &buffer);

} // aggregator
} // fdsdump

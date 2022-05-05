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
 * @brief Convert datetime to a text form.
 * @param[out] buffer       The buffer to write the text form to
 * @param[in]  ts_millisecs The timestamp value in milliseconds since UNIX epoch
 */
void
datetime_to_str(char *buffer, uint64_t ts_millisecs);

/**
 * @brief Translate an IPv4 address into a domain name
 * @param address The IPv4 address
 * @param buffer  The buffer to store the domain name
 * @return true if the translation succeeded, false otherwise
 */
bool
translate_ipv4_address(uint8_t *address, char *buffer);

/**
 * @brief Translate an IPv6 address into a domain name
 * @param address  The IPv4 address
 * @param buffer   The buffer to store the domain name
 * @return true if the translation succeeded, false otherwise
 */
bool
translate_ipv6_address(uint8_t *address, char *buffer);


/**
 * @brief Transform a view value into a text form
 * @param[in]  field               The view field
 * @param[out] value               The view value
 * @param[out] buffer              The buffer to store the text form
 * @param[in]  translate_ip_addrs  Whether IP addresses should be translated to domain names
 */
void
print_value(
    const ViewField &field,
    ViewValue &value,
    char *buffer,
    bool translate_ip_addrs);

} // aggregator
} // fdsdump

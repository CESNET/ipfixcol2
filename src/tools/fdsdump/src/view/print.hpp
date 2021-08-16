#pragma once

#include "view/view.hpp"

int
get_width(const ViewField &field);

void
datetime_to_str(char *buffer, uint64_t ts_millisecs);

bool
translate_ipv4_address(uint8_t *address, char *buffer);

bool
translate_ipv6_address(uint8_t *address, char *buffer);

void
print_value(const ViewField &field, ViewValue &value, char *buffer, bool translate_ip_addrs);

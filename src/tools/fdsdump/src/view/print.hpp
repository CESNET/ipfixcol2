/**
 * \file src/view/print.hpp
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * \brief View print functions
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
#pragma once

#include "view/view.hpp"

/**
 * \brief      Get a width the field should have in a text form
 *
 * \param[in]  field  The field
 *
 * \return     The width.
 */
int
get_width(const ViewField &field);

/**
 * \brief      Convert datetime to a text form.
 *
 * \param[out] buffer        The buffer to write the text form to
 * \param[in]  ts_millisecs  The timestamp value in milliseconds since UNIX epoch
 */
void
datetime_to_str(char *buffer, uint64_t ts_millisecs);

/**
 * \brief      Translate an IPv4 address into a domain name
 *
 * \param      address  The IPv4 address
 * \param      buffer   The buffer to store the domain name
 *
 * \return     true if the translation succeeded, false otherwise
 */
bool
translate_ipv4_address(uint8_t *address, char *buffer);

/**
 * \brief      Translate an IPv6 address into a domain name
 *
 * \param      address  The IPv4 address
 * \param      buffer   The buffer to store the domain name
 *
 * \return     true if the translation succeeded, false otherwise
 */
bool
translate_ipv6_address(uint8_t *address, char *buffer);


/**
 * \brief      Transform a view value into a text form
 *
 * \param[in]  field               The view field
 * \param[out] value               The view value
 * \param[out] buffer              The buffer to store the text form
 * \param[in]  translate_ip_addrs  Whether IP addresses should be translated to domain names
 */
void
print_value(const ViewField &field, ViewValue &value, char *buffer, bool translate_ip_addrs);

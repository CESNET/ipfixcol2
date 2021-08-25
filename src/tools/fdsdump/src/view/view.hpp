/**
 * \file src/view/view.hpp
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * \brief View definitions
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

#include <string>
#include <vector>
#include <cstdint>

#include <libfds.h>

/**
 * \brief      A representation of an IP address that can hold both an IPv4 or an IPv6 address
 */
struct IPAddress
{
    uint8_t length;
    uint8_t address[16];
};

/**
 * \brief      The possible data types a view value can be
 */
enum class DataType
{
    Unassigned,
    IPAddress,
    IPv4Address,
    IPv6Address,
    MacAddress,
    Unsigned8,
    Signed8,
    Unsigned16,
    Signed16,
    Unsigned32,
    Signed32,
    Unsigned64,
    Signed64,
    DateTime,
    String128B,
};

/**
 * \brief      The possible view value forms
 */
union ViewValue
{
    IPAddress ip;
    uint8_t ipv4[4];
    uint8_t ipv6[16];
    uint8_t mac[6];
    uint8_t u8;
    uint16_t u16;
    uint32_t u32;
    uint64_t u64;
    uint64_t ts_millisecs;
    int8_t i8;
    int16_t i16;
    int32_t i32;
    int64_t i64;
    char str[128];
};

/**
 * \brief      The possible kinds of a view field
 */
enum class ViewFieldKind
{
    Unassigned,
    VerbatimKey,
    IPv4SubnetKey,
    IPv6SubnetKey,
    BidirectionalIPv4SubnetKey,
    BidirectionalIPv6SubnetKey,
    SourceIPAddressKey,
    DestinationIPAddressKey,
    BidirectionalIPAddressKey,
    BidirectionalPortKey,
    BiflowDirectionKey,
    SumAggregate,
    MinAggregate,
    MaxAggregate,
    CountAggregate
};

/**
 * \brief      The direction in case of a bidirectional field
 */
enum class Direction
{
    Unassigned,
    In,
    Out
};

/**
 * \brief      The view field definition
 */
struct ViewField
{
    size_t size;
    size_t offset;
    std::string name;
    uint32_t pen;
    uint16_t id;
    DataType data_type;
    ViewFieldKind kind;
    Direction direction;
    struct
    {
        uint8_t prefix_length;
    } extra;
};

/**
 * \brief      The view definition
 */
struct ViewDefinition
{
    bool bidirectional;

    std::vector<ViewField> key_fields;
    std::vector<ViewField> value_fields;
    size_t keys_size;
    size_t values_size;
    bool biflow_enabled;
};

/**
 * \brief      Make a view definition
 *
 * \param[in]  keys    The aggregation keys specified in a text form
 * \param[in]  values  The aggregation values specified in a text
 * \param      iemgr   The iemgr instance
 *
 * \return     The view definition.
 */
ViewDefinition
make_view_def(const std::string &keys, const std::string &values, fds_iemgr_t *iemgr);

/**
 * \brief      Find a field in a view definition by its name
 *
 * \param      def   The view definition
 * \param[in]  name  The field name
 *
 * \return     The view field or nullptr if not found.
 */
ViewField *
find_field(ViewDefinition &def, const std::string &name);

/**
 * \brief      Advance view value pointer
 *
 * \param      value       The value pointer reference
 * \param[in]  value_size  The amount to advance by
 */
static inline void
advance_value_ptr(ViewValue *&value, size_t value_size)
{
    value = (ViewValue *) ((uint8_t *) value + value_size);
}

/**
 * \brief      Consturct an IPv4 address
 *
 * \param      address  The address bytes
 *
 * \return     The IPAddress instance
 */
static inline IPAddress
make_ipv4_address(uint8_t *address)
{
    IPAddress ip = {};
    ip.length = 4;
    memcpy(ip.address, address, 4);
    return ip;
}

/**
 * \brief      Construct an IPv6 address
 *
 * \param      address  The address bytes
 *
 * \return     The IPAddress instance
 */
static inline IPAddress
make_ipv6_address(uint8_t *address)
{
    IPAddress ip = {};
    ip.length = 16;
    memcpy(ip.address, address, 16);
    return ip;
}

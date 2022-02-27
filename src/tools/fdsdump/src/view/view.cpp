/**
 * \file src/view/view.cpp
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
#include "view.hpp"

#include "common.hpp"
#include "utils/util.hpp"
#include "ipfix/informationelements.hpp"

ViewField *
find_field(ViewDefinition &def, const std::string &name)
{
    for (auto &field : def.key_fields) {
        if (field.name == name) {
            return &field;
        }
    }

    for (auto &field : def.value_fields) {
        if (field.name == name) {
            return &field;
        }
    }

    return nullptr;
}

static void
add_ipfix_field(ViewDefinition &view_def, const fds_iemgr_elem *elem)
{
    ViewField field = {};

    switch (elem->data_type) {
    case FDS_ET_UNSIGNED_8:
        field.data_type = DataType::Unsigned8;
        field.size = sizeof(ViewValue::u8);
        break;
    case FDS_ET_UNSIGNED_16:
        field.data_type = DataType::Unsigned16;
        field.size = sizeof(ViewValue::u16);
        break;
    case FDS_ET_UNSIGNED_32:
        field.data_type = DataType::Unsigned32;
        field.size = sizeof(ViewValue::u32);
        break;
    case FDS_ET_UNSIGNED_64:
        field.data_type = DataType::Unsigned64;
        field.size = sizeof(ViewValue::u64);
        break;
    case FDS_ET_SIGNED_8:
        field.data_type = DataType::Signed8;
        field.size = sizeof(ViewValue::i8);
        break;
    case FDS_ET_SIGNED_16:
        field.data_type = DataType::Signed16;
        field.size = sizeof(ViewValue::i16);
        break;
    case FDS_ET_SIGNED_32:
        field.data_type = DataType::Signed32;
        field.size = sizeof(ViewValue::i32);
        break;
    case FDS_ET_SIGNED_64:
        field.data_type = DataType::Signed64;
        field.size = sizeof(ViewValue::i64);
        break;
    case FDS_ET_IPV4_ADDRESS:
        field.data_type = DataType::IPv4Address;
        field.size = sizeof(ViewValue::ipv4);
        break;
    case FDS_ET_IPV6_ADDRESS:
        field.data_type = DataType::IPv6Address;
        field.size = sizeof(ViewValue::ipv6);
        break;
    case FDS_ET_STRING:
        field.data_type = DataType::String128B;
        field.size = 128;
        break;
    case FDS_ET_DATE_TIME_MILLISECONDS:
    case FDS_ET_DATE_TIME_MICROSECONDS:
    case FDS_ET_DATE_TIME_NANOSECONDS:
    case FDS_ET_DATE_TIME_SECONDS:
        field.data_type = DataType::DateTime;
        field.size = sizeof(ViewValue::ts_millisecs);
        break;
    case FDS_ET_MAC_ADDRESS:
        field.data_type = DataType::MacAddress;
        field.size = sizeof(ViewValue::mac);
        break;
    default:
        throw ArgError("Invalid aggregation key \"" + std::string(elem->name) + "\" - data type not supported");
    }

    field.kind = ViewFieldKind::VerbatimKey;
    field.pen = elem->scope->pen;
    field.id = elem->id;
    field.name = elem->name;
    field.offset = view_def.keys_size;
    view_def.keys_size += field.size;
    view_def.key_fields.push_back(field);
}

static void
configure_keys(const std::string &options, ViewDefinition &view_def, fds_iemgr_t *iemgr)
{
    //NOTE: This isn't perfect and there is some unnecessary repetition and certain parts could be split into seperate functions,
    //      but it's still a work in progress and it's simple and flexible

    for (const auto &key : string_split(options, ",")) {
        ViewField field = {};

        //std::regex subnet_regex{"([a-zA-Z0-9:]+)/(\\d+)"};
        //std::smatch m;
        std::vector<std::string> pieces = string_split_right(key, "/", 2);

        if (pieces.size() == 2) {
            field.name = key;
            auto elem_name = pieces[0];
            auto prefix_length = std::stoul(pieces[1]);

            if (elem_name == "srcipv4") {
                field.pen = IPFIX::iana;
                field.id = IPFIX::sourceIPv4Address;
                field.data_type = DataType::IPv4Address;
                field.size = sizeof(ViewValue::ipv4);
                field.kind = ViewFieldKind::IPv4SubnetKey;

            } else if (elem_name == "dstipv4") {
                field.pen = IPFIX::iana;
                field.id = IPFIX::destinationIPv4Address;
                field.data_type = DataType::IPv4Address;
                field.size = sizeof(ViewValue::ipv4);
                field.kind = ViewFieldKind::IPv4SubnetKey;

            } else if (elem_name == "srcipv6") {
                field.pen = IPFIX::iana;
                field.id = IPFIX::sourceIPv6Address;
                field.data_type = DataType::IPv6Address;
                field.size = sizeof(ViewValue::ipv6);
                field.kind = ViewFieldKind::IPv6SubnetKey;

            } else if (elem_name == "dstipv6") {
                field.pen = IPFIX::iana;
                field.id = IPFIX::destinationIPv6Address;
                field.data_type = DataType::IPv6Address;
                field.size = sizeof(ViewValue::ipv6);
                field.kind = ViewFieldKind::IPv6SubnetKey;

            } else if (elem_name == "ipv4") {
                field.data_type = DataType::IPv4Address;
                field.size = sizeof(ViewValue::ipv4);
                field.kind = ViewFieldKind::BidirectionalIPv4SubnetKey;
                view_def.bidirectional = true;

            } else if (elem_name == "ipv6") {
                field.data_type = DataType::IPv6Address;
                field.size = sizeof(ViewValue::ipv6);
                field.kind = ViewFieldKind::BidirectionalIPv6SubnetKey;
                view_def.bidirectional = true;

            } else {
                const fds_iemgr_elem *elem = fds_iemgr_elem_find_name(iemgr, elem_name.c_str());

                if (!elem) {
                    throw ArgError("Invalid aggregation key \"" + key + "\" - element not found");
                }

                if (elem->data_type != FDS_ET_IPV4_ADDRESS && elem->data_type != FDS_ET_IPV6_ADDRESS) {
                    throw ArgError("Invalid aggregation key \"" + key + "\" - not an IP address but subnet is specified");
                }

                field.pen = elem->scope->pen;
                field.id = elem->id;
                if (elem->data_type == FDS_ET_IPV4_ADDRESS) {
                    field.data_type = DataType::IPv4Address;
                    field.size = sizeof(ViewValue::ipv4);
                    field.kind = ViewFieldKind::IPv4SubnetKey;
                } else if (elem->data_type == FDS_ET_IPV6_ADDRESS) {
                    field.data_type = DataType::IPv6Address;
                    field.size = sizeof(ViewValue::ipv6);
                    field.kind = ViewFieldKind::IPv6SubnetKey;
                }
            }

            if (field.data_type == DataType::IPv4Address && (prefix_length <= 0 || prefix_length > 32)) {
                throw ArgError("Invalid aggregation key \"" + key + "\" - invalid prefix length " + std::to_string(prefix_length) + " for IPv4 address");
            } else if (field.data_type == DataType::IPv6Address && (prefix_length <= 0 || prefix_length > 128)) {
                throw ArgError("Invalid aggregation key \"" + key + "\" - invalid prefix length " + std::to_string(prefix_length) + " for IPv6 address");
            }

            field.extra.prefix_length = prefix_length;
            field.offset = view_def.keys_size;
            view_def.keys_size += field.size;
            view_def.key_fields.push_back(field);

        } else if (key == "srcip") {
            field.data_type = DataType::IPAddress;
            field.kind = ViewFieldKind::SourceIPAddressKey;
            field.name = "srcip";
            field.size = sizeof(ViewValue::ip);
            field.offset = view_def.keys_size;
            view_def.keys_size += sizeof(ViewValue::ip);
            view_def.key_fields.push_back(field);

        } else if (key == "dstip") {
            field.data_type = DataType::IPAddress;
            field.kind = ViewFieldKind::DestinationIPAddressKey;
            field.name = "dstip";
            field.size = sizeof(ViewValue::ip);
            field.offset = view_def.keys_size;
            view_def.keys_size += sizeof(ViewValue::ip);
            view_def.key_fields.push_back(field);

        } else if (key == "srcport") {
            field.data_type = DataType::Unsigned16;
            field.kind = ViewFieldKind::VerbatimKey;
            field.pen = IPFIX::iana;
            field.id = IPFIX::sourceTransportPort;
            field.name = "srcport";
            field.size = sizeof(ViewValue::u16);
            field.offset = view_def.keys_size;
            view_def.keys_size += sizeof(ViewValue::u16);
            view_def.key_fields.push_back(field);

        } else if (key == "dstport") {
            field.data_type = DataType::Unsigned16;
            field.kind = ViewFieldKind::VerbatimKey;
            field.pen = IPFIX::iana;
            field.id = IPFIX::destinationTransportPort;
            field.name = "dstport";
            field.size = sizeof(ViewValue::u16);
            field.offset = view_def.keys_size;
            view_def.keys_size += sizeof(ViewValue::u16);
            view_def.key_fields.push_back(field);

        } else if (key == "proto") {
            field.data_type = DataType::Unsigned8;
            field.kind = ViewFieldKind::VerbatimKey;
            field.pen = IPFIX::iana;
            field.id = IPFIX::protocolIdentifier;
            field.name = "proto";
            field.size = sizeof(ViewValue::u8);
            field.offset = view_def.keys_size;
            view_def.keys_size += sizeof(ViewValue::u8);
            view_def.key_fields.push_back(field);

        } else if (key == "ip") {
            field.data_type = DataType::IPAddress;
            field.kind = ViewFieldKind::BidirectionalIPAddressKey;
            field.name = "ip";
            field.size = sizeof(ViewValue::ip);
            field.offset = view_def.keys_size;
            view_def.keys_size += sizeof(ViewValue::ip);
            view_def.bidirectional = true;
            view_def.key_fields.push_back(field);

        } else if (key == "port") {
            field.data_type = DataType::Unsigned16;
            field.kind = ViewFieldKind::BidirectionalPortKey;
            field.name = "port";
            field.size = sizeof(ViewValue::u16);
            field.offset = view_def.keys_size;
            view_def.keys_size += sizeof(ViewValue::u16);
            view_def.bidirectional = true;
            view_def.key_fields.push_back(field);

        } else if (key == "biflowdir") {
            field.data_type = DataType::Unsigned8;
            field.kind = ViewFieldKind::BiflowDirectionKey;
            field.name = "biflowdir";
            field.size = sizeof(ViewValue::u8);
            field.offset = view_def.keys_size;
            view_def.keys_size += sizeof(ViewValue::u8);
            view_def.biflow_enabled = true;
            view_def.key_fields.push_back(field);

        } else if (key == "srcipv4") {
            field.pen = IPFIX::iana;
            field.id = IPFIX::sourceIPv4Address;
            field.data_type = DataType::IPv4Address;
            field.name = "srcipv4";
            field.size = sizeof(ViewValue::ipv4);
            field.kind = ViewFieldKind::VerbatimKey;
            field.offset = view_def.keys_size;
            view_def.keys_size += sizeof(ViewValue::ipv4);
            view_def.key_fields.push_back(field);

        } else if (key == "dstipv4") {
            field.pen = IPFIX::iana;
            field.id = IPFIX::destinationIPv4Address;
            field.data_type = DataType::IPv4Address;
            field.name = "dstipv4";
            field.size = sizeof(ViewValue::ipv4);
            field.kind = ViewFieldKind::VerbatimKey;
            field.offset = view_def.keys_size;
            view_def.keys_size += sizeof(ViewValue::ipv4);
            view_def.key_fields.push_back(field);

        } else if (key == "srcipv6") {
            field.pen = IPFIX::iana;
            field.id = IPFIX::sourceIPv6Address;
            field.data_type = DataType::IPv6Address;
            field.size = sizeof(ViewValue::ipv6);
            field.kind = ViewFieldKind::VerbatimKey;
            field.name = "srcipv6";
            field.offset = view_def.keys_size;
            view_def.keys_size += sizeof(ViewValue::ipv6);
            view_def.key_fields.push_back(field);

        } else if (key == "dstipv6") {
            field.pen = IPFIX::iana;
            field.id = IPFIX::destinationIPv6Address;
            field.data_type = DataType::IPv6Address;
            field.name = "dstipv6";
            field.size = sizeof(ViewValue::ipv6);
            field.kind = ViewFieldKind::VerbatimKey;
            field.offset = view_def.keys_size;
            view_def.keys_size += sizeof(ViewValue::ipv6);
            view_def.key_fields.push_back(field);

        } else if (key == "ipv4") {
            field.data_type = DataType::IPv4Address;
            field.size = sizeof(ViewValue::ipv4);
            field.kind = ViewFieldKind::BidirectionalIPv4SubnetKey;
            field.name = "ipv4";
            view_def.bidirectional = true;
            field.offset = view_def.keys_size;
            view_def.keys_size += sizeof(ViewValue::ipv4);
            view_def.key_fields.push_back(field);

        } else if (key == "ipv6") {
            field.data_type = DataType::IPv6Address;
            field.size = sizeof(ViewValue::ipv6);
            field.kind = ViewFieldKind::BidirectionalIPv6SubnetKey;
            field.name = "ipv6";
            view_def.bidirectional = true;
            field.offset = view_def.keys_size;
            view_def.keys_size += sizeof(ViewValue::ipv6);
            view_def.key_fields.push_back(field);

        } else {
            const fds_iemgr_elem *elem = fds_iemgr_elem_find_name(iemgr, key.c_str());
            if (!elem) {
                throw ArgError("Invalid aggregation key \"" + key + "\" - element not found");
            }

            add_ipfix_field(view_def, elem);
        }


    }
}

static void
configure_values(const std::string &options, ViewDefinition &view_def, fds_iemgr_t *iemgr)
{
    //NOTE: This isn't perfect and there is some unnecessary repetition and certain parts could be split into seperate functions,
    //      but it's still a work in progress and it's simple and flexible

    auto values = string_split(options, ",");

    for (const auto &value : values) {
        ViewField field = {};

        //std::regex aggregate_regex{"(.+)\\((.+)\\)"};
        //std::regex aggregate_regex{"(.+):(min|max|sum)"};
        //std::smatch m;
        std::vector<std::string> pieces = string_split_right(value, ":", 2);

        if (pieces.size() == 2 && (pieces[1] == "min" || pieces[1] == "max" || pieces[1] == "sum")) {
            //const std::string &func = m[1].str();
            //const std::string &field_name = m[2].str();
            const std::string &field_name = pieces[0];
            const std::string &func = pieces[1];

            if (func == "min") {
                field.kind = ViewFieldKind::MinAggregate;
            } else if (func == "max") {
                field.kind = ViewFieldKind::MaxAggregate;
            } else if (func == "sum") {
                field.kind = ViewFieldKind::SumAggregate;
            } else {
                throw ArgError("Invalid aggregation value \"" + value + "\" - invalid aggregation function");
            }

            const fds_iemgr_elem *elem = fds_iemgr_elem_find_name(iemgr, field_name.c_str());

            if (!elem) {
                throw ArgError("Invalid aggregation value \"" + value + "\" - element not found");
            }

            field.pen = elem->scope->pen;
            field.id = elem->id;
            field.name = value;

            if (field.kind == ViewFieldKind::MinAggregate || field.kind == ViewFieldKind::MaxAggregate) {
                switch (elem->data_type) {
                case FDS_ET_UNSIGNED_8:
                    field.data_type = DataType::Unsigned8;
                    field.size = sizeof(ViewValue::u8);
                    break;
                case FDS_ET_UNSIGNED_16:
                    field.data_type = DataType::Unsigned16;
                    field.size = sizeof(ViewValue::u16);
                    break;
                case FDS_ET_UNSIGNED_32:
                    field.data_type = DataType::Unsigned32;
                    field.size = sizeof(ViewValue::u32);
                    break;
                case FDS_ET_UNSIGNED_64:
                    field.data_type = DataType::Unsigned64;
                    field.size = sizeof(ViewValue::u64);
                    break;
                case FDS_ET_SIGNED_8:
                    field.data_type = DataType::Signed8;
                    field.size = sizeof(ViewValue::i8);
                    break;
                case FDS_ET_SIGNED_16:
                    field.data_type = DataType::Signed16;
                    field.size = sizeof(ViewValue::i16);
                    break;
                case FDS_ET_SIGNED_32:
                    field.data_type = DataType::Signed32;
                    field.size = sizeof(ViewValue::i32);
                    break;
                case FDS_ET_SIGNED_64:
                    field.data_type = DataType::Signed64;
                    field.size = sizeof(ViewValue::i64);
                    break;
                case FDS_ET_DATE_TIME_MILLISECONDS:
                case FDS_ET_DATE_TIME_MICROSECONDS:
                case FDS_ET_DATE_TIME_NANOSECONDS:
                case FDS_ET_DATE_TIME_SECONDS:
                    field.data_type = DataType::DateTime;
                    field.size = sizeof(ViewValue::ts_millisecs);
                    break;
                default:
                    throw ArgError("Invalid aggregation value \"" + value + "\" - data type not supported for selected aggregation");
                }

            } else if (field.kind == ViewFieldKind::SumAggregate) {
                switch (elem->data_type) {
                case FDS_ET_UNSIGNED_8:
                case FDS_ET_UNSIGNED_16:
                case FDS_ET_UNSIGNED_32:
                case FDS_ET_UNSIGNED_64:
                    field.data_type = DataType::Unsigned64;
                    field.size = sizeof(ViewValue::u64);
                    break;
                case FDS_ET_SIGNED_8:
                case FDS_ET_SIGNED_16:
                case FDS_ET_SIGNED_32:
                case FDS_ET_SIGNED_64:
                    field.data_type = DataType::Signed64;
                    field.size = sizeof(ViewValue::i64);
                    break;
                default:
                    throw ArgError("Invalid aggregation value \"" + value + "\" - data type not supported for selected aggregation");
                }

            } else {
                assert(0);
            }

            field.offset = view_def.keys_size + view_def.values_size;
            view_def.values_size += sizeof(field.size);

        } else if (value == "packets") {
            field.data_type = DataType::Unsigned64;
            field.pen = IPFIX::iana;
            field.id = IPFIX::packetDeltaCount;
            field.kind = ViewFieldKind::SumAggregate;
            field.name = "packets";
            field.size = sizeof(ViewValue::u64);
            field.offset = view_def.keys_size + view_def.values_size;
            view_def.values_size += sizeof(ViewValue::u64);

        } else if (value == "bytes") {
            field.data_type = DataType::Unsigned64;
            field.pen = IPFIX::iana;
            field.id = IPFIX::octetDeltaCount;
            field.kind = ViewFieldKind::SumAggregate;
            field.name = "bytes";
            field.size = sizeof(ViewValue::u64);
            field.offset = view_def.keys_size + view_def.values_size;
            view_def.values_size += sizeof(ViewValue::u64);

        } else if (value == "flows") {
            field.data_type = DataType::Unsigned64;
            field.kind = ViewFieldKind::CountAggregate;
            field.name = "flows";
            field.size = sizeof(ViewValue::u64);
            field.offset = view_def.keys_size + view_def.values_size;
            view_def.values_size += sizeof(ViewValue::u64);

        } else if (value == "inpackets") {
            field.data_type = DataType::Unsigned64;
            field.pen = IPFIX::iana;
            field.id = IPFIX::packetDeltaCount;
            field.kind = ViewFieldKind::SumAggregate;
            field.name = "inpackets";
            field.size = sizeof(ViewValue::u64);
            field.direction = Direction::In;
            field.offset = view_def.keys_size + view_def.values_size;
            view_def.values_size += sizeof(ViewValue::u64);

        } else if (value == "inbytes") {
            field.data_type = DataType::Unsigned64;
            field.pen = IPFIX::iana;
            field.id = IPFIX::octetDeltaCount;
            field.kind = ViewFieldKind::SumAggregate;
            field.name = "inbytes";
            field.size = sizeof(ViewValue::u64);
            field.direction = Direction::In;
            field.offset = view_def.keys_size + view_def.values_size;
            view_def.values_size += sizeof(ViewValue::u64);

        } else if (value == "inflows") {
            field.data_type = DataType::Unsigned64;
            field.kind = ViewFieldKind::CountAggregate;
            field.name = "inflows";
            field.size = sizeof(ViewValue::u64);
            field.direction = Direction::In;
            field.offset = view_def.keys_size + view_def.values_size;
            view_def.values_size += sizeof(ViewValue::u64);

        } else if (value == "outpackets") {
            field.data_type = DataType::Unsigned64;
            field.pen = IPFIX::iana;
            field.id = IPFIX::packetDeltaCount;
            field.kind = ViewFieldKind::SumAggregate;
            field.name = "outpackets";
            field.size = sizeof(ViewValue::u64);
            field.direction = Direction::Out;
            field.offset = view_def.keys_size + view_def.values_size;
            view_def.values_size += sizeof(ViewValue::u64);

        } else if (value == "outbytes") {
            field.data_type = DataType::Unsigned64;
            field.pen = IPFIX::iana;
            field.id = IPFIX::octetDeltaCount;
            field.kind = ViewFieldKind::SumAggregate;
            field.name = "outbytes";
            field.size = sizeof(ViewValue::u64);
            field.direction = Direction::Out;
            field.offset = view_def.keys_size + view_def.values_size;
            view_def.values_size += sizeof(ViewValue::u64);

        } else if (value == "outflows") {
            field.data_type = DataType::Unsigned64;
            field.kind = ViewFieldKind::CountAggregate;
            field.name = "outflows";
            field.size = sizeof(ViewValue::u64);
            field.direction = Direction::Out;
            field.offset = view_def.keys_size + view_def.values_size;
            view_def.values_size += sizeof(ViewValue::u64);

        } else {
            throw ArgError("Invalid aggregation value \"" + value + "\"");
        }
        view_def.value_fields.push_back(field);
    }
}

ViewDefinition
make_view_def(const std::string &keys, const std::string &values, fds_iemgr_t *iemgr)
{
    ViewDefinition def = {};

    configure_keys(keys, def, iemgr);
    configure_values(values, def, iemgr);

    return def;
}

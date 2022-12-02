/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief IPFIX field
 *
 * Copyright: (C) 2024 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <aggregator/ipfixField.hpp>
#include <common/fieldView.hpp>

#include <cstring>

namespace fdsdump {
namespace aggregator {

IpfixField::IpfixField(const fds_iemgr_elem &elem)
{
    m_pen = elem.scope->pen;
    m_id = elem.id;

    DataType data_type = DataType::Unassigned;
    switch (elem.data_type) {
    case FDS_ET_UNSIGNED_8:
        data_type = DataType::Unsigned8;
        break;
    case FDS_ET_UNSIGNED_16:
        data_type = DataType::Unsigned16;
        break;
    case FDS_ET_UNSIGNED_32:
        data_type = DataType::Unsigned32;
        break;
    case FDS_ET_UNSIGNED_64:
        data_type = DataType::Unsigned64;
        break;
    case FDS_ET_SIGNED_8:
        data_type = DataType::Signed8;
        break;
    case FDS_ET_SIGNED_16:
        data_type = DataType::Signed16;
        break;
    case FDS_ET_SIGNED_32:
        data_type = DataType::Signed32;
        break;
    case FDS_ET_SIGNED_64:
        data_type = DataType::Signed64;
        break;
    case FDS_ET_IPV4_ADDRESS:
        data_type = DataType::IPv4Address;
        break;
    case FDS_ET_IPV6_ADDRESS:
        data_type = DataType::IPv6Address;
        break;
    case FDS_ET_STRING:
        data_type = DataType::String128B;
        break;
    case FDS_ET_OCTET_ARRAY:
        data_type = DataType::Octets128B;
        break;
    case FDS_ET_DATE_TIME_MILLISECONDS:
    case FDS_ET_DATE_TIME_MICROSECONDS:
    case FDS_ET_DATE_TIME_NANOSECONDS:
    case FDS_ET_DATE_TIME_SECONDS:
        data_type = DataType::DateTime;
        break;
    case FDS_ET_MAC_ADDRESS:
        data_type = DataType::MacAddress;
        break;
    default:
        throw std::invalid_argument("unsupported IPFIX field data type");
    }

    set_data_type(data_type);

    set_name(elem.name);
}

bool
IpfixField::load(FlowContext &ctx, Value &value) const
{
    fds_drec_field drec_field;
    if (!ctx.find_field(m_pen, m_id, drec_field)) {
        return false;
    }

    switch (data_type()) {
    case DataType::Unsigned8:
        value.u8 = FieldView(drec_field).as_uint();
        break;
    case DataType::Unsigned16:
        value.u16 = FieldView(drec_field).as_uint();
        break;
    case DataType::Unsigned32:
        value.u32 = FieldView(drec_field).as_uint();
        break;
    case DataType::Unsigned64:
        value.u64 = FieldView(drec_field).as_uint();
        break;
    case DataType::Signed8:
        value.i8 = FieldView(drec_field).as_int();
        break;
    case DataType::Signed16:
        value.i16 = FieldView(drec_field).as_int();
        break;
    case DataType::Signed32:
        value.i32 = FieldView(drec_field).as_int();
        break;
    case DataType::Signed64:
        value.i64 = FieldView(drec_field).as_int();
        break;
    case DataType::String128B:
    case DataType::Octets128B:
        memset(value.str, 0, 128);
        memcpy(value.str, drec_field.data, std::min<int>(drec_field.size, 128));
        break;
    case DataType::DateTime:
        value.ts_millisecs = FieldView(drec_field).as_datetime_ms();
        break;
    case DataType::IPv4Address:
        if (drec_field.size != sizeof(value.ipv4)) {
            throw std::runtime_error(
                "Unexpected IPFIX field size when attempting to read IPv4 address:"
                " expected " + std::to_string(sizeof(value.ipv4)) +
                " got: " + std::to_string(drec_field.size)
            );
        }
        memcpy(&value.ipv4, drec_field.data, sizeof(value.ipv4));
        break;
    case DataType::IPv6Address:
        if (drec_field.size != sizeof(value.ipv6)) {
            throw std::runtime_error(
                "Unexpected IPFIX field size when attempting to read IPv6 address:"
                " expected " + std::to_string(sizeof(value.ipv6)) +
                " got: " + std::to_string(drec_field.size)
            );
        }
        memcpy(&value.ipv6, drec_field.data, sizeof(value.ipv6));
        break;
    case DataType::MacAddress:
        if (drec_field.size != sizeof(value.mac)) {
            throw std::runtime_error(
                "Unexpected IPFIX field size when attempting to read MAC address:"
                " expected " + std::to_string(sizeof(value.mac)) +
                " got: " + std::to_string(drec_field.size)
            );
        }
        memcpy(value.mac, drec_field.data, sizeof(value.mac));
        break;
    default:
        throw std::logic_error("unexpected IPFIX field data type");
    }

    return true;
}

std::string
IpfixField::repr() const
{
    return std::string("IpfixField(") +
        + "name=" + name() + ", data_type=" + data_type_to_str(data_type())
        + ", size=" + std::to_string(size()) + ", offset=" + std::to_string(offset())
        + ", pen=" + std::to_string(m_pen) + ", id=" + std::to_string(m_id) + ")";
}

uint32_t
IpfixField::pen() const
{
    return m_pen;
}

uint16_t
IpfixField::id() const
{
    return m_id;
}

bool
IpfixField::operator==(const IpfixField &other) const
{
    return pen() == other.pen() && id() == other.id();
}

bool
IpfixField::operator==(const Field &other) const
{
    if (const auto *other_ipfix = dynamic_cast<const IpfixField *>(&other)) {
        return *this == *other_ipfix;
    }
    return false;
}

size_t
IpfixField::size(const Value* value) const
{
    if (data_type() == DataType::VarString && value != nullptr) {
        return sizeof(value->varstr.len) + value->varstr.len;
    } else {
        return Field::size(value);
    }
}

} // aggregator
} // fdsdump

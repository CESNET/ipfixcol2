/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Aggregator field value representation
 *
 * Copyright: (C) 2024 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <aggregator/value.hpp>

#include <cassert>
#include <stdexcept>

namespace fdsdump {
namespace aggregator {

std::string
data_type_to_str(DataType data_type)
{
    switch (data_type) {
    case DataType::Unassigned:
        return "Unassigned";
    case DataType::IPAddress:
        return "IPAddress";
    case DataType::IPv4Address:
        return "IPv4Address";
    case DataType::IPv6Address:
        return "IPv6Address";
    case DataType::MacAddress:
        return "MacAddress";
    case DataType::Unsigned8:
        return "Unsigned8";
    case DataType::Signed8:
        return "Signed8";
    case DataType::Unsigned16:
        return "Unsigned16";
    case DataType::Signed16:
        return "Signed16";
    case DataType::Unsigned32:
        return "Unsigned32";
    case DataType::Signed32:
        return "Signed32";
    case DataType::Unsigned64:
        return "Unsigned64";
    case DataType::Signed64:
        return "Signed64";
    case DataType::DateTime:
        return "DateTime";
    case DataType::String128B:
        return "String128B";
    case DataType::Octets128B:
        return "Octets128B";
    case DataType::VarString:
        return "VarString";
    }
    assert(0);
    return "<unknown>";
}

ValueView::ValueView(DataType data_type, Value &value) :
    m_data_type(data_type),
    m_value(value)
{
}

uint8_t
ValueView::as_u8() const
{
    switch (m_data_type) {
    case DataType::Unsigned8:
        return m_value.u8;
    default:
        throw std::runtime_error("Cannot view value as u8: incompatible data type");
    }
}

uint16_t
ValueView::as_u16() const
{
    switch (m_data_type) {
    case DataType::Unsigned8:
        return m_value.u8;
    case DataType::Unsigned16:
        return m_value.u16;
    default:
        throw std::runtime_error("Cannot view value as u16: incompatible data type");
    }
}

uint32_t
ValueView::as_u32() const
{
    switch (m_data_type) {
    case DataType::Unsigned8:
        return m_value.u8;
    case DataType::Unsigned16:
        return m_value.u16;
    case DataType::Unsigned32:
        return m_value.u32;
    default:
        throw std::runtime_error("Cannot view value as u32: incompatible data type");
    }
}

uint64_t
ValueView::as_u64() const
{
    switch (m_data_type) {
    case DataType::Unsigned8:
        return m_value.u8;
    case DataType::Unsigned16:
        return m_value.u16;
    case DataType::Unsigned32:
        return m_value.u32;
    case DataType::Unsigned64:
        return m_value.u64;
    default:
        throw std::runtime_error("Cannot view value as u64: incompatible data type");
    }
}

int8_t
ValueView::as_i8() const
{
    switch (m_data_type) {
    case DataType::Signed8:
        return m_value.i8;
    default:
        throw std::runtime_error("Cannot view value as i8: incompatible data type");
    }
}

int16_t
ValueView::as_i16() const
{
    switch (m_data_type) {
    case DataType::Signed8:
        return m_value.i8;
    case DataType::Signed16:
        return m_value.i16;
    default:
        throw std::runtime_error("Cannot view value as i16: incompatible data type");
    }
}

int32_t
ValueView::as_i32() const
{
    switch (m_data_type) {
    case DataType::Signed8:
        return m_value.i8;
    case DataType::Signed16:
        return m_value.i16;
    case DataType::Signed32:
        return m_value.i32;
    default:
        throw std::runtime_error("Cannot view value as i32: incompatible data type");
    }
}

int64_t
ValueView::as_i64() const
{
    switch (m_data_type) {
    case DataType::Signed8:
        return m_value.i8;
    case DataType::Signed16:
        return m_value.i16;
    case DataType::Signed32:
        return m_value.i32;
    case DataType::Signed64:
        return m_value.i64;
    default:
        throw std::runtime_error("Cannot view value as i64: incompatible data type");
    }
}

IPAddr
ValueView::as_ip()
{
    switch (m_data_type) {
    case DataType::IPv4Address:
        return IPAddr::ip4(m_value.ipv4);
    case DataType::IPv6Address:
        return IPAddr::ip6(m_value.ipv6);
    case DataType::IPAddress:
        return m_value.ip;
    default:
        throw std::runtime_error("Cannot view value as ip: incompatible data type");
    }
}

} // namespace aggregator
} // namespace fdsdump


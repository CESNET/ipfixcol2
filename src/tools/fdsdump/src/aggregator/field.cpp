/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Field base
 *
 * Copyright: (C) 2024 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <aggregator/field.hpp>

#include <libfds.h>

#include <stdexcept>

namespace fdsdump {
namespace aggregator {

void
Field::set_data_type(DataType data_type)
{
    m_data_type = data_type;

    switch (data_type) {
    case DataType::IPv4Address:
        m_size = sizeof(Value::ipv4);
        break;
    case DataType::IPv6Address:
        m_size = sizeof(Value::ipv6);
        break;
    case DataType::IPAddress:
        m_size = sizeof(Value::ip);
        break;
    case DataType::MacAddress:
        m_size = sizeof(Value::mac);
        break;
    case DataType::Signed8:
        m_size = sizeof(Value::i8);
        break;
    case DataType::Signed16:
        m_size = sizeof(Value::i16);
        break;
    case DataType::Signed32:
        m_size = sizeof(Value::i32);
        break;
    case DataType::Signed64:
        m_size = sizeof(Value::i64);
        break;
    case DataType::Unsigned8:
        m_size = sizeof(Value::u8);
        break;
    case DataType::Unsigned16:
        m_size = sizeof(Value::u16);
        break;
    case DataType::Unsigned32:
        m_size = sizeof(Value::u32);
        break;
    case DataType::Unsigned64:
        m_size = sizeof(Value::u64);
        break;
    case DataType::DateTime:
        m_size = sizeof(Value::ts_millisecs);
        break;
    case DataType::String128B:
        m_size = sizeof(Value::str);
        break;
    case DataType::Octets128B:
        m_size = sizeof(Value::str);
        break;
    case DataType::VarString:
        m_size = 0;
        break;
    case DataType::Unassigned:
        throw std::logic_error("unexpected field data type");
    }
}

void
Field::set_offset(size_t offset)
{
    m_offset = offset;
}

void
Field::set_name(std::string name)
{
    m_name = name;
}

bool
Field::load(FlowContext &ctx, Value &value) const
{
    (void) ctx;
    (void) value;
    throw std::logic_error("unimplemented load for field " + name());
}

void
Field::init(Value &value) const
{
    (void) value;
    throw std::logic_error("unimplemented init for field " + name());
}

bool
Field::aggregate(FlowContext &ctx, Value &aggregated_value) const
{
    (void) ctx;
    (void) aggregated_value;
    throw std::logic_error("unimplemented aggregate for field " + name());
}

void
Field::merge(Value &value, const Value &other) const
{
    (void) value;
    (void) other;
    throw std::logic_error("unimplemented merge for field " + name());
}

template <typename T>
static CmpResult
cmp(T a, T b)
{
    if (a < b) {
        return CmpResult::Lt;
    } else if (a > b) {
        return CmpResult::Gt;
    } else {
        return CmpResult::Eq;
    }
}

static CmpResult
cmp(const uint8_t *a, const uint8_t *b, size_t count)
{
    int result = std::memcmp(a, b, count);
    if (result < 0) {
        return CmpResult::Lt;
    } else if (result > 0) {
        return CmpResult::Gt;
    } else {
        return CmpResult::Eq;
    }
}

CmpResult
Field::compare(const Value &a, const Value &b) const
{
    switch (data_type()) {
    case DataType::Unsigned8:
        return cmp(a.u8, b.u8);
    case DataType::Signed8:
        return cmp(a.i8, b.i8);
    case DataType::Unsigned16:
        return cmp(a.u16, b.u16);
    case DataType::Signed16:
        return cmp(a.i16, b.i16);
    case DataType::Unsigned32:
        return cmp(a.u32, b.u32);
    case DataType::Signed32:
        return cmp(a.i32, b.i32);
    case DataType::Unsigned64:
        return cmp(a.u64, b.u64);
    case DataType::Signed64:
        return cmp(a.i64, b.i64);
    case DataType::DateTime:
        return cmp(a.ts_millisecs, b.ts_millisecs);
    case DataType::IPv4Address:
        return cmp(a.ipv4, b.ipv4, sizeof(a.ipv4));
    case DataType::IPv6Address:
        return cmp(a.ipv6, b.ipv6, sizeof(a.ipv6));
    case DataType::IPAddress:
        return cmp(a.ip, b.ip);
    case DataType::MacAddress:
        return cmp(a.mac, b.mac, sizeof(a.mac));
    case DataType::String128B:
    case DataType::Octets128B:
        return cmp(reinterpret_cast<const uint8_t *>(a.str), reinterpret_cast<const uint8_t *>(b.str), sizeof(a.str));
    case DataType::VarString:
        return cmp(a.varstr, b.varstr);
    case DataType::Unassigned:
        throw std::logic_error("cannot compare fields with unassigned data type");
    }

    __builtin_unreachable();
}

bool
Field::operator==(const std::unique_ptr<Field> &other) const
{
    return *this == *other.get();
}

bool
Field::operator!=(const Field &other) const
{
    return !(*this == other);
}

bool
Field::is_number() const
{
    switch (data_type()) {
    case DataType::Unsigned8:
    case DataType::Unsigned16:
    case DataType::Unsigned32:
    case DataType::Unsigned64:
    case DataType::Signed8:
    case DataType::Signed16:
    case DataType::Signed32:
    case DataType::Signed64:
        return true;
    default:
        return false;
    }
}

} // aggregator
} // fdsdump

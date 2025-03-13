/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Extra fields
 *
 * Copyright: (C) 2024 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <aggregator/extraFields.hpp>

namespace fdsdump {
namespace aggregator {

FlowCountField::FlowCountField()
{
    set_data_type(DataType::Unsigned64);
    set_name("flowcount");
}

bool
FlowCountField::load(FlowContext &ctx, Value &value) const
{
    (void) ctx;
    value.u64 = 1;
    return true;
}

bool
FlowCountField::operator==(const Field &other) const
{
    const auto *other_flowcount = dynamic_cast<const FlowCountField *>(&other);
    if (!other_flowcount) {
        return false;
    }
    return true;
}

std::string
FlowCountField::repr() const
{
    return std::string("FlowCountField(") +
        + "name=" + name() + ", data_type=" + data_type_to_str(data_type())
        + ", size= " + std::to_string(size()) + ", offset=" + std::to_string(offset())
        + ")";
}

DirectionField::DirectionField()
{
    set_data_type(DataType::Unsigned8);
    set_name("direction");
}

bool
DirectionField::load(FlowContext &ctx, Value &value) const
{
    value.u8 = (ctx.flow_dir == FlowDirection::Reverse ? REV_VALUE : FWD_VALUE);
    return true;
}

bool
DirectionField::operator==(const Field &other) const
{
    const auto *other_dirfield = dynamic_cast<const DirectionField *>(&other);
    if (!other_dirfield) {
        return false;
    }
    return true;
}

std::string
DirectionField::repr() const
{
    return std::string("DirectionField(") +
        + "name=" + name() + ", data_type=" + data_type_to_str(data_type())
        + ", size= " + std::to_string(size()) + ", offset=" + std::to_string(offset())
        + ")";
}

TimeWindowField::TimeWindowField(std::unique_ptr<Field> source_field, uint64_t window_millisec) :
    m_window_millisec(window_millisec)
{
    if (source_field->data_type() != DataType::DateTime) {
        throw std::invalid_argument("source field of timewindow field is not datetime");
    }

    if (window_millisec == 0) {
        throw std::invalid_argument("time window duration cannot be 0");
    }

    m_source_field = std::move(source_field);
    set_name("timewindow(" + m_source_field->name() + ", " + std::to_string(window_millisec) + "ms)");
    set_data_type(m_source_field->data_type());
}

bool
TimeWindowField::load(FlowContext &ctx, Value &value) const
{
    if (!m_source_field->load(ctx, value)) {
        return false;
    }

    value.ts_millisecs = (value.ts_millisecs / m_window_millisec) * m_window_millisec;
    return true;
}

bool
TimeWindowField::operator==(const Field &other) const
{
    const auto *other_timewindow = dynamic_cast<const TimeWindowField *>(&other);
    if (!other_timewindow) {
        return false;
    }

    return m_source_field == other_timewindow->m_source_field
        && m_window_millisec == other_timewindow->m_window_millisec;
}

std::string
TimeWindowField::repr() const
{
    return std::string("TimeWindowField(") +
        + "name=" + name() + ", data_type=" + data_type_to_str(data_type())
        + ", size= " + std::to_string(size()) + ", offset=" + std::to_string(offset())
        + ", source=" + m_source_field->repr() + ", window_millisec=" + std::to_string(m_window_millisec)
        + ")";
}

SubnetField::SubnetField(std::unique_ptr<Field> source_field, uint8_t prefix_len)
{
    uint8_t addr_bytes;
    if (source_field->data_type() == DataType::IPv4Address) {
        addr_bytes = 4;
    } else if (source_field->data_type() == DataType::IPv6Address) {
        addr_bytes = 16;
    } else {
        throw std::invalid_argument("invalid data type of source field"); //FIXME
    }

    if (prefix_len > addr_bytes * 8) {
        throw std::invalid_argument("invalid prefix len"); //FIXME
    }

    m_source_field = std::move(source_field);
    set_data_type(m_source_field->data_type());
    set_name(m_source_field->name() + "/" + std::to_string(prefix_len));

    m_startbyte = prefix_len / 8;
    m_zerobytes = addr_bytes - m_startbyte;
    m_startmask = ~(uint8_t(0xFF) >> (prefix_len % 8));
    m_prefix_len = prefix_len;
}

bool
SubnetField::load(FlowContext &ctx, Value &value) const
{
    if (!m_source_field->load(ctx, value)) {
        return false;
    }

    if (m_zerobytes > 0) {
        // value.str to access raw bytes
        uint8_t tmp = value.str[m_startbyte] & m_startmask;
        memset(&value.str[m_startbyte], 0, m_zerobytes);
        value.str[m_startbyte] = tmp;
    }
    return true;
}

bool
SubnetField::operator==(const Field &other) const
{
    const auto *other_subnet = dynamic_cast<const SubnetField *>(&other);
    if (!other_subnet) {
        return false;
    }

    return m_source_field == other_subnet->m_source_field
        && m_prefix_len == other_subnet->m_prefix_len;
}

std::string
SubnetField::repr() const
{
    return std::string("SubnetField(") +
        + "name=" + name() + ", data_type=" + data_type_to_str(data_type())
        + ", size= " + std::to_string(size()) + ", offset=" + std::to_string(offset())
        + ", source=" + m_source_field->repr() + ", prefix_len=" + std::to_string(m_prefix_len)
        + ")";
}

} // aggregator
} // fdsdump

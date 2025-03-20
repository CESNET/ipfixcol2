/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief In/out fields
 *
 * Copyright: (C) 2024 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <aggregator/extraFields.hpp>
#include <aggregator/inOutField.hpp>
#include <aggregator/viewFactory.hpp>

namespace fdsdump {
namespace aggregator {

InOutKeyField::InOutKeyField(
    std::unique_ptr<Field> in_field, std::unique_ptr<Field> out_field) :
    m_in_field(std::move(in_field)),
    m_out_field(std::move(out_field))
{
    if (m_in_field->data_type() != m_out_field->data_type()) {
        throw std::invalid_argument("in and out field not of same type");
    }

    set_data_type(m_in_field->data_type());
    set_name(m_in_field->name());
}

bool
InOutKeyField::load(FlowContext &ctx, Value &value) const
{
    switch (ctx.view_dir) {
    case ViewDirection::In:
        return m_in_field->load(ctx, value);
    case ViewDirection::Out:
        return m_out_field->load(ctx, value);
    case ViewDirection::None:
        assert(0);
    }
    return false;
}

std::string
InOutKeyField::repr() const
{
    return std::string("InOutKeyField(") +
        + "name=" + name() + ", data_type=" + data_type_to_str(data_type())
        + ", size=" + std::to_string(size()) + ", offset=" + std::to_string(offset())
        + ", in=" + m_in_field->repr() + ", out=" + m_out_field->repr() + ")";
}

bool
InOutKeyField::operator==(const InOutKeyField &other) const
{
    return *m_in_field.get() == *other.m_in_field.get()
        && *m_out_field.get() == *other.m_out_field.get();
}

bool
InOutKeyField::operator==(const Field &other) const
{
    if (const auto *other_inout = dynamic_cast<const InOutKeyField *>(&other)) {
        return *this == *other_inout;
    }
    return false;
}

std::unique_ptr<Field>
InOutKeyField::create_from(const Field &field)
{
    std::unique_ptr<Field> new_field;

    if (field == ViewFactory::create_key_field("ip")) {
        new_field.reset(new InOutKeyField(
            ViewFactory::create_key_field("dstip"),
            ViewFactory::create_key_field("srcip")));

    } else if (field == ViewFactory::create_key_field("ip4")) {
        new_field.reset(new InOutKeyField(
            ViewFactory::create_key_field("iana:destinationIPv4Address"),
            ViewFactory::create_key_field("iana:sourceIPv4Address")));

    } else if (field == ViewFactory::create_key_field("ip6")) {
        new_field.reset(new InOutKeyField(
            ViewFactory::create_key_field("iana:destinationIPv6Address"),
            ViewFactory::create_key_field("iana:sourceIPv6Address")));

    } else if (field == ViewFactory::create_key_field("port")) {
        new_field.reset(new InOutKeyField(
            ViewFactory::create_key_field("dstport"),
            ViewFactory::create_key_field("srcport")));

    } else if (const auto *subnet_field = dynamic_cast<const SubnetField *>(&field)) {
        auto inner = InOutKeyField::create_from(*subnet_field->m_source_field.get());
        new_field.reset(new SubnetField(std::move(inner), subnet_field->m_prefix_len));
    }

    return new_field;
}


InOutValueField::InOutValueField(std::unique_ptr<Field> field, ViewDirection dir) :
    m_field(std::move(field)),
    m_dir(dir)
{
    assert(m_dir == ViewDirection::In || m_dir == ViewDirection::Out);
    set_data_type(m_field->data_type());
    set_name((m_dir == ViewDirection::In ? "in " : "out ") + m_field->name());
}

void
InOutValueField::init(Value &value) const
{
    m_field->init(value);
}

bool
InOutValueField::aggregate(FlowContext &ctx, Value &aggregated_value) const
{
    if (m_dir == ctx.view_dir) {
        return m_field->aggregate(ctx, aggregated_value);
    }
    return true;
}

void
InOutValueField::merge(Value &value, const Value &other) const
{
    m_field->merge(value, other);
}

std::string
InOutValueField::repr() const
{
    std::string dir;
    if (m_dir == ViewDirection::None) {
        dir = "None";
    } else if (m_dir == ViewDirection::In) {
        dir = "In";
    } else if (m_dir == ViewDirection::Out) {
        dir = "Out";
    }
    return std::string("InOutValueField(") +
        + "name=" + name() + ", data_type=" + data_type_to_str(data_type())
        + ", size=" + std::to_string(size()) + ", offset=" + std::to_string(offset())
        + ", dir=" + dir
        + ", field=" + m_field->repr() + ")";
}

bool
InOutValueField::operator==(const InOutValueField &other) const
{
    return *m_field.get() == *other.m_field.get();
}

bool
InOutValueField::operator==(const Field &other) const
{
    if (const auto *other_inout = dynamic_cast<const InOutValueField *>(&other)) {
        return *this == *other_inout;
    }
    return false;
}

} // aggregator
} // fdsdump

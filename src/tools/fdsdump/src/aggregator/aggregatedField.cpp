/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Aggregated field
 *
 * Copyright: (C) 2024 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <aggregator/aggregatedField.hpp>

#include <stdexcept>

namespace fdsdump {
namespace aggregator {

SumAggregatedField::SumAggregatedField(std::unique_ptr<Field> source_field) :
    m_source_field(std::move(source_field))
{
    switch (m_source_field->data_type()) {
    case DataType::Signed8:
    case DataType::Signed16:
    case DataType::Signed32:
    case DataType::Signed64:
        set_data_type(DataType::Signed64);
        break;
    case DataType::Unsigned8:
    case DataType::Unsigned16:
    case DataType::Unsigned32:
    case DataType::Unsigned64:
        set_data_type(DataType::Unsigned64);
        break;
    default:
        throw std::invalid_argument("invalid aggregate field");
    }
    set_name("sum(" + m_source_field->name() + ")");
}

void
SumAggregatedField::init(Value &value) const
{
    value.u64 = 0;
}

bool
SumAggregatedField::aggregate(FlowContext &ctx, Value &aggregated_value) const
{
    Value value;
    if (!m_source_field->load(ctx, value)) {
        return false;
    }

    switch (data_type()) {
    case DataType::Unsigned64:
        switch (m_source_field->data_type()) {
        case DataType::Unsigned8:
            aggregated_value.u64 += value.u8;
            break;
        case DataType::Unsigned16:
            aggregated_value.u64 += value.u16;
            break;
        case DataType::Unsigned32:
            aggregated_value.u64 += value.u32;
            break;
        case DataType::Unsigned64:
            aggregated_value.u64 += value.u64;
            break;
        default:
            assert(0);
        }
        break;
    case DataType::Signed64:
        switch (m_source_field->data_type()) {
        case DataType::Signed8:
            aggregated_value.i64 += value.i8;
            break;
        case DataType::Signed16:
            aggregated_value.i64 += value.i16;
            break;
        case DataType::Signed32:
            aggregated_value.i64 += value.i32;
            break;
        case DataType::Signed64:
            aggregated_value.i64 += value.i64;
            break;
        default:
            assert(0);
        }
        break;
    default:
        assert(0);
    }

    return true;
}

void
SumAggregatedField::merge(Value &value, const Value &other) const
{
    switch (data_type()) {
    case DataType::Unsigned64:
        value.u64 += other.u64;
        break;
    case DataType::Signed64:
        value.i64 += other.i64;
        break;
    default: assert(0);
    }
}

std::string
SumAggregatedField::repr() const
{
    return std::string("SumAggregatedField(") +
        + "name=" + name() + ", data_type=" + data_type_to_str(data_type())
        + ", size= " + std::to_string(size()) + ", offset=" + std::to_string(offset())
        + ", source=" + m_source_field->repr() + ")";
}

bool
SumAggregatedField::operator==(const Field &other) const
{
    const auto *other_aggregated = dynamic_cast<const SumAggregatedField *>(&other);
    if (!other_aggregated) {
        return false;
    }
    return m_source_field == other_aggregated->m_source_field;
}

MinAggregatedField::MinAggregatedField(std::unique_ptr<Field> source_field) :
    m_source_field(std::move(source_field))
{
    switch (m_source_field->data_type()) {
    case DataType::Signed8:
    case DataType::Signed16:
    case DataType::Signed32:
    case DataType::Signed64:
    case DataType::Unsigned8:
    case DataType::Unsigned16:
    case DataType::Unsigned32:
    case DataType::Unsigned64:
    case DataType::DateTime:
        set_data_type(m_source_field->data_type());
        break;
    default:
        throw std::invalid_argument("invalid aggregate field");
    }
    set_name("min(" + m_source_field->name() + ")");
}

void
MinAggregatedField::init(Value &value) const
{
    switch (data_type()) {
    case DataType::Unsigned8:
        value.u8 = UINT8_MAX;
        break;
    case DataType::Unsigned16:
        value.u16 = UINT16_MAX;
        break;
    case DataType::Unsigned32:
        value.u32 = UINT32_MAX;
        break;
    case DataType::Unsigned64:
        value.u64 = UINT64_MAX;
        break;
    case DataType::Signed8:
        value.i8 = INT8_MAX;
        break;
    case DataType::Signed16:
        value.i16 = INT16_MAX;
        break;
    case DataType::Signed32:
        value.i32 = INT32_MAX;
        break;
    case DataType::Signed64:
        value.i64 = INT64_MAX;
        break;
    case DataType::DateTime:
        value.ts_millisecs = UINT64_MAX;
        break;
    default:
        assert(0);
    }
}

bool
MinAggregatedField::aggregate(FlowContext &ctx, Value &aggregated_value) const
{
    Value value;
    if (!m_source_field->load(ctx, value)) {
        return false;
    }

    switch (data_type()) {
    case DataType::Unsigned8:
        aggregated_value.u8 = std::min<uint8_t>(aggregated_value.u8, value.u8);
        break;
    case DataType::Unsigned16:
        aggregated_value.u16 = std::min<uint16_t>(aggregated_value.u16, value.u16);
        break;
    case DataType::Unsigned32:
        aggregated_value.u32 = std::min<uint32_t>(aggregated_value.u32, value.u32);
        break;
    case DataType::Unsigned64:
        aggregated_value.u64 = std::min<uint64_t>(aggregated_value.u64, value.u64);
        break;
    case DataType::Signed8:
        aggregated_value.i8 = std::min<int8_t>(aggregated_value.i8, value.i8);
        break;
    case DataType::Signed16:
        aggregated_value.i16 = std::min<int16_t>(aggregated_value.i16, value.i16);
        break;
    case DataType::Signed32:
        aggregated_value.i32 = std::min<int32_t>(aggregated_value.i32, value.i32);
        break;
    case DataType::Signed64:
        aggregated_value.i64 = std::min<int64_t>(aggregated_value.i64, value.i64);
        break;
    case DataType::DateTime:
        aggregated_value.ts_millisecs = std::min<uint64_t>(aggregated_value.ts_millisecs, value.ts_millisecs);
        break;
    default: assert(0);
    }

    return true;
}

void
MinAggregatedField::merge(Value &value, const Value &other) const
{
    switch (data_type()) {
    case DataType::Unsigned8:
        value.u8 = std::min<uint8_t>(other.u8, value.u8);
        break;
    case DataType::Unsigned16:
        value.u16 = std::min<uint16_t>(other.u16, value.u16);
        break;
    case DataType::Unsigned32:
        value.u32 = std::min<uint32_t>(other.u32, value.u32);
        break;
    case DataType::Unsigned64:
        value.u64 = std::min<uint64_t>(other.u64, value.u64);
        break;
    case DataType::Signed8:
        value.i8 = std::min<int8_t>(other.i8, value.i8);
        break;
    case DataType::Signed16:
        value.i16 = std::min<int16_t>(other.i16, value.i16);
        break;
    case DataType::Signed32:
        value.i32 = std::min<int32_t>(other.i32, value.i32);
        break;
    case DataType::Signed64:
        value.i64 = std::min<int64_t>(other.i64, value.i64);
        break;
    case DataType::DateTime:
        value.ts_millisecs = std::min<uint64_t>(other.ts_millisecs, value.ts_millisecs);
        break;
    default:
        assert(0);
    }
}

std::string
MinAggregatedField::repr() const
{
    return std::string("MinAggregatedField(") +
        + "name=" + name() + ", data_type=" + data_type_to_str(data_type())
        + ", size= " + std::to_string(size()) + ", offset=" + std::to_string(offset())
        + ", source=" + m_source_field->repr() + ")";
}

bool
MinAggregatedField::operator==(const Field &other) const
{
    const auto *other_aggregated = dynamic_cast<const MinAggregatedField *>(&other);
    if (!other_aggregated) {
        return false;
    }
    return m_source_field == other_aggregated->m_source_field;
}

MaxAggregatedField::MaxAggregatedField(std::unique_ptr<Field> source_field) :
    m_source_field(std::move(source_field))
{
    switch (m_source_field->data_type()) {
    case DataType::Signed8:
    case DataType::Signed16:
    case DataType::Signed32:
    case DataType::Signed64:
    case DataType::Unsigned8:
    case DataType::Unsigned16:
    case DataType::Unsigned32:
    case DataType::Unsigned64:
    case DataType::DateTime:
        set_data_type(m_source_field->data_type());
        break;
    default:
        throw std::invalid_argument("invalid aggregate field");
    }
    set_name("max(" + m_source_field->name() + ")");
}

void
MaxAggregatedField::init(Value &value) const
{
    switch (data_type()) {
    case DataType::Unsigned8:
        value.u8 = 0;
        break;
    case DataType::Unsigned16:
        value.u16 = 0;
        break;
    case DataType::Unsigned32:
        value.u32 = 0;
        break;
    case DataType::Unsigned64:
        value.u64 = 0;
        break;
    case DataType::Signed8:
        value.i8 = INT8_MIN;
        break;
    case DataType::Signed16:
        value.i16 = INT16_MIN;
        break;
    case DataType::Signed32:
        value.i32 = INT32_MIN;
        break;
    case DataType::Signed64:
        value.i64 = INT64_MIN;
        break;
    case DataType::DateTime:
        value.ts_millisecs = 0;
        break;
    default:
        assert(0);
    }
}

bool
MaxAggregatedField::aggregate(FlowContext &ctx, Value &aggregated_value) const
{
    Value value;
    if (!m_source_field->load(ctx, value)) {
        return false;
    }

    switch (data_type()) {
    case DataType::Unsigned8:
        aggregated_value.u8 = std::max<uint8_t>(aggregated_value.u8, value.u8);
        break;
    case DataType::Unsigned16:
        aggregated_value.u16 = std::max<uint16_t>(aggregated_value.u16, value.u16);
        break;
    case DataType::Unsigned32:
        aggregated_value.u32 = std::max<uint32_t>(aggregated_value.u32, value.u32);
        break;
    case DataType::Unsigned64:
        aggregated_value.u64 = std::max<uint64_t>(aggregated_value.u64, value.u64);
        break;
    case DataType::Signed8:
        aggregated_value.i8 = std::max<int8_t>(aggregated_value.i8, value.i8);
        break;
    case DataType::Signed16:
        aggregated_value.i16 = std::max<int16_t>(aggregated_value.i16, value.i16);
        break;
    case DataType::Signed32:
        aggregated_value.i32 = std::max<int32_t>(aggregated_value.i32, value.i32);
        break;
    case DataType::Signed64:
        aggregated_value.i64 = std::max<int64_t>(aggregated_value.i64, value.i64);
        break;
    case DataType::DateTime:
        aggregated_value.ts_millisecs = std::max<uint64_t>(aggregated_value.ts_millisecs, value.ts_millisecs);
        break;
    default: assert(0);
    }

    return true;
}

void
MaxAggregatedField::merge(Value &value, const Value &other) const
{
    switch (data_type()) {
    case DataType::Unsigned8:
        value.u8 = std::max<uint8_t>(other.u8, value.u8);
        break;
    case DataType::Unsigned16:
        value.u16 = std::max<uint16_t>(other.u16, value.u16);
        break;
    case DataType::Unsigned32:
        value.u32 = std::max<uint32_t>(other.u32, value.u32);
        break;
    case DataType::Unsigned64:
        value.u64 = std::max<uint64_t>(other.u64, value.u64);
        break;
    case DataType::Signed8:
        value.i8 = std::max<int8_t>(other.i8, value.i8);
        break;
    case DataType::Signed16:
        value.i16 = std::max<int16_t>(other.i16, value.i16);
        break;
    case DataType::Signed32:
        value.i32 = std::max<int32_t>(other.i32, value.i32);
        break;
    case DataType::Signed64:
        value.i64 = std::max<int64_t>(other.i64, value.i64);
        break;
    case DataType::DateTime:
        value.ts_millisecs = std::max<uint64_t>(other.ts_millisecs, value.ts_millisecs);
        break;
    default:
        assert(0);
    }
}

std::string
MaxAggregatedField::repr() const
{
    return std::string("MaxAggregatedField(") +
        + "name=" + name() + ", data_type=" + data_type_to_str(data_type())
        + ", size= " + std::to_string(size()) + ", offset=" + std::to_string(offset())
        + ", source=" + m_source_field->repr() + ")";
}

bool
MaxAggregatedField::operator==(const Field &other) const
{
    const auto *other_aggregated = dynamic_cast<const MaxAggregatedField *>(&other);
    if (!other_aggregated) {
        return false;
    }
    return m_source_field == other_aggregated->m_source_field;
}

} // aggregator
} // fdsdump

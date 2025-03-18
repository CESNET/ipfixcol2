/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Threshold algorithm implementation
 *
 * Copyright: (C) 2024 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <aggregator/thresholdAlgorithm.hpp>
#include <aggregator/aggregatedField.hpp>
#include <common/logger.hpp>

namespace fdsdump {
namespace aggregator {


static void min_aggregate_value(DataType data_type, Value& value, const Value &other)
{
    switch (data_type) {
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

void max_aggregate_value(DataType data_type, Value &value, Value &other)
{
    switch (data_type) {
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

void sum_aggregate_value(DataType data_type, Value &value, const Value &other)
{
    switch (data_type) {
    case DataType::Unsigned64:
        value.u64 += other.u64;
        break;
    case DataType::Signed64:
        value.i64 += other.i64;
        break;
    default: assert(0);
    }
}

static std::vector<uint8_t> estabilish_threshold(std::vector<HashTable *> &tables, View &view, unsigned int row)
{
    std::vector<uint8_t> buffer;

    if (view.is_fixed_size()) {
        for (auto *table : tables) {
            if (table->items().size() <= row) {
                continue;
            }

            uint8_t *rec = table->items()[row];
            if (buffer.empty()) {
                std::size_t size = view.record_size(rec);
                buffer.resize(size);
                std::memcpy(&buffer[0], rec, size);
                continue;
            }

            for (const auto &order_field : view.order_fields()) {
                Value &a = view.access_field(*order_field.field, buffer.data());
                Value &b = view.access_field(*order_field.field, rec);

                switch (order_field.dir) {
                case View::OrderDirection::Ascending: {
                    if (order_field.field->is_of_type<MinAggregatedField>()) {
                        min_aggregate_value(order_field.field->data_type(), a, b);
                    } else if (order_field.field->is_of_type<MaxAggregatedField>()) {
                        min_aggregate_value(order_field.field->data_type(), a, b);
                    } else if (order_field.field->is_of_type<SumAggregatedField>()) {
                        min_aggregate_value(order_field.field->data_type(), a, b);
                    } else {
                        if (order_field.field->compare(a, b) == CmpResult::Gt) {
                            std::memcpy(&a, &b, order_field.field->size());
                        }
                    }
                } break;

                case View::OrderDirection::Descending: {
                    if (order_field.field->is_of_type<MinAggregatedField>()) {
                        max_aggregate_value(order_field.field->data_type(), a, b);
                    } else if (order_field.field->is_of_type<MaxAggregatedField>()) {
                        max_aggregate_value(order_field.field->data_type(), a, b);
                    } else if (order_field.field->is_of_type<SumAggregatedField>()) {
                        sum_aggregate_value(order_field.field->data_type(), a, b);
                    } else {
                        if (order_field.field->compare(a, b) == CmpResult::Lt) {
                            std::memcpy(&a, &b, order_field.field->size());
                        }
                    }
                } break;
                }
            }
        }
    } else {
        //FIXME
        throw std::runtime_error("not implemented");
    }

    return buffer;
}

ThresholdAlgorithm::ThresholdAlgorithm(std::vector<HashTable *> &tables, View &view, unsigned int top_count) :
    m_result_table(new HashTable(view)),
    m_tables(tables),
    m_view(view),
    m_top_count(top_count),
    m_min_queue(view.rec_orderer())
{}

void ThresholdAlgorithm::process_row()
{
    for (auto *table : m_tables) {
        if (m_row >= table->items().size()) {
            continue;
        }
        uint8_t *rec = table->items()[m_row];
        uint8_t *result_rec = nullptr;
        bool found = m_result_table->find_or_create(rec, result_rec);
        if (found) {
            continue;
        }
        // If not found - copy over
        // Key is already copied by find_or_create, so only value needs to be copied
        unsigned int key_size = m_view.key_size(rec);
        unsigned int value_size = m_view.value_size();
        std::memcpy(result_rec + key_size, rec + key_size, value_size);

        for (auto *other_table : m_tables) {
            if (table == other_table) {
                continue;
            }
            if (m_row >= other_table->items().size()) {
                continue;
            }
            uint8_t *other_rec = nullptr;
            bool found = other_table->find(rec, other_rec);
            if (!found) {
                continue;
            }
            for (auto x : m_view.iter_values(result_rec, other_rec)) {
                x.field.merge(x.value1, x.value2);
            }
        }
        m_min_queue.push(result_rec);
        if (m_min_queue.size() > m_top_count) {
            m_min_queue.pop();
        }
    }
    m_row++;
}

bool ThresholdAlgorithm::out_of_items()
{
    if (m_row >= m_max_row) {
        return true;
    }
    for (auto *table : m_tables) {
        if (table->items().size() > m_row) {
            return false;
        }
    }
    return true;
}

bool ThresholdAlgorithm::check_finish_condition()
{
    if (m_min_queue.size() < m_top_count) {
        return false;
    }
    m_threshold = estabilish_threshold(m_tables, m_view, m_row);

    if (m_view.compare(m_threshold.data(), m_min_queue.top()) == CmpResult::Lt) {
        return false;
    }
    return true;
}

} // aggregator
} // fdsdump

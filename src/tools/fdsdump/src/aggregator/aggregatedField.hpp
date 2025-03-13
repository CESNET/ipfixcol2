/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Aggregated field
 *
 * Copyright: (C) 2024 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <aggregator/field.hpp>
#include <aggregator/print.hpp>
#include <aggregator/value.hpp>

#include <libfds.h>

#include <memory>

namespace fdsdump {
namespace aggregator {

/**
 * @brief A field that is the sum aggregation of the value
 */
class SumAggregatedField : public Field {
public:
    /**
     * @brief Create a sum aggregation of the source field
     *
     * @param source_field  The source field that will be aggregated
     */
    SumAggregatedField(std::unique_ptr<Field> source_field);

    /**
     * @brief Initialize the value with the default value
     *
     * @param  The value
     */
    void
    init(Value &value) const override;

    /**
     * @brief Aggregate the value retrieved from the provided flow record into the provided
     *        aggregated value
     *
     * @param ctx  The flow record to retrieve the value that will be aggregated from
     * @param aggregated_value  The currently accumulated value that the retrieved value will be
     *                          aggregated towards
     *
     * @return true if the field was found in the flow record and the aggregated value was updated,
     * false otherwise
     */
    bool
    aggregate(FlowContext &ctx, Value &aggregated_value) const override;

    /**
     * @brief Merge one value with another
     *
     * @param value  The value that will be updated with the result of the merge
     * @param other  The value to merge onto the first value
     */
    void
    merge(Value &value, const Value &other) const override;

    /**
     * @brief Get a string representation of the field
     */
    std::string
    repr() const override;

    /**
     * @brief Check if the fields are equal
     */
    bool
    operator==(const Field &other) const override;

private:
    std::unique_ptr<Field> m_source_field;
};

/**
 * @brief A field that is the min aggregation of the value
 */
class MinAggregatedField : public Field {
public:
    /**
     * @brief Create a min aggregation of the source field
     *
     * @param source_field  The source field that will be aggregated
     */
    MinAggregatedField(std::unique_ptr<Field> source_field);

    /**
     * @brief Initialize the value with the default value
     *
     * @param  The value
     */
    void
    init(Value &value) const override;

    /**
     * @brief Aggregate the value retrieved from the provided flow record into the provided
     *        aggregated value
     *
     * @param ctx  The flow record to retrieve the value that will be aggregated from
     * @param aggregated_value  The currently accumulated value that the retrieved value will be
     *                          aggregated towards
     *
     * @return true if the field was found in the flow record and the aggregated value was updated,
     * false otherwise
     */
    bool
    aggregate(FlowContext &ctx, Value &aggregated_value) const override;

    /**
     * @brief Merge one value with another
     *
     * @param value  The value that will be updated with the result of the merge
     * @param other  The value to merge onto the first value
     */
    void
    merge(Value &value, const Value &other) const override;

    /**
     * @brief Get a string representation of the field
     */
    std::string
    repr() const override;

    /**
     * @brief Check if the fields are equal
     */
    bool
    operator==(const Field &other) const override;

private:
    std::unique_ptr<Field> m_source_field;
};

/**
 * @brief A field that is the max aggregation of the value
 */
class MaxAggregatedField : public Field {
public:
    /**
     * @brief Create a max aggregation of the source field
     *
     * @param source_field  The source field that will be aggregated
     */
    MaxAggregatedField(std::unique_ptr<Field> source_field);

    /**
     * @brief Initialize the value with the default value
     *
     * @param  The value
     */
    void
    init(Value &value) const override;

    /**
     * @brief Aggregate the value retrieved from the provided flow record into the provided
     *        aggregated value
     *
     * @param ctx  The flow record to retrieve the value that will be aggregated from
     * @param aggregated_value  The currently accumulated value that the retrieved value will be
     *                          aggregated towards
     *
     * @return true if the field was found in the flow record and the aggregated value was updated,
     * false otherwise
     */
    bool
    aggregate(FlowContext &ctx, Value &aggregated_value) const override;

    /**
     * @brief Merge one value with another
     *
     * @param value  The value that will be updated with the result of the merge
     * @param other  The value to merge onto the first value
     */
    void
    merge(Value &value, const Value &other) const override;

    /**
     * @brief Get a string representation of the field
     */
    std::string
    repr() const override;

    /**
     * @brief Check if the fields are equal
     */
    bool
    operator==(const Field &other) const override;

private:
    std::unique_ptr<Field> m_source_field;
};

} // aggregator
} // fdsdump

/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief In/out fields
 *
 * Copyright: (C) 2024 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <aggregator/field.hpp>

#include <memory>

namespace fdsdump {
namespace aggregator {

/**
 * @brief A special key field providing a view on a value from both in/out directions,
 *        e.g. in/out bytes of an IP address
 */
class InOutKeyField : public Field {
public:
    /**
     * @brief Create an instance of a InOutKeyField
     *
     * @param in_field  The source field in the In direction
     * @param out_field  The source field in the Out direction
     */
    InOutKeyField(std::unique_ptr<Field> in_field, std::unique_ptr<Field> out_field);

    /**
     * @brief Load the value of the field flow record
     *
     * @param[in] ctx  The flow record
     * @param[out] value  The value
     */
    bool
    load(FlowContext &ctx, Value &value) const override;

    /**
     * @brief Get a string representation of the field
     */
    std::string
    repr() const override;

    /**
     * @brief Check if the fields are equal
     */
    bool
    operator==(const InOutKeyField &other) const;

    /**
     * @brief Check if the fields are equal
     */
    bool
    operator==(const Field &other) const override;

    /**
     * @brief Create the an in/out field from a non-in/out source field
     */
    static std::unique_ptr<Field>
    create_from(const Field &field);

private:
    std::unique_ptr<Field> m_in_field;
    std::unique_ptr<Field> m_out_field;
};

/**
 * @brief A special value field providing a view on a value from both in/out directions,
 *        e.g. in/out bytes of an IP address
 */
class InOutValueField : public Field {
public:
    /**
     * @brief Create an instance of a InOutValueField
     *
     * @param field  The source aggregation field
     * @param dir    The direction in which to aggregate the field
     */
    InOutValueField(std::unique_ptr<Field> field, ViewDirection dir);

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
    operator==(const InOutValueField &other) const;

    /**
     * @brief Check if the fields are equal
     */
    bool
    operator==(const Field &other) const override;

private:
    std::unique_ptr<Field> m_field;
    ViewDirection m_dir;
};

} // aggregator
} // fdsdump

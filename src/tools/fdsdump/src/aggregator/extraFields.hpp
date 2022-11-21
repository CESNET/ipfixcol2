/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Extra fields
 *
 * Copyright: (C) 2024 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <aggregator/field.hpp>

#include <string>

namespace fdsdump {
namespace aggregator {

/**
 * @brief A special field providing the number of flows
 */
class FlowCountField : public Field {
public:
    /**
     * @brief Create a new instance of a FlowCountField
     */
    FlowCountField();

    /**
     * @brief Load the value of the field flow record
     *
     * @param[in] ctx  The flow record
     * @param[out] value  The value
     */
    bool
    load(FlowContext &ctx, Value &value) const override;

    /**
     * @brief Check if the fields are equal
     */
    bool
    operator==(const Field &other) const override;

    /**
     * @brief Get a string representation of the field
     */
    std::string
    repr() const override;
};

/**
 * @brief A special field providing the direction of the flow
 */
class DirectionField : public Field {
public:
    static constexpr uint8_t FWD_VALUE = 1;
    static constexpr uint8_t REV_VALUE = 2;

    DirectionField();

    /**
     * @brief Load the value of the field flow record
     *
     * @param[in] ctx  The flow record
     * @param[out] value  The value
     */
    bool
    load(FlowContext &ctx, Value &value) const override;

    /**
     * @brief Check if the fields are equal
     */
    bool
    operator==(const Field &other) const override;

    /**
     * @brief Get a string representation of the field
     */
    std::string
    repr() const override;
};

/**
 * @brief A special field allowing to group datetime fields into time windows of a specified duration
 */
class TimeWindowField : public Field {
public:
    /**
     * @brief Create a new instance of a TimeWindowField
     *
     * @param source_field  The source field
     * @param window_millisec  The length of the time window in milliseconds
     */
    TimeWindowField(std::unique_ptr<Field> source_field, uint64_t window_millisec);

    /**
     * @brief Load the value of the field flow record
     *
     * @param[in] ctx  The flow record
     * @param[out] value  The value
     */
    bool
    load(FlowContext &ctx, Value &value) const override;

    /**
     * @brief Check if the fields are equal
     */
    bool
    operator==(const Field &other) const override;

    /**
     * @brief Get a string representation of the field
     */
    std::string
    repr() const override;

private:
    uint64_t m_window_millisec;
    std::unique_ptr<Field> m_source_field;
};

/**
 * @brief A special field allowing to group IP fields based on their subnet
 */
class SubnetField : public Field {
    friend class InOutKeyField;

public:
    /**
     * @brief Create a new instance of a SubnetField
     *
     * @param source_field  The source field
     * @param prefix_len  The prefix length of the subnet
     */
    SubnetField(std::unique_ptr<Field> source_field, uint8_t prefix_len);

    /**
     * @brief Load the value of the field flow record
     *
     * @param[in] ctx  The flow record
     * @param[out] value  The value
     */
    bool
    load(FlowContext &ctx, Value &value) const override;

    /**
     * @brief Check if the fields are equal
     */
    bool
    operator==(const Field &other) const override;

    /**
     * @brief Get a string representation of the field
     */
    std::string
    repr() const override;

private:
    std::unique_ptr<Field> m_source_field;
    uint8_t m_startbyte;
    uint8_t m_startmask;
    uint8_t m_zerobytes;
    uint8_t m_prefix_len;
};

} // aggregator
} // fdsdump

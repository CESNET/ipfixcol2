/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Field base
 *
 * Copyright: (C) 2024 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <aggregator/flowContext.hpp>
#include <aggregator/value.hpp>

#include <libfds.h>

#include <cstddef>
#include <string>
#include <memory>
#include <typeinfo>

namespace fdsdump {
namespace aggregator {

/**
 * @brief Result of a comparison
 */
enum class CmpResult {
    Lt,
    Eq,
    Gt
};

/**
 * @brief The base class for all aggregator fields
 */
class Field {
    friend class ViewFactory;

public:
    /**
     * @brief Get the size of the field in bytes
     */
    virtual size_t size(const Value *value = nullptr) const { (void) value; return m_size; }

    /**
     * @brief Get the offset of the field from the beginning of the aggregation record
     */
    size_t offset() const { return m_offset; }

    /**
     * @brief Get the name of the field
     */
    const std::string &name() const { return m_name; }

    /**
     * @brief Get the data type of the field
     */
    DataType data_type() const { return m_data_type; }

    /**
     * @brief Load the value of the field flow record
     *
     * @param[in] ctx  The flow record
     * @param[out] value  The value
     */
    virtual bool
    load(FlowContext &ctx, Value &value) const;

    /**
     * @brief Initialize the value with the default value
     *
     * @param  The value
     */
    virtual void
    init(Value &value) const;

    /**
     * @brief Aggregate the value retrieved from the provided flow record into the provided
     *        aggregated value
     *
     * @param ctx  The flow record to retrieve the value that will be aggregated from
     * @param aggregated_value  The currently accumulated value that the retrieved value will be
     *                          aggregated towards
     *
     * @return true if the field was found in the flow record and the aggregated value was updated,
     *         false otherwise
     */
    virtual bool
    aggregate(FlowContext &ctx, Value &aggregated_value) const;

    /**
     * @brief Merge one value with another
     *
     * @param value  The value that will be updated with the result of the merge
     * @param other  The value to merge onto the first value
     */
    virtual void
    merge(Value &value, const Value &other) const;

    /**
     * @brief Compare two values
     *
     * @param a  The first value
     * @param b  The second value
     *
     * @return the result of the comparison
     */
    CmpResult
    compare(const Value &a, const Value &b) const;

    /**
     * @brief Get a string representation of the field
     */
    virtual std::string
    repr() const = 0;

    /**
     * @brief Check if the fields are equal
     */
    virtual bool
    operator==(const Field &other) const = 0;

    /**
     * @brief Check if the fields are equal
     */
    bool
    operator==(const std::unique_ptr<Field> &other) const;

    /**
     * @brief Check if the fields are not equal
     */
    bool
    operator!=(const Field &other) const;

    /**
     * @brief Check if the field is of the provided type
     */
    template <typename T>
    bool
    is_of_type() const
    {
        return typeid(T) == typeid(*this);
    }

    /**
     * @brief Check if the field is a number
     */
    bool
    is_number() const;

protected:
    size_t m_size = 0;
    size_t m_offset = 0;
    std::string m_name;
    DataType m_data_type = DataType::Unassigned;

    /**
     * @brief Set the data type of the field
     */
    void
    set_data_type(DataType data_type);

    /**
     * @brief Set the offset of the field
     */
    void
    set_offset(size_t offset);

    /**
     * @brief Set the name of the field
     */
    void
    set_name(std::string name);
};

} // aggregator
} // fdsdump

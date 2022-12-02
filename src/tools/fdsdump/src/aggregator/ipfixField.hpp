/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief IPFIX field
 *
 * Copyright: (C) 2024 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <aggregator/field.hpp>

#include <libfds.h>

namespace fdsdump {
namespace aggregator {

/**
 * @brief A field mapping directly to an IPFIX element
 */
class IpfixField : public Field {
public:
    /**
     * @brief Construct a new IPFIX field
     *
     * @param elem  The information element describing the IPFIX field
     */
    IpfixField(const fds_iemgr_elem &elem);

    /**
     * @brief Load the value of the IPFIX field from the flow record
     *
     * @param[in] ctx  The flow record
     * @param[out] value  The value
     */
    bool load(FlowContext &ctx, Value &value) const override;

    /**
     * @brief Get a string representation of the field
     */
    std::string repr() const override;

    /**
     * @brief Get the Private Enterprise Number of the IPFIX element
     */
    uint32_t pen() const;

    /**
     * @brief Get the ID number of the IPFIX element
     */
    uint16_t id() const;

    /**
     * @brief Compare two IPFIX fields for equality
     */
    bool operator==(const IpfixField &other) const;

    /**
     * @brief Compare two fields for equality
     */
    bool operator==(const Field &other) const override;

    /**
     * @brief Get the size the value of this field occupies in the aggregation record
     */
    size_t size(const Value* value = nullptr) const override;

private:
    uint32_t m_pen;
    uint16_t m_id;
};

} // aggregator
} // fdsdump

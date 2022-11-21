/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Alias field
 *
 * Copyright: (C) 2024 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <aggregator/field.hpp>
#include <aggregator/ipfixField.hpp>

#include <libfds.h>

#include <vector>

namespace fdsdump {
namespace aggregator {

/**
 * @brief A field that is an alias mapping to other fields
 */
class AliasField : public Field {
public:
    /**
     * @brief Create a new field based on an iemgr alias
     *
     * @param alias  The iemgr alias
     */
    AliasField(const fds_iemgr_alias &alias);

    /**
     * @brief Load the value of the field flow record
     *
     * @param[in] ctx  The flow record
     * @param[out] value  The value
     */
    virtual bool
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
    operator==(const AliasField &other) const;

    /**
     * @brief Check if the fields are equal
     */
    bool
    operator==(const Field &other) const override;

private:
    std::vector<IpfixField> m_sources;
};

} // aggregator
} // fdsdump

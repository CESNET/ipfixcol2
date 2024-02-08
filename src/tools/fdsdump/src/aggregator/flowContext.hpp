/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief The flow context
 *
 * Copyright: (C) 2024 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <libfds.h>

#include <cstdint>

namespace fdsdump {
namespace aggregator {

/**
 * @brief The view direction
 */
enum class ViewDirection {
    None,
    In,
    Out
};

/**
 * @brief The flow direction
 */
enum class FlowDirection {
    None,
    Forward,
    Reverse
};

/**
 * @brief The flow context
 */
struct FlowContext {
    fds_drec &drec;
    ViewDirection view_dir = ViewDirection::None;
    FlowDirection flow_dir = FlowDirection::None;

    /**
     * @brief Create a flow context instance for the underlying flow data record
     */
    FlowContext(fds_drec &drec) : drec(drec) {}

    /**
     * @brief Find an IPFIX field in the underlying data record
     *
     * @param[in] pen  The IPFIX PEN of the field
     * @param[in] id  The IPFIX ID of the field
     * @param[out] field  The field if found
     *
     * @return true if the field was found, else false
     */
    bool
    find_field(uint32_t pen, uint16_t id, fds_drec_field &field);
};

} // aggregator
} // fdsdump

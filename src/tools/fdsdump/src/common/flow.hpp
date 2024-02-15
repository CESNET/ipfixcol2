/**
 * @file
 * @author Lukas Hutak <hutak@cesnet.cz>
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Flow abstraction
 *
 * Copyright: (C) 2024 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

namespace fdsdump {

/**
 * @brief Direction of flow record to be considered when processing.
 *
 * Flow records might be unidirection or bidirectional. During flow processing,
 * however, we may only be interested in selected flow directions, e.g. due to
 * applied filtering according to user-defined parameters.
 */
enum Direction {
    /** @brief No direction should be processed.                   */
    DIRECTION_NONE = 0x00,
    /** @brief Process the record from the forward direction only. */
    DIRECTION_FWD =  0x01,
    /** @brief Process the record from the reverse direction only. */
    DIRECTION_REV =  0x02,
    /** @brief Process the record from the both directions.        */
    DIRECTION_BOTH = 0x03,
};

/**
 * @brief Flow record to be processed.
 * @warning
 *   This structure cannot be simply copied as <em>rec</em> contains
 *   primitive reference to IPFIX templates and flow data.
 */
struct Flow {
    /** @brief Direction(s) of the flow to be considered.          */
    enum Direction dir;
    /** @brief Flow Data record                                    */
    struct fds_drec rec;
};

} // fdsdump

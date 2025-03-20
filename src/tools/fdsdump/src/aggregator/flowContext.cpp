/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief The flow context
 *
 * Copyright: (C) 2024 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <aggregator/flowContext.hpp>

namespace fdsdump {
namespace aggregator {

bool
FlowContext::find_field(uint32_t pen, uint16_t id, fds_drec_field &field)
{
    if (flow_dir == FlowDirection::None) {
        int ret = fds_drec_find(&drec, pen, id, &field);
        return ret != FDS_EOC;

    } else {
        int flags = (flow_dir == FlowDirection::Forward
                ? FDS_DREC_BIFLOW_FWD : FDS_DREC_BIFLOW_REV);
        fds_drec_iter iter;
        fds_drec_iter_init(&iter, &drec, flags);
        int ret = fds_drec_iter_find(&iter, pen, id);
        if (ret != FDS_EOC) {
            field = iter.field;
            return true;
        } else {
            return false;
        }
    }
}

} // aggregator
} // fdsdump

/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Information element manager singleton
 */

#pragma once

#include <libfds.h>

#include <memory>

namespace fdsdump {

class IEMgr {
public:
    /**
     * @brief Get the iemgr singleton instance
     */
    static IEMgr &instance();

    /**
     * @brief Get the pointer to the underlying fds_iemgr
     */
    fds_iemgr_t *ptr() { return m_iemgr.get(); }

private:
    std::unique_ptr<fds_iemgr_t, decltype(&fds_iemgr_destroy)> m_iemgr;

    IEMgr();
};

} // fdsdump

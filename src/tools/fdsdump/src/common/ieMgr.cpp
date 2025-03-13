/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Information element manager singleton
 */

#include <common/ieMgr.hpp>

#include <string>
#include <stdexcept>

namespace fdsdump {

IEMgr &
IEMgr::instance()
{
    static IEMgr iemgr;
    return iemgr;
}

IEMgr::IEMgr() :
    m_iemgr(fds_iemgr_create(), &fds_iemgr_destroy)
{
    if (!m_iemgr) {
        throw std::runtime_error("fds_iemgr_create() has failed");
    }

    const std::string dir = fds_api_cfg_dir();
    int ret = fds_iemgr_read_dir(m_iemgr.get(), dir.c_str());
    if (ret != FDS_OK) {
        const std::string err_msg = fds_iemgr_last_err(m_iemgr.get());
        throw std::runtime_error("fds_iemgr_read_dir() failed: " + err_msg);
    }
}

} // fdsdump

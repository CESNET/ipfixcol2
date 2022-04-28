
#include <cstdlib>
#include <iostream>
#include <vector>
#include <unistd.h>

#include <libfds.h>

#include <common/common.hpp>
#include <common/filelist.hpp>
#include <lister/lister.hpp>
#include <aggregator/mode.hpp>

#include "options.hpp"


static unique_iemgr iemgr_prepare(const std::string &path)
{
    unique_iemgr iemgr {fds_iemgr_create(), &fds_iemgr_destroy};
    int ret;

    if (!iemgr) {
        throw std::runtime_error("fds_iemgr_create() has failed");
    }

    ret = fds_iemgr_read_dir(iemgr.get(), path.c_str());
    if (ret != FDS_OK) {
        const std::string err_msg = fds_iemgr_last_err(iemgr.get());
        throw std::runtime_error("fds_iemgr_read_dir() failed: " + err_msg);
    }

    return iemgr;
}

static void fds_iemgr_deleter(fds_iemgr_t *mgr) {
    if (mgr != nullptr) {
        fds_iemgr_destroy(mgr);
    }
}

int
main(int argc, char *argv[])
{
    try {
        Options options {argc, argv};
        shared_iemgr iemgr {nullptr, fds_iemgr_deleter};

        iemgr = iemgr_prepare(std::string(fds_api_cfg_dir()));

        switch (options.get_mode()) {
        case Options::Mode::list:
            mode_list(iemgr, options);
            break;
        case Options::Mode::aggregate:
            mode_aggregate(iemgr, options);
            break;
        default:
            throw std::runtime_error("Invalid mode");
        }
    } catch (const std::exception &ex) {
        std::cerr << "ERROR: " << ex.what() << std::endl;
        return EXIT_FAILURE;
    }

    return 0;
}


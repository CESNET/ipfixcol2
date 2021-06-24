#pragma once

#include <libfds.h>
#include <memory>

struct fds_file_closer
{
    void
    operator()(fds_file_t *file)
    {
        fds_file_close(file);
    }
};

using unique_fds_file = std::unique_ptr<fds_file_t, fds_file_closer>;

struct fds_iemgr_destroyer
{
    void
    operator()(fds_iemgr_t *iemgr)
    {
        fds_iemgr_destroy(iemgr);
    }
};

using unique_fds_iemgr = std::unique_ptr<fds_iemgr_t, fds_iemgr_destroyer>;
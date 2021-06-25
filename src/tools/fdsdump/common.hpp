#pragma once

#include <libfds.h>
#include <memory>

struct fds_file_closer
{
    void
    operator()(fds_file_t *file) const
    {
        fds_file_close(file);
    }
};

using unique_fds_file = std::unique_ptr<fds_file_t, fds_file_closer>;

struct fds_iemgr_destroyer
{
    void
    operator()(fds_iemgr_t *iemgr) const
    {
        fds_iemgr_destroy(iemgr);
    }
};

using unique_fds_iemgr = std::unique_ptr<fds_iemgr_t, fds_iemgr_destroyer>;

struct fds_filter_destroyer
{
    void
    operator()(fds_filter_t *filter) const
    {
        fds_filter_destroy(filter);
    }
};

using unique_fds_filter = std::unique_ptr<fds_filter_t, fds_filter_destroyer>;

struct fds_filter_opts_destroyer
{
    void
    operator()(fds_filter_opts_t *opts) const
    {
        fds_filter_destroy_opts(opts);
    }
};

using unique_fds_filter_opts = std::unique_ptr<fds_filter_opts_t, fds_filter_opts_destroyer>;

struct fds_ipfix_filter_destroyer
{
    void
    operator()(fds_ipfix_filter_t *filter) const
    {
        fds_ipfix_filter_destroy(filter);
    }
};

using unique_fds_ipfix_filter = std::unique_ptr<fds_ipfix_filter_t, fds_ipfix_filter_destroyer>;

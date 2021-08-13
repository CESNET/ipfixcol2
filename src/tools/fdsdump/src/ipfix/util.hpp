#pragma once

#include <memory>
#include <stdexcept>
#include <libfds.h>

struct fds_file_closer {
    void operator()(fds_file_t *file) const { fds_file_close(file); }
};

struct fds_iemgr_destroyer {
    void operator()(fds_iemgr_t *iemgr) const { fds_iemgr_destroy(iemgr); }
};

struct fds_filter_destroyer {
    void operator()(fds_filter_t *filter) const { fds_filter_destroy(filter); }
};

struct fds_filter_opts_destroyer {
    void operator()(fds_filter_opts_t *opts) const { fds_filter_destroy_opts(opts); }
};

struct fds_ipfix_filter_destroyer {
    void operator()(fds_ipfix_filter_t *filter) const { fds_ipfix_filter_destroy(filter); }
};

using unique_fds_file = std::unique_ptr<fds_file_t, fds_file_closer>;
using unique_fds_iemgr = std::unique_ptr<fds_iemgr_t, fds_iemgr_destroyer>;
using unique_fds_filter = std::unique_ptr<fds_filter_t, fds_filter_destroyer>;
using unique_fds_filter_opts = std::unique_ptr<fds_filter_opts_t, fds_filter_opts_destroyer>;
using unique_fds_ipfix_filter = std::unique_ptr<fds_ipfix_filter_t, fds_ipfix_filter_destroyer>;

static inline uint64_t
get_uint(fds_drec_field &field)
{
    uint64_t tmp;
    int rc = fds_get_uint_be(field.data, field.size, &tmp);
    assert(rc == FDS_OK);
    return tmp;
}

static inline int64_t
get_int(fds_drec_field &field)
{
    int64_t tmp;
    int rc = fds_get_int_be(field.data, field.size, &tmp);
    assert(rc == FDS_OK);
    return tmp;
}

static inline uint64_t
get_datetime(fds_drec_field &field)
{
    uint64_t tmp;
    int rc = fds_get_datetime_lp_be(field.data, field.size, field.info->def->data_type, &tmp);
    assert(rc == FDS_OK);
    return tmp;
}

static unique_fds_iemgr
make_iemgr()
{
    int rc;

    unique_fds_iemgr iemgr;
    iemgr.reset(fds_iemgr_create());
    if (!iemgr) {
        throw std::bad_alloc();
    }

    rc = fds_iemgr_read_dir(iemgr.get(), fds_api_cfg_dir());
    if (rc != FDS_OK) {
        throw std::runtime_error("cannot read iemgr definitions");
    }

    return iemgr;
}

static int
fds_drec_find(fds_drec *drec, uint32_t pen, uint16_t id, uint16_t flags, fds_drec_field *field)
{
    if (flags == 0) {
        return fds_drec_find(drec, pen, id, field);

    } else {
        fds_drec_iter iter;
        fds_drec_iter_init(&iter, drec, flags);

        int ret = fds_drec_iter_find(&iter, pen, id);
        if (ret != FDS_EOC) {
            *field = iter.field;
        }

        assert(ret == FDS_EOC || field->data != nullptr);

        return ret;
    }
}
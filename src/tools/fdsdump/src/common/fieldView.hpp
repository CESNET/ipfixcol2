
#pragma once

#include <ctime>

#include <libfds.h>

#include "ipaddr.hpp"

namespace fdsdump {

class FieldView {
public:
    FieldView(const struct fds_drec_field &field)
        : m_field(field) {};

    uint64_t as_uint() const;
    int64_t as_int() const;
    double as_float() const;
    bool as_bool() const;
    struct timespec as_datetime() const;
    uint64_t as_datetime_ms() const;
    IPAddr as_ipaddr() const;

private:
    const struct fds_drec_field &m_field;
};

} // fdsdump

/**
 * @file
 * @author Lukas Hutak <hutak@cesnet.cz>
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Field view
 *
 * Copyright: (C) 2024 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <cstdint>
#include <ctime>
#include <string>
#include <vector>

#include <libfds.h>

#include <common/ipaddr.hpp>

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
    std::string as_string() const;
    std::vector<uint8_t> as_bytes() const;

private:
    const struct fds_drec_field &m_field;
};

} // fdsdump

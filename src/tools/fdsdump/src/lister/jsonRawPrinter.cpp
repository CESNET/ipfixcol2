/**
 * @file
 * @author Lukas Hutak <hutak@cesnet.cz>
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief JSON-RAW lister printer
 *
 * Copyright: (C) 2024 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <cstdlib>
#include <iostream>
#include <string>
#include <stdexcept>
#include <vector>

#include <common/common.hpp>

#include <common/ieMgr.hpp>
#include <lister/jsonRawPrinter.hpp>

namespace fdsdump {
namespace lister {

JsonRawPrinter::JsonRawPrinter(const std::string &args)
{
    const std::vector<std::string> args_vec = string_split(args, ",");

    if (args.empty()) {
        return;
    }

    for (const std::string &arg_raw : args_vec) {
        const std::string arg = string_trim_copy(arg_raw);

        if (strcasecmp(arg.c_str(), "no-biflow-split") == 0) {
            m_biflow_split = false;
        } else if (strcasecmp(arg.c_str(), "hide-reverse") == 0) {
            m_biflow_hide_reverse = true;
        } else {
            throw std::invalid_argument("JSON output: unknown option '" + arg + "'");
        }
    }

    if (m_biflow_hide_reverse && !m_biflow_split) {
        throw std::invalid_argument(
            "JSON output: reverse field hiding requires enabled biflow splitting");
    }
}

JsonRawPrinter::~JsonRawPrinter()
{
    if (m_buffer) {
        free(m_buffer);
    }
}

void
JsonRawPrinter::print_record(struct fds_drec *rec, uint32_t flags)
{
    const fds_iemgr_t *iemgr = IEMgr::instance().ptr();
    const uint32_t base_flags =
        FDS_CD2J_ALLOW_REALLOC | FDS_CD2J_OCTETS_NOINT | FDS_CD2J_TS_FORMAT_MSEC;
    int ret;

    flags |= base_flags;

    ret = fds_drec2json(rec, flags, iemgr, &m_buffer, &m_buffer_size);
    if (ret < 0) {
        throw std::runtime_error("JSON conversion failed: " + std::to_string(ret));
    }

    std::cout << m_buffer << "\n";
}

unsigned int
JsonRawPrinter::print_record(Flow *flow)
{
    uint32_t flags = 0;

    if (m_biflow_hide_reverse) {
        flags |= FDS_CD2J_REVERSE_SKIP;
    }

    switch (flow->dir) {
    case DIRECTION_NONE:
        return 0;
    case DIRECTION_FWD:
        print_record(&flow->rec, flags);
        return 1;
    case DIRECTION_REV:
        print_record(&flow->rec, flags | FDS_CD2J_BIFLOW_REVERSE);
        return 1;
    case DIRECTION_BOTH:
        if (m_biflow_split) {
            print_record(&flow->rec, flags);
            print_record(&flow->rec, flags | FDS_CD2J_BIFLOW_REVERSE);
            return 2;
        } else {
            print_record(&flow->rec, flags);
            return 1;
        }
    }

    return 0;
}

} // lister
} // fdsdump

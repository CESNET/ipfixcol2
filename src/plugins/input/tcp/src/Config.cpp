/**
 * \file
 * \author Jakub Antonín Štigler <xstigl00@stud.fit.vutbr.cz>
 * \brief Config for tcp input plugin (source file)
 * \date 2024
 *
 * Copyright: (C) 2023 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "Config.hpp"

#include <memory>    // unique_ptr
#include <stdexcept> // runtime_error, invalid_argument
#include <string>    // string
#include <cassert>   // assert
#include <cstdint>   // UINT16_MAX

#include <libfds.h> // fds_*, FDS_*

#include <ipfixcol2.h> // ipx_ctx, IPX_CTX_WARNING

#include "IpAddress.hpp" // IpAddress

#define DEFAULT_PORT 4739

namespace tcp_in {

/*
 * <params>
 *  <localPort>...</localPort>                    <!-- optional -->
 *  <localIPAddress>...</localIPAddress>          <!-- optional, multiple times -->
 * </params>
 */

enum ParamsXmlNodes {
    PARAM_PORT,
    PARAM_IPADDR,
};

static const struct fds_xml_args args_params[] = {
    FDS_OPTS_ROOT("params"),
    FDS_OPTS_ELEM(PARAM_PORT  , "localPort"     , FDS_OPTS_T_UINT  , FDS_OPTS_P_OPT),
    FDS_OPTS_ELEM(PARAM_IPADDR, "localIPAddress", FDS_OPTS_T_STRING, FDS_OPTS_P_OPT
                                                                   | FDS_OPTS_P_MULTI),
    FDS_OPTS_END,
};

Config::Config(ipx_ctx *ctx, const char *params) : local_port(DEFAULT_PORT), local_addrs() {
    std::unique_ptr<fds_xml_t, decltype(&fds_xml_destroy)> xml(fds_xml_create(), &fds_xml_destroy);
    if (!xml) {
        throw std::runtime_error("Failed to create XML parser.");
    }

    if (fds_xml_set_args(xml.get(), args_params) != FDS_OK) {
        std::string err = fds_xml_last_err(xml.get());
        throw std::runtime_error("Failed to parse XML document description: " + err);
    }

    fds_xml_ctx *params_ctx = fds_xml_parse_mem(xml.get(), params, true);
    if (!params_ctx) {
        std::string err = fds_xml_last_err(xml.get());
        throw std::runtime_error("Failed to parse the TCP configuration: " + err);
    }

    parse_params(ctx, params_ctx);
}

void Config::parse_params(ipx_ctx *ctx, fds_xml_ctx_t *params) {
    const struct fds_xml_cont *content;
    bool empty_address = false;

    while (fds_xml_next(params, &content) != FDS_EOC) {
        switch (content->id) {
        case PARAM_PORT:
            assert(content->type == FDS_OPTS_T_UINT);
            if (content->val_uint > UINT16_MAX) {
                throw std::invalid_argument(
                    "Port must have value in range from 0 to 65535 but it was " + std::to_string(
                        content->val_uint
                    )
                );
            }
            local_port = content->val_uint;
            break;
        case PARAM_IPADDR:
            assert(content->type == FDS_OPTS_T_STRING);
            // check if the string is not empty
            if (*content->ptr_string) {
                local_addrs.push_back(IpAddress(content->ptr_string));
            } else {
                empty_address = true;
            }
            break;
        default:
            throw std::invalid_argument("Unexpected element within <params>.");
        }
    }

    if (empty_address && local_addrs.size() != 0) {
        IPX_CTX_WARNING(
            ctx,
            "Empty address in configuration ignored. Tcp plugin will NOT "
            "listen on all interfaces but only on the specified addresses."
        );
    }
}

} // namespace tcp_in


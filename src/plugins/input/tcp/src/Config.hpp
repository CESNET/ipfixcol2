/**
 * \file
 * \author Jakub Antonín Štigler <xstigl00@stud.fit.vutbr.cz>
 * \brief Config for tcp input plugin (header file)
 * \date 2024
 *
 * Copyright: (C) 2023 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <vector>  // std::vector
#include <cstdint> // uint16_t

#include <libfds.h> // fds_xml_ctx_t

#include <ipfixcol2.h> // ipx_ctx

#include "IpAddress.hpp" // IpAddress

namespace tcp_in {

/** TCP input plugin configuration */
struct Config {
    uint16_t local_port;
    std::vector<IpAddress> local_addrs;

    /**
     * @brief Parse configuration of the TCP plugin
     * @param[in] params Plugin part of the configuration of the collector
     * @throws std::invalid_argument if the arguments have invalid value
     * @throws std::runtime_error other errors while parsing xml
     */
    Config(ipx_ctx *ctx, const char *params);

private:
    void parse_params(ipx_ctx *ctx, fds_xml_ctx_t *params);
};

} // namespace tcp_in

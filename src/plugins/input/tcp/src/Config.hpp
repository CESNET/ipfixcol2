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

#include <cstdint> // uint16_t
#include <string>
#include <vector>  // std::vector

#include <libfds.h> // fds_xml_ctx_t

#include <ipfixcol2.h> // ipx_ctx

#include "IpAddress.hpp" // IpAddress

namespace tcp_in {

/** TCP input plugin configuration */
struct Config {
    uint16_t local_port;
    std::vector<IpAddress> local_addrs;
    /**
     * @brief Path to file in pem format which contains certificate and private key for TLS. If
     * empty, TLS connections are not accepted.
     */
    std::string certificate_file;
    /** Path to private key. */
    std::string private_key_file;
    /** If true, ipfixcol server will require certificate verification of clients */
    bool verify_peer = false;
    /** Path to certificate authority. */
    std::string ca_file;
    /** If true, load default ca file. */
    bool default_ca_file = false;
    /** Path to certificate authority directory. */
    std::string ca_dir;
    /** If true, load default ca directory. */
    bool default_ca_dir = false;
    /** Path to certificate authority store. */
    std::string ca_store;
    /** If true, load default ca store. */
    bool default_ca_store;
    /** If set, ignore any other ca configuration paths and use system defaults. */
    bool use_default_ca = true;
    /** If set to false, only TCP connections are accepted. */
    bool allow_insecure = true;

    /**
     * @brief Parse configuration of the TCP plugin
     * @param[in] params Plugin part of the configuration of the collector
     * @throws std::invalid_argument if the arguments have invalid value
     * @throws std::runtime_error other errors while parsing xml
     */
    Config(ipx_ctx *ctx, const char *params);

private:
    void parse_params(ipx_ctx *ctx, fds_xml_ctx_t *params);
    void parse_tls(ipx_ctx *ctx, fds_xml_ctx_t *params);
};

} // namespace tcp_in

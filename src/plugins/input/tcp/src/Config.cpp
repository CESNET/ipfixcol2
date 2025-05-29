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
#include <cstring>

#include <libfds.h> // fds_*, FDS_*
#include <openssl/opensslv.h>

#include <ipfixcol2.h> // ipx_ctx, IPX_CTX_WARNING

#include "IpAddress.hpp" // IpAddress

#define DEFAULT_PORT 4739

namespace tcp_in {

/*
 * <params>
 *  <localPort>...</localPort>                    <!-- optional -->
 *  <localIPAddress>...</localIPAddress>          <!-- optional, multiple times -->
 *  <certificatePath>...</certificatePath>        <!-- optional -->
 * </params>
 */

enum ParamsXmlNodes {
    PARAM_PORT,
    PARAM_IPADDR,
    PARAM_TLS,

    PARAM_TLS_CERTIFICATE,
    PARAM_TLS_PRIVATE_KEY,
    PARAM_TLS_VERIFY_PEER,
    PARAM_TLS_CA_FILE,
    PARAM_TLS_CA_DIR,
    PARAM_TLS_CA_STORE,
    PARAM_TLS_INSECURE,
};

static const struct fds_xml_args args_tls[] = {
    FDS_OPTS_ELEM(PARAM_TLS_CERTIFICATE, "certificateFile", FDS_OPTS_T_STRING, 0),
    FDS_OPTS_ELEM(PARAM_TLS_PRIVATE_KEY, "privateKeyFile" , FDS_OPTS_T_STRING, FDS_OPTS_P_OPT),
    FDS_OPTS_ELEM(PARAM_TLS_VERIFY_PEER, "verifyPeer"     , FDS_OPTS_T_BOOL  , FDS_OPTS_P_OPT),
    FDS_OPTS_ELEM(PARAM_TLS_CA_FILE    , "caFile"         , FDS_OPTS_T_STRING, FDS_OPTS_P_OPT),
    FDS_OPTS_ELEM(PARAM_TLS_CA_DIR     , "caDir"          , FDS_OPTS_T_STRING, FDS_OPTS_P_OPT),
    FDS_OPTS_ELEM(PARAM_TLS_CA_STORE   , "caStore"        , FDS_OPTS_T_STRING, FDS_OPTS_P_OPT),
    FDS_OPTS_ELEM(PARAM_TLS_INSECURE   , "allowInsecure"  , FDS_OPTS_T_BOOL  , FDS_OPTS_P_OPT),
    FDS_OPTS_END,
};

static const struct fds_xml_args args_params[] = {
    FDS_OPTS_ROOT("params"),
    FDS_OPTS_ELEM(PARAM_PORT  , "localPort"     , FDS_OPTS_T_UINT  , FDS_OPTS_P_OPT),
    FDS_OPTS_ELEM(PARAM_IPADDR, "localIPAddress", FDS_OPTS_T_STRING, FDS_OPTS_P_OPT
                                                                   | FDS_OPTS_P_MULTI),
    FDS_OPTS_NESTED(PARAM_TLS , "tls"           , args_tls         , FDS_OPTS_P_OPT),
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
            // check if the string is empty
            if (std::strcmp(content->ptr_string, "") == 0) {
                empty_address = true;
            } else {
                local_addrs.push_back(IpAddress(content->ptr_string));
            }
            break;
        case PARAM_TLS:
            assert(content->type == FDS_OPTS_T_CONTEXT);
            parse_tls(ctx, content->ptr_ctx);
            break;
        default:
            throw std::invalid_argument("Unexpected element within <params>.");
        }
    }

    if (empty_address && local_addrs.size() != 0) {
        IPX_CTX_WARNING(
            ctx,
            "Empty address in configuration ignored. TCP plugin will NOT "
            "listen on all interfaces but only on the specified addresses."
        );
    }
}

void Config::parse_tls(ipx_ctx *ctx, fds_xml_ctx_t *params) {
    const struct fds_xml_cont *content;
    bool empty_private_key;

    // The default when TCP is enabled.
    allow_insecure = false;

    while (fds_xml_next(params, &content) != FDS_EOC) {
        switch (content->id) {
        case PARAM_TLS_CERTIFICATE:
            assert(content->type == FDS_OPTS_T_STRING);
            // check if the string is empty
            if (std::strcmp(content->ptr_string, "") == 0) {
                throw std::invalid_argument("TLS certificate path must not be empty.");
            } else {
                certificate_file = content->ptr_string;
            }
            break;
        case PARAM_TLS_PRIVATE_KEY:
            assert(content->type == FDS_OPTS_T_STRING);
            empty_private_key = std::strcmp(content->ptr_string, "") == 0;
            private_key_file = content->ptr_string;
            break;
        case PARAM_TLS_VERIFY_PEER:
            assert(content->type == FDS_OPTS_T_BOOL);
            verify_peer = content->val_bool;
            break;
        case PARAM_TLS_CA_FILE:
            assert(content->type == FDS_OPTS_T_STRING);
            default_ca_file = std::strcmp(content->ptr_string, "") == 0;
            ca_file = content->ptr_string;
            break;
        case PARAM_TLS_CA_DIR:
            assert(content->type == FDS_OPTS_T_STRING);
            default_ca_dir = std::strcmp(content->ptr_string, "") == 0;
            ca_dir = content->ptr_string;
            break;
        case PARAM_TLS_CA_STORE:
            assert(content->type == FDS_OPTS_T_STRING);
            default_ca_store = std::strcmp(content->ptr_string, "") == 0;
            ca_store = content->ptr_string;
            break;
        case PARAM_TLS_INSECURE:
            assert(content->type == FDS_OPTS_T_BOOL);
            allow_insecure = content->val_bool;
            break;
        default:
            throw std::invalid_argument("Unexpected element within <params>.");
        }
    }

    if (empty_private_key) {
        IPX_CTX_WARNING(
            ctx,
            "Empty private key ignored. Ipfixcol will use the same file as "
            "certificate."
        );
    }

    if (private_key_file.empty()) {
        private_key_file = certificate_file;
    }

    use_default_ca = !default_ca_file && ca_file.empty()
        && !default_ca_dir && ca_dir.empty()
        && !default_ca_store && ca_store.empty();

// Supported only from OpenSSL 3.0.0-0 release
#if OPENSSL_VERSION_NUMBER < 0x03000000f
    if (default_ca_store || !ca_store.empty()) {
        throw std::invalid_argument("Certificate store is not supported before openssl 3.");
    }
#endif // OPENSSL_VERSION_MAJOR < 0x03000000f
}

} // namespace tcp_in


/**
 * \file src/plugins/output/json/Config.cpp
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Configuration of JSON output plugin (source file)
 * \date 2018
 */

/* Copyright (C) 2018 CESNET, z.s.p.o.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of the Company nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * ALTERNATIVELY, provided that this notice is retained in full, this
 * product may be distributed under the terms of the GNU General Public
 * License (GPL) version 2 or later, in which case the provisions
 * of the GPL apply INSTEAD OF those given above.
 *
 * This software is provided ``as is'', and any express or implied
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose are disclaimed.
 * In no event shall the company or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even
 * if advised of the possibility of such damage.
 *
 */

#include <memory>
#include <set>

#include <libfds.h>
#include <inttypes.h>
#include <arpa/inet.h>

#include "Config.hpp"

/** XML nodes */
enum params_xml_nodes {
    // Formatting parameters
    FMT_TFLAGS,        /**< TCP flags                       */
    FMT_TIMESTAMP,     /**< Timestamp                       */
    FMT_PROTO,         /**< Protocol                        */
    FMT_UNKNOWN,       /**< Unknown definitions             */
    FMT_OPTIONS,       /**< Ignore Options Template Records */
    FMT_NONPRINT,      /**< Non-printable chars             */
    // Common output
    OUTPUT_LIST,       /**< List of output types            */
    OUTPUT_PRINT,      /**< Print to standard output        */
    OUTPUT_SEND,       /**< Send over network               */
    OUTPUT_SERVER,     /**< Provide as server               */
    OUTPUT_FILE,       /**< Store to file                   */
    // Standard output
    PRINT_NAME,        /**< Printer name                    */
    // Send output
    SEND_NAME,         /**< Sender name                     */
    SEND_IP,           /**< Destination IP                  */
    SEND_PORT,         /**< Destination port                */
    SEND_PROTO,        /**< Transport Protocol              */
    // Server output
    SERVER_NAME,       /**< Server name                     */
    SERVER_PORT,       /**< Server port                     */
    SERVER_BLOCK,      /**< Blocking connection             */
    // FIle output
    FILE_NAME,         /**< File storage name               */
    FILE_PATH,         /**< Path specification format       */
    FILE_PREFIX,       /**< File prefix                     */
    FILE_WINDOW,       /**< Window interval                 */
    FILE_ALIGN         /**< Window alignment                */
};

/** Definition of the \<print\> node  */
static const struct fds_xml_args args_print[] = {
    FDS_OPTS_ELEM(PRINT_NAME, "name", FDS_OPTS_T_STRING, 0),
    FDS_OPTS_END
};

/** Definition of the \<server\> node  */
static const struct fds_xml_args args_server[] = {
    FDS_OPTS_ELEM(SERVER_NAME,  "name",     FDS_OPTS_T_STRING, 0),
    FDS_OPTS_ELEM(SERVER_PORT,  "port",     FDS_OPTS_T_UINT,   0),
    FDS_OPTS_ELEM(SERVER_BLOCK, "blocking", FDS_OPTS_T_BOOL,   0),
    FDS_OPTS_END
};

/** Definition of the \<send\> node  */
static const struct fds_xml_args args_send[] = {
    FDS_OPTS_ELEM(SEND_NAME,  "name",     FDS_OPTS_T_STRING, 0),
    FDS_OPTS_ELEM(SEND_IP,    "ip",       FDS_OPTS_T_STRING, 0),
    FDS_OPTS_ELEM(SEND_PORT,  "port",     FDS_OPTS_T_UINT,   0),
    FDS_OPTS_ELEM(SEND_PROTO, "protocol", FDS_OPTS_T_STRING, 0),
    FDS_OPTS_END
};

/** Definition of the \<file\> node  */
static const struct fds_xml_args args_file[] = {
    FDS_OPTS_ELEM(FILE_NAME,   "name",          FDS_OPTS_T_STRING, 0),
    FDS_OPTS_ELEM(FILE_PATH,   "path",          FDS_OPTS_T_STRING, 0),
    FDS_OPTS_ELEM(FILE_PREFIX, "prefix",        FDS_OPTS_T_STRING, 0),
    FDS_OPTS_ELEM(FILE_WINDOW, "timeWindow",    FDS_OPTS_T_UINT,   0),
    FDS_OPTS_ELEM(FILE_ALIGN,  "timeAlignment", FDS_OPTS_T_BOOL,   0),
    FDS_OPTS_END
};

/** Definition of the \<outputs\> node  */
static const struct fds_xml_args args_outputs[] = {
    FDS_OPTS_NESTED(OUTPUT_PRINT,  "print",  args_print,  FDS_OPTS_P_OPT | FDS_OPTS_P_MULTI),
    FDS_OPTS_NESTED(OUTPUT_SERVER, "server", args_server, FDS_OPTS_P_OPT | FDS_OPTS_P_MULTI),
    FDS_OPTS_NESTED(OUTPUT_SEND,   "send",   args_send,   FDS_OPTS_P_OPT | FDS_OPTS_P_MULTI),
    FDS_OPTS_NESTED(OUTPUT_FILE,   "file",   args_file,   FDS_OPTS_P_OPT | FDS_OPTS_P_MULTI),
    FDS_OPTS_END
};

/** Definition of the \<params\> node  */
static const struct fds_xml_args args_params[] = {
    FDS_OPTS_ROOT("params"),
    FDS_OPTS_ELEM(FMT_TFLAGS,    "tcpFlags",  FDS_OPTS_T_STRING,      FDS_OPTS_P_OPT),
    FDS_OPTS_ELEM(FMT_TIMESTAMP, "timestamp", FDS_OPTS_T_STRING,      FDS_OPTS_P_OPT),
    FDS_OPTS_ELEM(FMT_PROTO,     "protocol",  FDS_OPTS_T_STRING,      FDS_OPTS_P_OPT),
    FDS_OPTS_ELEM(FMT_UNKNOWN,   "ignoreUnknown",    FDS_OPTS_T_BOOL, FDS_OPTS_P_OPT),
    FDS_OPTS_ELEM(FMT_OPTIONS,   "ignoreOptions",    FDS_OPTS_T_BOOL, FDS_OPTS_P_OPT),
    FDS_OPTS_ELEM(FMT_NONPRINT,  "nonPrintableChar", FDS_OPTS_T_BOOL, FDS_OPTS_P_OPT),
    FDS_OPTS_NESTED(OUTPUT_LIST, "outputs",   args_outputs, 0),
    FDS_OPTS_END
};

/**
 * \brief Check if a given string is a valid IPv4/IPv6 address
 * \param[in] ip_addr Address to check
 * \return True or false
 */
bool
Config::check_ip(const std::string &ip_addr)
{
    in_addr ipv4;
    in6_addr ipv6;

    return (inet_pton(AF_INET, ip_addr.c_str(), &ipv4) == 1
        || inet_pton(AF_INET6, ip_addr.c_str(), &ipv6) == 1);
}

/**
 * \brief Check one of 2 expected options
 *
 * \param[in] elem      XML element to check (just for exception purposes)
 * \param[in] value     String to check
 * \param[in] val_true  True string
 * \param[in] val_false False string
 * \throw invalid_argument if the value doesn't match any expected string
 * \return True of false
 */
bool
Config::check_or(const std::string &elem, const char *value, const std::string &val_true,
    const std::string &val_false)
{
    if (strcasecmp(value, val_true.c_str()) == 0) {
        return true;
    }

    if (strcasecmp(value, val_false.c_str()) == 0) {
        return false;
    }

    // Error
    throw std::invalid_argument("Unexpected parameter of the element <" + elem + "> (expected '"
        + val_true + "' or '" + val_false + "')");
}

/**
 * \brief Parse "print" output parameters
 *
 * Successfully parsed output is added to the vector of outputs
 * \param[in] print Parsed XML context
 * \throw invalid_argument or runtime_error
 */
void
Config::parse_print(fds_xml_ctx_t *print)
{
    struct cfg_print output;

    const struct fds_xml_cont *content;
    while (fds_xml_next(print, &content) != FDS_EOC) {
        switch (content->id) {
        case PRINT_NAME:
            assert(content->type == FDS_OPTS_T_STRING);
            output.name = content->ptr_string;
            break;
        default:
            throw std::invalid_argument("Unexpected element within <print>!");
        }
    }

    if (output.name.empty()) {
        throw std::runtime_error("Name of a <print> output must be defined!");
    }

    outputs.prints.push_back(output);
}

/**
 * \brief Parse "server" output parameters
 *
 * Successfully parsed output is added to the vector of outputs
 * \param[in] server Parsed XML context
 * \throw invalid_argument or runtime_error
 */
void
Config::parse_server(fds_xml_ctx_t *server)
{
    struct cfg_server output;
    output.port = 0;
    output.blocking = false;

    const struct fds_xml_cont *content;
    while (fds_xml_next(server, &content) != FDS_EOC) {
        switch (content->id) {
        case SERVER_NAME:
            assert(content->type == FDS_OPTS_T_STRING);
            output.name = content->ptr_string;
            break;
        case SERVER_PORT:
            assert(content->type == FDS_OPTS_T_UINT);
            if (content->val_uint > UINT16_MAX || content->val_uint == 0) {
                throw std::invalid_argument("Invalid port number of a <server> output!");
            }

            output.port = static_cast<uint16_t>(content->val_uint);
            break;
        case SERVER_BLOCK:
            assert(content->type == FDS_OPTS_T_BOOL);
            output.blocking = content->val_bool;
            break;
        default:
            throw std::invalid_argument("Unexpected element within <server>!");
        }
    }

    if (output.name.empty()) {
        throw std::runtime_error("Name of a <server> output must be defined!");
    }

    outputs.servers.push_back(output);
}

/**
 * \brief Parse "send" output parameters
 *
 * Successfully parsed output is added to the vector of outputs
 * \param[in] send Parsed XML context
 * \throw invalid_argument or runtime_error
 */
void
Config::parse_send(fds_xml_ctx_t *send)
{
    struct cfg_send output;
    output.proto = cfg_send::proto::PROTO_UDP;
    output.addr = "127.0.0.1";
    output.port = 4739;

    const struct fds_xml_cont *content;
    while (fds_xml_next(send, &content) != FDS_EOC) {
        switch (content->id) {
        case SEND_NAME:
            assert(content->type == FDS_OPTS_T_STRING);
            output.name = content->ptr_string;
            break;
        case SEND_IP:
            assert(content->type == FDS_OPTS_T_STRING);
            output.addr = content->ptr_string;
            break;
        case SEND_PORT:
            assert(content->type == FDS_OPTS_T_UINT);
            if (content->val_uint > UINT16_MAX || content->val_uint == 0) {
                throw std::invalid_argument("Invalid port number of a <send> output!");
            }

            output.port = static_cast<uint16_t>(content->val_uint);
            break;
        case SEND_PROTO:
            assert(content->type == FDS_OPTS_T_STRING);
            output.proto = check_or("protocol", content->ptr_string, "UDP", "TCP")
                ? cfg_send::proto::PROTO_UDP : cfg_send::proto::PROTO_TCP;
            break;
        default:
            throw std::invalid_argument("Unexpected element within <send>!");
        }
    }

    if (output.name.empty()) {
        throw std::runtime_error("Name of a <send> output must be defined!");
    }

    if (output.addr.empty() || !check_ip(output.addr)) {
        throw std::runtime_error("Value of the element <ip> of the output <send> '" + output.name
            + "' is not a valid IPv4/IPv6 address");
    }

    outputs.sends.push_back(output);
}

/**
 * \brief Parse "file" output parameters
 *
 * Successfully parsed output is added to the vector of outputs
 * \param[in] file Parsed XML context
 * \throw invalid_argument or runtime_error
 */
void
Config::parse_file(fds_xml_ctx_t *file)
{
    // Set defaults
    struct cfg_file output;
    output.window_align = true;
    output.window_size = 300;

    const struct fds_xml_cont *content;
    while (fds_xml_next(file, &content) != FDS_EOC) {
        switch (content->id) {
        case FILE_NAME:
            assert(content->type == FDS_OPTS_T_STRING);
            output.name = content->ptr_string;
            break;
        case FILE_PATH:
            assert(content->type == FDS_OPTS_T_STRING);
            output.path_pattern = content->ptr_string;
            break;
        case FILE_PREFIX:
            assert(content->type == FDS_OPTS_T_STRING);
            output.prefix = content->ptr_string;
            break;
        case FILE_WINDOW:
            assert(content->type == FDS_OPTS_T_UINT);
            if (content->val_uint > UINT32_MAX) {
                throw std::invalid_argument("Windows size must be between 0.."
                    + std::to_string(UINT32_MAX) + "!");
            }

            output.window_size = static_cast<uint32_t>(content->val_uint);
            break;
        case FILE_ALIGN:
            assert(content->type == FDS_OPTS_T_BOOL);
            output.window_align = content->val_bool;
            break;
        default:
            throw std::invalid_argument("Unexpected element within <file>!");
        }
    }

    if (output.name.empty()) {
        throw std::runtime_error("Name of a <file> output must be defined!");
    }

    if (output.path_pattern.empty()) {
        throw std::runtime_error("Element <path> of the output '" + output.name
            + "' must be defined!");
    }

    outputs.files.push_back(output);
}

/**
 * \brief Parse list of outputs
 * \param[in] outputs Parsed XML context
 * \throw invalid_argument or runtime_error
 */
void
Config::parse_outputs(fds_xml_ctx_t *outputs)
{
    const struct fds_xml_cont *content;
    while (fds_xml_next(outputs, &content) != FDS_EOC) {
        assert(content->type == FDS_OPTS_T_CONTEXT);
        switch (content->id) {
        case OUTPUT_PRINT:
            parse_print(content->ptr_ctx);
            break;
        case OUTPUT_SEND:
            parse_send(content->ptr_ctx);
            break;
        case OUTPUT_FILE:
            parse_file(content->ptr_ctx);
            break;
        case OUTPUT_SERVER:
            parse_server(content->ptr_ctx);
            break;
        default:
            throw std::invalid_argument("Unexpected element within <outputs>!");
        }
    }
}

/**
 * \brief Parse all parameters
 *
 * This is the main parser function that process all format specifiers and parser all
 * specifications of outputs.
 * \param[in] params Initialized XML parser context of the root element
 * \throw invalid_argument or runtime_error
 */
void
Config::parse_params(fds_xml_ctx_t *params)
{
    const struct fds_xml_cont *content;
    while (fds_xml_next(params, &content) != FDS_EOC) {
        switch (content->id) {
        case FMT_TFLAGS:   // Format TCP flags
            assert(content->type == FDS_OPTS_T_STRING);
            format.tcp_flags = check_or("tcpFlags", content->ptr_string, "formatted", "raw");
            break;
        case FMT_TIMESTAMP: // Format timestamp
            assert(content->type == FDS_OPTS_T_STRING);
            format.timestamp = check_or("timestamp", content->ptr_string, "formatted", "unix");
            break;
        case FMT_PROTO:    // Format protocols
            assert(content->type == FDS_OPTS_T_STRING);
            format.proto = check_or("protocol", content->ptr_string, "formatted", "raw");
            break;
        case FMT_UNKNOWN:  // Ignore unknown
            assert(content->type == FDS_OPTS_T_BOOL);
            format.ignore_unknown = content->val_bool;
            break;
        case FMT_OPTIONS:  // Ignore Options Template records
            assert(content->type == FDS_OPTS_T_BOOL);
            format.ignore_options = content->val_bool;
            break;
        case FMT_NONPRINT: // Print non-printable characters
            assert(content->type == FDS_OPTS_T_BOOL);
            format.white_spaces = content->val_bool;
            break;
        case OUTPUT_LIST: // List of output plugin
            assert(content->type == FDS_OPTS_T_CONTEXT);
            parse_outputs(content->ptr_ctx);
            break;
        default:
            throw std::invalid_argument("Unexpected element within <params>!");
        }
    }
}

/**
 * \brief Reset all parameters to default values
 */
void
Config::default_set()
{
    format.proto = true;
    format.tcp_flags = true;
    format.timestamp = true;
    format.white_spaces = true;
    format.ignore_unknown = true;
    format.ignore_options = true;

    outputs.prints.clear();
    outputs.files.clear();
    outputs.servers.clear();
    outputs.sends.clear();
}

/**
 * \brief Check if parsed configuration is valid
 * \throw invalid_argument if the configuration is not valid
 */
void
Config::check_validity()
{
    size_t output_cnt = 0;
    output_cnt += outputs.prints.size();
    output_cnt += outputs.servers.size();
    output_cnt += outputs.sends.size();
    output_cnt += outputs.files.size();
    if (output_cnt == 0) {
        throw std::invalid_argument("At least one output must be defined!");
    }

    if (outputs.prints.size() > 1) {
        throw std::invalid_argument("Multiple <print> outputs are not allowed!");
    }

    // Check collision of output names
    std::set<std::string> names;
    auto check_and_add = [&](const std::string &name) {
        if (names.find(name) != names.end()) {
            throw std::invalid_argument("Multiple outputs with the same name '" + name + "'!");
        }
        names.insert(name);
    };
    for (const auto &print : outputs.prints) {
        check_and_add(print.name);
    }
    for (const auto &send : outputs.sends) {
        check_and_add(send.name);
    }
    for (const auto &server : outputs.servers) {
        check_and_add(server.name);
    }
    for (const auto &file : outputs.files) {
        check_and_add(file.name);
    }
}

Config::Config(const char *params)
{
    default_set();

    // Create XML parser
    std::unique_ptr<fds_xml_t, decltype(&fds_xml_destroy)> xml(fds_xml_create(), &fds_xml_destroy);
    if (!xml) {
        throw std::runtime_error("Failed to create an XML parser!");
    }

    if (fds_xml_set_args(xml.get(), args_params) != FDS_OK) {
        throw std::runtime_error("Failed to parse the description of an XML document!");
    }

    fds_xml_ctx_t *params_ctx = fds_xml_parse_mem(xml.get(), params, true);
    if (!params_ctx) {
        std::string err = fds_xml_last_err(xml.get());
        throw std::runtime_error("Failed to parse the configuration: " + err);
    }

    // Parse parameters and check configuration
    try {
        parse_params(params_ctx);
        check_validity();
    } catch (std::exception &ex) {
        throw std::runtime_error("Failed to parse the configuration: " + std::string(ex.what()));
    }
}

Config::~Config()
{
    // Nothing to do
}

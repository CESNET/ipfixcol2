/**
 * \file src/plugins/output/json/src/Config.hpp
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Configuration of JSON output plugin (header file)
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

#ifndef JSON_CONFIG_H
#define JSON_CONFIG_H

#include <string>
#include <vector>
#include <ipfixcol2.h>

/** Configuration of output format                                                               */
struct cfg_format {
    /** TCP flags format - true (formatted), false (raw)                                         */
    bool tcp_flags;
    /** Timestamp format - true (formatted), false (UNIX)                                        */
    bool timestamp;
    /** Protocol format  - true (formatted), false (raw)                                         */
    bool proto;
    /** Skip unknown elements                                                                    */
    bool ignore_unknown;
    /** Convert white spaces in string (do not skip)                                             */
    bool white_spaces;
    /** Ignore Options Template records                                                          */
    bool ignore_options;
    /** Use only numeric identifiers of Information Elements                                     */
    bool numeric_names;
    /** Split biflow records                                                                     */
    bool split_biflow;
};

/** Output configuration base structure                                                          */
struct cfg_output {
    /** Plugin identification                                                                    */
    std::string name;
};

/** Configuration of printer to standard output                                                  */
struct cfg_print : cfg_output {
    // Nothing more
};

/** Configuration of sender                                                                      */
struct cfg_send : cfg_output {
    /** Remote IPv4/IPv6 address                                                                 */
    std::string addr;
    /** Destination port                                                                         */
    uint16_t port;
    /** Blocking communication                                                                   */
    bool blocking;
    /** Transport Protocol                                                                       */
    enum {
        SEND_PROTO_UDP,
        SEND_PROTO_TCP
    } proto; /**< Communication protocol                                                         */
};

/** Configuration of TCP server                                                                  */
struct cfg_server : cfg_output {
    /** Destination port                                                                         */
    uint16_t port;
    /** Blocking communication                                                                   */
    bool blocking;
};

/** Configuration of file writer                                                                 */
struct cfg_file : cfg_output {
    /** Path pattern                                                                             */
    std::string path_pattern;
    /** File prefix                                                                              */
    std::string prefix;
    /** Window size (0 == disabled)                                                              */
    uint32_t window_size;
    /** Enable/disable window alignment                                                          */
    bool window_align;
};

/** Parsed configuration of an instance                                                          */
class Config {
private:
    bool check_ip(const std::string &ip_addr);
    bool check_or(const std::string &elem, const char *value, const std::string &val_true,
        const std::string &val_false);
    void check_validity();
    void default_set();
    void parse_print(fds_xml_ctx_t *print);
    void parse_server(fds_xml_ctx_t *server);
    void parse_send(fds_xml_ctx_t *send);
    void parse_file(fds_xml_ctx_t *file);
    void parse_outputs(fds_xml_ctx_t *outputs);
    void parse_params(fds_xml_ctx_t *params);

public:
    /** Transformation format                                                                    */
    struct cfg_format format;

    struct {
        /** Printers                                                                             */
        std::vector<struct cfg_print> prints;
        /** Senders                                                                              */
        std::vector<struct cfg_send> sends;
        /** File writers                                                                         */
        std::vector<struct cfg_file> files;
        /** Servers                                                                              */
        std::vector<struct cfg_server> servers;
    } outputs; /**< Outputs                                                                      */

    /**
     * \brief Create a new configuration
     * \param[in] params XML configuration of JSON plugin
     * \throw runtime_error in case of invalid configuration
     */
    Config(const char *params);
    /**
     * \brief Configuration destructor
     */
    ~Config();
};

#endif // JSON_CONFIG_H

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

#include <map>
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
    /** Converter octetArray type as unsigned integer (only if field size <= 8)                  */
    bool octets_as_uint;
    /** Convert white spaces in string (do not skip)                                             */
    bool white_spaces;
    /** Add detailed information about each record                                               */
    bool detailed_info;
    /** Ignore Options Template records                                                          */
    bool ignore_options;
    /** Use only numeric identifiers of Information Elements                                     */
    bool numeric_names;
    /** Split biflow records                                                                     */
    bool split_biflow;
    /** Add template records                                                                     */
    bool template_info;
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

enum class calg {
    NONE, ///< Do not use compression
    GZIP  ///< GZIP compression
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
    /** Compression algorithm                                                                    */
    calg m_calg;
};

/** Configuration of kafka output                                                                */
struct cfg_kafka : cfg_output {
    /// Comma separated list of IP[:Port]
    std::string brokers;
    /// Produced topic
    std::string topic;
    /// Partition to which data should be send
    int32_t partition;
    /// Broker version fallback (empty or X.X.X.X)
    std::string broker_fallback;
    /// Block conversion if sender buffer is full
    bool blocking;
    /// Add default properties for librdkafka
    bool perf_tuning;

    /// Additional librdkafka properties (might overwrite common parameters)
    std::map<std::string, std::string> properties;
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
    void parse_kafka(fds_xml_ctx_t *kafka);
    void parse_kafka_property(struct cfg_kafka &kafka, fds_xml_ctx_t *property);
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
        /** Kafkas                                                                                */
        std::vector<struct cfg_kafka> kafkas;
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

/**
 * \brief Parse application version (i.e. A.B.C.D)
 *
 * \note At least major and minor version must be specified. Undefined sub-versions are set to zero.
 * \param[in]  str     Version string
 * \param[out] version Parsed version
 * \return #IPX_OK on success
 * \return #IPX_ERR_FORMAT if the version string is malformed
 */
int
parse_version(const std::string &str, int version[4]);

#endif // JSON_CONFIG_H

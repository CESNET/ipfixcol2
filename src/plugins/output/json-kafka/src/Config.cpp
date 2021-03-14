/**
 * \file src/plugins/output/json-kafka/src/Config.cpp
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Configuration of JSON output plugin (source file)
 * \date 2018-2020
 */

/* Copyright (C) 2018-2020 CESNET, z.s.p.o.
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

#include <cstdio>
#include <memory>
#include <set>
#include <sstream>

#include <libfds.h>
#include <inttypes.h>
#include <arpa/inet.h>
#include <librdkafka/rdkafka.h>

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
    FMT_OCTETASUINT,   /**< OctetArray as unsigned integer  */
    FMT_NUMERIC,       /**< Use numeric names               */
    FMT_BFSPLIT,       /**< Split biflow                    */
    FMT_DETAILEDINFO,  /**< Detailed information            */
    FMT_TMPLTINFO,     /**< Template records                */
    // Common output
    OUTPUT_LIST,       /**< List of output types            */
    OUTPUT_KAFKA,      /**< Store to Kafka                  */
    // Kafka output
    KAFKA_NAME,        /**< Name of the output              */
    KAFKA_BROKERS,     /**< List of brokers                 */
    KAFKA_TOPIC,       /**< Topic                           */
    KAFKA_PARTION,     /**< Producer partition              */
    KAFKA_BVERSION,    /**< Broker fallback version         */
    KAFKA_BLOCKING,    /**< Block when queue is full        */
    KAFKA_PERF_TUN,    /**< Add performance tuning options  */
    KAFKA_PROPERTY,    /**< Additional librdkafka property  */
    KAFKA_PROP_KEY,    /**< Property key                    */
    KAFKA_PROP_VALUE,  /**< Property value                  */
};

/** Definition of the \<property\> of \<kafka\> node  */
static const struct fds_xml_args args_kafka_prop[] = {
    FDS_OPTS_ELEM(KAFKA_PROP_KEY,  "key",   FDS_OPTS_T_STRING, 0),
    FDS_OPTS_ELEM(KAFKA_PROP_VALUE,"value", FDS_OPTS_T_STRING, 0),
    FDS_OPTS_END
};

/** Definition of the \<kafka\> node  */
static const struct fds_xml_args args_kafka[] = {
    FDS_OPTS_ELEM(KAFKA_NAME,       "name",          FDS_OPTS_T_STRING, 0),
    FDS_OPTS_ELEM(KAFKA_BROKERS,    "brokers",       FDS_OPTS_T_STRING, 0),
    FDS_OPTS_ELEM(KAFKA_TOPIC,      "topic",         FDS_OPTS_T_STRING, 0),
    FDS_OPTS_ELEM(KAFKA_PARTION,    "partition",     FDS_OPTS_T_STRING, FDS_OPTS_P_OPT),
    FDS_OPTS_ELEM(KAFKA_BVERSION,   "brokerVersion", FDS_OPTS_T_STRING, FDS_OPTS_P_OPT),
    FDS_OPTS_ELEM(KAFKA_BLOCKING,   "blocking",      FDS_OPTS_T_BOOL,   FDS_OPTS_P_OPT),
    FDS_OPTS_ELEM(KAFKA_PERF_TUN,   "performanceTuning", FDS_OPTS_T_BOOL, FDS_OPTS_P_OPT),
    FDS_OPTS_NESTED(KAFKA_PROPERTY, "property", args_kafka_prop, FDS_OPTS_P_OPT | FDS_OPTS_P_MULTI),
    FDS_OPTS_END
};

/** Definition of the \<outputs\> node  */
static const struct fds_xml_args args_outputs[] = {
    FDS_OPTS_NESTED(OUTPUT_KAFKA,  "kafka",  args_kafka,  FDS_OPTS_P_OPT | FDS_OPTS_P_MULTI),
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
    FDS_OPTS_ELEM(FMT_NUMERIC,   "numericNames",     FDS_OPTS_T_BOOL, FDS_OPTS_P_OPT),
    FDS_OPTS_ELEM(FMT_OCTETASUINT, "octetArrayAsUint", FDS_OPTS_T_BOOL, FDS_OPTS_P_OPT),
    FDS_OPTS_ELEM(FMT_BFSPLIT,   "splitBiflow",      FDS_OPTS_T_BOOL, FDS_OPTS_P_OPT),
    FDS_OPTS_ELEM(FMT_DETAILEDINFO,  "detailedInfo", FDS_OPTS_T_BOOL, FDS_OPTS_P_OPT),
    FDS_OPTS_ELEM(FMT_TMPLTINFO, "templateInfo", FDS_OPTS_T_BOOL, FDS_OPTS_P_OPT),
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
 * \brief Parser "kafka property" parameter
 *
 * \param[in,out] kafka    Configuration of Kafka instance (properties will be filled here)
 * \param[in]     property XML context
 * \throw invalid_argument or runtime_error
 */
void
Config::parse_kafka_property(struct cfg_kafka &kafka, fds_xml_ctx_t *property)
{
    std::string key, value;

    const struct fds_xml_cont *content;
    while (fds_xml_next(property, &content) != FDS_EOC) {
        switch (content->id) {
        case KAFKA_PROP_KEY:
            assert(content->type == FDS_OPTS_T_STRING);
            key = content->ptr_string;
            break;
        case KAFKA_PROP_VALUE:
            assert(content->type == FDS_OPTS_T_STRING);
            value = content->ptr_string;
            break;
        default:
            throw std::invalid_argument("Unexpected element within <property>!");
        }
    }

    if (key.empty()) {
        throw std::invalid_argument("Property key of a <kafka> output cannot be empty!");
    }

    kafka.properties.emplace(key, value);
}

/**
 * \brief Parse "kafka" output parameters
 *
 * Successfully parsed output is added to the vector of outputs
 * \param[in] kafka Parsed XML context
 * \throw invalid_argument or runtime_error
 */
void
Config::parse_kafka(fds_xml_ctx_t *kafka)
{
    // Prepare default values
    struct cfg_kafka output;
    output.partition = RD_KAFKA_PARTITION_UA;
    output.blocking = false;
    output.perf_tuning = true;

    // For partition parser
    int32_t value;
    char aux;

    const struct fds_xml_cont *content;
    while (fds_xml_next(kafka, &content) != FDS_EOC) {
        switch (content->id) {
        case KAFKA_NAME:
            assert(content->type == FDS_OPTS_T_STRING);
            output.name = content->ptr_string;
            break;
        case KAFKA_BROKERS:
            assert(content->type == FDS_OPTS_T_STRING);
            output.brokers = content->ptr_string;
            break;
        case KAFKA_TOPIC:
            assert(content->type == FDS_OPTS_T_STRING);
            output.topic = content->ptr_string;
            break;
        case KAFKA_PARTION:
            assert(content->type == FDS_OPTS_T_STRING);
            if (strcasecmp(content->ptr_string, "unassigned") == 0) {
                output.partition = RD_KAFKA_PARTITION_UA;
                break;
            }

            if (sscanf(content->ptr_string, "%" SCNi32 "%c", &value, &aux) != 1 || value < 0) {
                throw std::invalid_argument("Invalid partition number of a <kafka> output!");
            }
            output.partition = value;
            break;
        case KAFKA_BVERSION:
            assert(content->type == FDS_OPTS_T_STRING);
            output.broker_fallback = content->ptr_string;
            break;
        case KAFKA_BLOCKING:
            assert(content->type == FDS_OPTS_T_BOOL);
            output.blocking = content->val_bool;
            break;
        case KAFKA_PERF_TUN:
            assert(content->type == FDS_OPTS_T_BOOL);
            output.perf_tuning = content->val_bool;
            break;
        case KAFKA_PROPERTY:
            assert(content->type == FDS_OPTS_T_CONTEXT);
            parse_kafka_property(output, content->ptr_ctx);
            break;
        default:
            throw std::invalid_argument("Unexpected element within <kafka>!");
        }
    }

    // Check validity
    if (output.brokers.empty()) {
        throw std::invalid_argument("List of <kafka> brokers must be specified!");
    }
    if (output.topic.empty()) {
        throw std::invalid_argument("Topic of <kafka> output must be specified!");
    }
    if (!output.broker_fallback.empty()) {
        // Try to check if version string is valid version (at least expect major + minor version)
        int version[4];
        if (parse_version(output.broker_fallback, version) != IPX_OK) {
            throw std::invalid_argument("Broker version of a <kafka> output is not invalid!");
        }
    }

    outputs.kafkas.push_back(output);
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
        case OUTPUT_KAFKA:
            parse_kafka(content->ptr_ctx);
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
        case FMT_NUMERIC:  // Use only numeric identifiers
            assert(content->type == FDS_OPTS_T_BOOL);
            format.numeric_names = content->val_bool;
            break;
        case FMT_OCTETASUINT:
            assert(content->type == FDS_OPTS_T_BOOL);
            format.octets_as_uint = content->val_bool;
            break;
        case FMT_BFSPLIT:  // Split biflow records
            assert(content->type == FDS_OPTS_T_BOOL);
            format.split_biflow = content->val_bool;
            break;
        case FMT_DETAILEDINFO: // Add detailed information about each record
            assert(content->type == FDS_OPTS_T_BOOL);
            format.detailed_info = content->val_bool;
            break;
        case FMT_TMPLTINFO: // Add template records
            assert(content->type == FDS_OPTS_T_BOOL);
            format.template_info = content->val_bool;
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
    format.octets_as_uint = true;
    format.numeric_names = false;
    format.split_biflow = false;
    format.detailed_info = false;
    format.template_info = false;

    outputs.kafkas.clear();
}

/**
 * \brief Check if parsed configuration is valid
 * \throw invalid_argument if the configuration is not valid
 */
void
Config::check_validity()
{
    size_t output_cnt = 0;
    output_cnt += outputs.kafkas.size();
    if (output_cnt == 0) {
        throw std::invalid_argument("At least one output must be defined!");
    }

    // Check collision of output names
    std::set<std::string> names;
    auto check_and_add = [&](const std::string &name) {
        if (names.find(name) != names.end()) {
            throw std::invalid_argument("Multiple outputs with the same name '" + name + "'!");
        }
        names.insert(name);
    };

    for (const auto &kafka : outputs.kafkas) {
        check_and_add(kafka.name);
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

int
parse_version(const std::string &str, int version[4])
{
    static const int FIELDS_MIN = 2;
    static const int FIELDS_MAX = 4;

    // Parse the required version
    std::istringstream parser(str);
    for (int i = 0; i < FIELDS_MAX; ++i) {
        version[i] = 0;
    }

    int idx;
    for (idx = 0; idx < FIELDS_MAX && !parser.eof(); idx++) {
        if (idx != 0 && parser.get() != '.') {
            return IPX_ERR_FORMAT;
        }

        parser >> version[idx];
        if (parser.fail() || version[idx] < 0) {
            return IPX_ERR_FORMAT;
        }
    }

    if (!parser.eof() || idx < FIELDS_MIN) {
        return IPX_ERR_FORMAT;
    }

    return IPX_OK;
}

/**
 * \file src/plugins/output/json/src/Kafka.cpp
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Kafka output (source file)
 * \date 2020
 */

/* Copyright (C) 2020 CESNET, z.s.p.o.
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

#include "Config.hpp"
#include "Kafka.hpp"
#include <stdexcept>

static void
dr_msg_cb(rd_kafka_t *rk, const rd_kafka_message_t *rkmessage, void *opaque)
{
    if (rkmessage->err) {
        fprintf(stderr, "%% Message delivery failed: %s\n", rd_kafka_err2str(rkmessage->err));
    } else {
        fprintf(stderr, "%% Message delivered (%zd bytes, partition %" PRId32 ")\n",
            rkmessage->len, rkmessage->partition);
    }
}

/**
 * \brief Class constructor
 * \param[in] cfg Kafka configuration
 * \param[in] ctx Instance context
 */
Kafka::Kafka(const struct cfg_kafka &cfg, ipx_ctx_t *ctx)
    : Output(cfg.name, ctx), m_partition(cfg.partition)
{
    char err_str[512];
    const size_t err_size = sizeof(err_str);
    uniq_config kafka_cfg(nullptr, &rd_kafka_conf_destroy);

    m_produce_flags = RD_KAFKA_MSG_F_COPY;
    if (cfg.blocking) {
        m_produce_flags |= RD_KAFKA_MSG_F_BLOCK;
    }

    // Create configuration
    m_params.clear();
    m_params["bootstrap.servers"] = cfg.brokers;
    // TODO: add default size ....

    if (!cfg.broker_fallback.empty()) {
        // Some parameters must be configured based on library version
        // https://github.com/edenhill/librdkafka/blob/master/INTRODUCTION.md#compatibility
        int lib_ver[4] = {0};
        int broker_ver[4] = {};
        const std::string lib_ver_str = rd_kafka_version_str();

        if (parse_version(std::string(lib_ver_str), lib_ver) != IPX_OK) {
            throw std::runtime_error("Unable to parse librdkafka version '" + lib_ver_str + "'");
        }
        if (parse_version(cfg.broker_fallback, broker_ver) != IPX_OK) {
            throw std::runtime_error("Unable to parse broker version '" + cfg.broker_fallback + "'");
        }

        if ((broker_ver[0] > 0 || (broker_ver[0] == 0 && broker_ver[1] >= 10)) && lib_ver[0] < 1) {
            // Broker version >= 0.10.0.0 && librdkafka < v1.0.0
            m_params["api.version.request"] = "true";
            m_params["api.version.fallback.ms"] = "0";
        } else if ((broker_ver[0] == 0) && (broker_ver[1] == 8 || broker_ver[1] == 9)) {
            // Broker version 0.9.x and 0.8.x
            m_params["api.version.request"] = "false";
            m_params["broker.version.fallback"] = cfg.broker_fallback;
        }
    }

    // Add user specified params... (default parameters might be overwritten)
    for (const auto &prop : cfg.properties) {
        m_params[prop.first] = prop.second;
    }

    // Prepare Kafka configuration object
    kafka_cfg.reset(rd_kafka_conf_new());
    if (!kafka_cfg) {
        throw std::runtime_error("rd_kafka_conf_new() failed!");
    }

    for (const auto &param : m_params) {
        // Set all parameters
        rd_kafka_conf_res_t res;
        const char *p_name = param.first.c_str();
        const char *p_value = param.second.c_str();
        IPX_CTX_INFO(ctx, "Setting parameters: '%s'='%s'", p_name, p_value);

        res = rd_kafka_conf_set(kafka_cfg.get(), p_name, p_value, err_str, err_size);
        if (res != RD_KAFKA_CONF_OK) {
            size_t err_len = strnlen(err_str, err_size);
            //
            throw std::runtime_error("Unable to set '" + param.first + "'='" + param.second + "' "
                + "(rd_kafka_conf_set() failed: '" + std::string(err_str, err_len) + "')");
        }
    }

    rd_kafka_conf_set_dr_msg_cb(kafka_cfg.get(), dr_msg_cb);

    // Create the Kafka connector
    m_kafka.reset(rd_kafka_new(RD_KAFKA_PRODUCER, kafka_cfg.get(), err_str, err_size));
    if (!m_kafka) {
        size_t err_len = strnlen(err_str, err_size);
        throw std::runtime_error("Failed to create Kafka producer: " + std::string(err_str, err_len));
    }
    kafka_cfg.release(); // Ownership has been successfully passed to the kafka

    // Create the topic
    m_topic.reset(rd_kafka_topic_new(m_kafka.get(), cfg.topic.c_str(), nullptr));
    if (!m_topic) {
        rd_kafka_resp_err_t err_code = rd_kafka_last_error();
        const char *err_msg = rd_kafka_err2str(err_code);
        throw std::runtime_error("rd_kafka_topic_new() failed: " + std::string(err_msg));
    }
}

/** Destructor */
Kafka::~Kafka()
{
    // Wait for outstanding messages (five seconds timeout)
    if (rd_kafka_flush(m_kafka.get(), 5000) == RD_KAFKA_RESP_ERR__TIMED_OUT) {
        IPX_CTX_WARNING(_ctx, "Some outstanding Kafka requests were NOT completed due to timeout!");
    }

    // Destruction must be called in this order!
    m_topic.reset(nullptr);
    m_kafka.reset(nullptr);
}

/**
 * \brief Send a JSON record
 * \param[in] str JSON Record to send
 * \param[in] len Size of the record
 * \return Always #IPX_OK
 */
int
Kafka::process(const char *str, size_t len)
{
    int rc = rd_kafka_produce(m_topic.get(), m_partition, m_produce_flags,
        reinterpret_cast<void *>(const_cast<char *>(str)), len,   // Payload and length
        NULL, 0, // Optional key and its length
        NULL);    // Message opaque
    if (rc != 0) {
        rd_kafka_resp_err_t err_code = rd_kafka_last_error();
        const char *err_msg = rd_kafka_err2str(err_code);
        IPX_CTX_WARNING(_ctx, "rd_kafka_produce() failed: %s", err_msg);
    }

    rd_kafka_poll(m_kafka.get(), 0);
    return IPX_OK;
}

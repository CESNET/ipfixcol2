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
#include <pthread.h>
#include <stdexcept>

/// Optimized value for "batch.num.messages"
#define PERF_BATCH_NUM_MSG "60000"
/// Optimized value for "queue.buffering.max.ms"
#define PERF_BUFFERING_MS  "200"

/**
 * \brief Class constructor
 * \param[in] cfg Kafka configuration
 * \param[in] ctx Instance context
 */
Kafka::Kafka(const struct cfg_kafka &cfg, ipx_ctx_t *ctx)
    : Output(cfg.name, ctx), m_partition(cfg.partition)
{
    IPX_CTX_DEBUG(_ctx, "Initialization of Kafka connector in progress...", '\0');
    IPX_CTX_INFO(_ctx, "The plugin was built against librdkafka %X, now using %X",
        BUILD_VERSION, rd_kafka_version());

    char err_str[512];
    const size_t err_size = sizeof(err_str);
    uniq_config kafka_cfg(nullptr, &rd_kafka_conf_destroy);
    clock_gettime(CLOCK_MONOTONIC, &m_err_ts);
    m_thread.reset(new thread_ctx_t);

    m_produce_flags = RD_KAFKA_MSG_F_COPY;
    if (cfg.blocking) {
        m_produce_flags |= RD_KAFKA_MSG_F_BLOCK;
    }

    // Prepare Kafka configuration object
    kafka_cfg.reset(rd_kafka_conf_new());
    if (!kafka_cfg) {
        throw std::runtime_error("rd_kafka_conf_new() failed!");
    }

    prepare_params(cfg, m_params);
    for (const auto &param : m_params) {
        // Set all parameters
        rd_kafka_conf_res_t res;
        const char *p_name = param.first.c_str();
        const char *p_value = param.second.c_str();
        IPX_CTX_INFO(ctx, "Setting Kafka parameter: '%s'='%s'", p_name, p_value);

        res = rd_kafka_conf_set(kafka_cfg.get(), p_name, p_value, err_str, err_size);
        if (res != RD_KAFKA_CONF_OK) {
            size_t err_len = strnlen(err_str, err_size);
            //
            throw std::runtime_error("Unable to set '" + param.first + "'='" + param.second + "' "
                + "(rd_kafka_conf_set() failed: '" + std::string(err_str, err_len) + "')");
        }
    }

    // Set callbacks for Kafka events (they will be called by the poller thread)
    rd_kafka_conf_set_dr_msg_cb(kafka_cfg.get(), thread_cb_delivery);
    rd_kafka_conf_set_opaque(kafka_cfg.get(), m_thread.get());

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

    // Start poller thread
    m_thread->stop = false;
    m_thread->ctx = ctx;
    m_thread->kafka = m_kafka.get();
    if (pthread_create(&m_thread->thread, nullptr, &thread_polling, m_thread.get()) != 0) {
        throw std::runtime_error("Failed to start polling thread for Kafka events");
    }

    IPX_CTX_DEBUG(_ctx, "Kafka connector successfully initialized!", '\0')
}

/** Destructor */
Kafka::~Kafka()
{
    IPX_CTX_DEBUG(_ctx, "Destruction of Kafka connector in progress...", '\0');

    // Stop poller thread
    m_thread->stop = true;
    int rc = pthread_join(m_thread->thread, nullptr);
    if (rc != 0) {
        const char *err_msg;
        ipx_strerror(rc, err_msg);
        IPX_CTX_WARNING(_ctx, "pthread_join() failed: %s", err_msg);
    }

    // Wait for outstanding messages (five seconds timeout)
    if (rd_kafka_flush(m_kafka.get(), FLUSH_TIMEOUT) == RD_KAFKA_RESP_ERR__TIMED_OUT) {
        IPX_CTX_WARNING(_ctx, "Some outstanding Kafka requests were NOT completed due to timeout!");
    }

    // Destruction must be called in this order!
    m_topic.reset(nullptr);
    m_kafka.reset(nullptr);

    IPX_CTX_DEBUG(_ctx, "Destruction of Kafka connector completed!", '\0');
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
        // Payload and length (without tailing new-line character)
        reinterpret_cast<void *>(const_cast<char *>(str)), len - 1,
        NULL, 0,  // Optional key and its length
        NULL);    // Message opaque
    if (rc == 0 && m_err_cnt == 0) {
        // No error and previous errors
        return IPX_OK;
    }

    // Get the error (it probably uses errno so it should go first)
    rd_kafka_resp_err_t err_code = rd_kafka_last_error();

    // The following code aggregates produce() errors
    struct timespec ts_now;
    clock_gettime(CLOCK_MONOTONIC, &ts_now);

    if (rc != 0) {
        // An error has occurred
        if (err_code != m_err_type) {
            // Different error then previously - print the previous one now
            produce_error(ts_now);
            m_err_type = err_code;
        }

        m_err_cnt++;
    }

    if (difftime(ts_now.tv_sec, m_err_ts.tv_sec) >= 1.0) {
        produce_error(ts_now);
    }

    return IPX_OK;
}

/**
 * @brief Print the aggregated error and reset the counter
 * @param[in] ts_now Current timestamp
 */
void
Kafka::produce_error(struct timespec ts_now)
{
    if (m_err_type == RD_KAFKA_RESP_ERR_NO_ERROR || m_err_cnt == 0) {
        return;
    }

    const char *err_msg = rd_kafka_err2str(m_err_type);
    IPX_CTX_ERROR(_ctx, "rd_kafka_produce() failed: %s (%" PRIu64 "x)", err_msg, m_err_cnt);

    m_err_ts = ts_now;
    m_err_type = RD_KAFKA_RESP_ERR_NO_ERROR;
    m_err_cnt = 0;
}

/**
 * @brief Prepare parameters for Kafka object configuration
 *
 * @param[in]  cfg    Parsed XML configuration
 * @param[out] params Kafka parameters (key/value pairs)
 */
void
Kafka::prepare_params(const struct cfg_kafka &cfg, map_params &params)
{
    params.clear();
    params["bootstrap.servers"] = cfg.brokers;

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
            params["api.version.request"] = "true";
            params["api.version.fallback.ms"] = "0";
        } else if ((broker_ver[0] == 0) && (broker_ver[1] == 8 || broker_ver[1] == 9)) {
            // Broker version 0.9.x and 0.8.x
            params["api.version.request"] = "false";
            params["broker.version.fallback"] = cfg.broker_fallback;
        }
    }

    if (cfg.perf_tuning) {
        // Default performance tuning
        params["batch.num.messages"] = PERF_BATCH_NUM_MSG;

        /* "linger.ms" and "queue.buffering.max.ms" are aliases. Since we don't know which could
         * be set by the user, so we have to check it in advance here to avoid redefinition */
        bool key_found = false;
        for (const auto &name : {"queue.buffering.max.ms", "linger.ms"}) {
            if (cfg.properties.find(name) != cfg.properties.end()) {
                key_found = true;
                break;
            }
        }
        if (!key_found) {
            params["queue.buffering.max.ms"] = PERF_BUFFERING_MS;
        }
    }

    // Add user specified params... (default parameters might be overwritten)
    for (const auto &prop : cfg.properties) {
        params[prop.first] = prop.second;
    }
}

/**
 * @brief Poller thread for Kafka events
 *
 * Only purpose of the thread is to wait for Kafka events and process them instead of the main
 * processing thread. It also makes sure that the producer can work in blocking mode.
 *
 * Additionally statistics about successful and failed deliveries is regularly printed.
 * @param[in] Thread context
 */
void *
Kafka::thread_polling(void *context)
{
    auto *data = reinterpret_cast<thread_ctx_t *>(context);
    IPX_CTX_DEBUG(data->ctx, "Thread for polling Kafka events started!");

    // Initialization
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    data->cnt_delivered = 0;
    data->cnt_failed = 0;

    while (!data->stop) {
        rd_kafka_poll(data->kafka, POLLER_TIMEOUT);

        // Print statistics
        struct timespec ts_now;
        clock_gettime(CLOCK_MONOTONIC, &ts_now);
        if (difftime(ts_now.tv_sec, ts.tv_sec) < 1.0) {
            continue;
        }

        ts = ts_now;
        IPX_CTX_INFO(data->ctx, "STATS: successful deliveries: %" PRIu64 ", failures: %" PRIu64,
            data->cnt_delivered, data->cnt_failed);
        data->cnt_delivered = 0;
        data->cnt_failed = 0;
    }

    IPX_CTX_DEBUG(data->ctx, "Thread for polling Kafka events terminated!")
    return nullptr;
}

/**
 * @brief Message delivery callback for Kafka messages
 *
 * @param[in] rk        Kafka object
 * @param[in] rkmessage Kafka message
 * @param[in] opaque    Application specified data
 */
void
Kafka::thread_cb_delivery(rd_kafka_t *rk, const rd_kafka_message_t *rkmessage, void *opaque)
{
    (void) rk;
    auto *data = reinterpret_cast<thread_ctx_t *>(opaque);

    if (rkmessage->err) {
        IPX_CTX_WARNING(data->ctx, "Message delivery failed: %s", rd_kafka_err2str(rkmessage->err));
        data->cnt_failed++;
    } else {
        data->cnt_delivered++;
    }
}
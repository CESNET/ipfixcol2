/**
 * \file src/plugins/output/json/src/Kafka.hpp
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Kafka output (header file)
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

#ifndef JSON_KAFKA_H
#define JSON_KAFKA_H

#include "Storage.hpp"
#include <memory>
#include <librdkafka/rdkafka.h>

/** JSON kafka connector */
class Kafka : public Output {
public:
    // Constructor
    Kafka(const struct cfg_kafka &cfg, ipx_ctx_t *ctx);
    // Destructor
    ~Kafka();

    // Processing records
    int process(const char *str, size_t len);

private:
    using uniq_kafka = std::unique_ptr<rd_kafka_t, decltype(&rd_kafka_destroy)>;
    using uniq_topic = std::unique_ptr<rd_kafka_topic_t, decltype(&rd_kafka_topic_destroy)>;
    using uniq_config = std::unique_ptr<rd_kafka_conf_t, decltype(&rd_kafka_conf_destroy)>;

    /// Configuration
    std::map<std::string, std::string> m_params;
    /// Kafka object
    uniq_kafka m_kafka = {nullptr, &rd_kafka_destroy};
    /// Topic object
    uniq_topic m_topic = {nullptr, &rd_kafka_topic_destroy};
    /// Producer partition
    int32_t m_partition;
    /// Producer flags
    int m_produce_flags;
};

#endif // JSON_KAFKA_H

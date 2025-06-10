/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Main plugin class implementation
 * @date 2025
 *
 * Copyright(c) 2025 CESNET z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "plugin.h"
#include <iomanip>
#include <iostream>

Plugin::Plugin(ipx_ctx_t *ctx, const char *xml_config)
    : m_ctx(ctx)
    , m_config(parse_config(xml_config))
{
}

void Plugin::process(ipx_msg_t *msg)
{
    if (ipx_msg_get_type(msg) != IPX_MSG_IPFIX) {
        return;
    }

    ipx_msg_ipfix_t *ipfix_msg = ipx_msg_base2ipfix(msg);
    uint32_t drec_cnt = ipx_msg_ipfix_get_drec_cnt(ipfix_msg);

    for (uint32_t i = 0; i < drec_cnt; i++) {
        ipx_ipfix_record *drec = ipx_msg_ipfix_get_drec(ipfix_msg, i);

        if (m_config.skip_option_templates && drec->rec.tmplt->type == FDS_TYPE_TEMPLATE_OPTS) {
            continue;
        }

        fds_drec_iter it;
        fds_drec_iter_init(&it, &drec->rec, 0);

        while (fds_drec_iter_next(&it) != FDS_EOC) {
            uint64_t key = uint64_t(it.field.info->en) << 16 | uint64_t(it.field.info->id);
            auto it = m_field_counts.find(key);
            if (it == m_field_counts.end()) {
                m_field_counts.emplace(key, 1);
                m_fields.emplace_back(key);
            } else {
                it->second++;
            }
        }

        m_total_rec_count++;
    }
}

void Plugin::stop()
{
    const fds_iemgr_t *iemgr = ipx_ctx_iemgr_get(m_ctx);

    std::cout << "{\n";
    std::cout << "    \"total_number_of_records\": " << m_total_rec_count << ",\n";
    std::cout << "    \"elements\": [";

    bool is_first = true;

    for (const uint64_t field_key : m_fields) {
        uint32_t pen = field_key >> 16;
        uint16_t id = field_key & 0xFFFF;
        uint64_t count = m_field_counts[field_key];

        const fds_iemgr_elem *elem = fds_iemgr_elem_find_id(iemgr, pen, id);

        if (!is_first) {
            std::cout << ",";
        }
        std::cout << "\n";
        is_first = false;

        std::cout << "        {\n";
        std::cout << "            \"pen\": " << pen << ",\n";
        std::cout << "            \"id\": " << id << ",\n";
        if (elem) {
            std::cout << "            \"data_type\": \"" << fds_iemgr_type2str(elem->data_type) << "\",\n";
        } else {
            std::cout << "            \"data_type\": null,\n";
        }
        if (elem && elem->scope) {
            std::cout << "            \"name\": \"" << elem->scope->name << ":" << elem->name << "\",\n";
        } else {
            std::cout << "            \"name\": null,\n"; // field definition is missing
        }

        if (elem) {
            std::cout << "            \"aliases\": [";
            bool is_first_alias = true;
            for (size_t i = 0; i < elem->aliases_cnt; i++) {
                const fds_iemgr_alias *alias = elem->aliases[i];
                for (size_t j = 0; j < alias->aliased_names_cnt; j++) {
                    if (!is_first_alias) {
                        std::cout << ",";
                    }
                    std::cout << " ";
                    is_first_alias = false;
                    std::cout << "\"" << alias->aliased_names[j] << "\"";
                }
            }
            std::cout << " ";
            std::cout << "],\n";
        } else {
            std::cout << "            \"aliases\": [ ],\n";
        }

        std::cout << "            \"in_number_of_records\": " << count << ",\n";
        std::cout << "            \"in_percent_of_records\": " << std::setprecision(2) << (double(count) / m_total_rec_count * 100.0) << "\n";
        std::cout << "        }";
    }
    std::cout << "\n";
    std::cout << "    ]\n";
    std::cout << "}\n";
}

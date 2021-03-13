/**
 * @file src/code/configurator/extensions.cpp
 * @author Lukas Hutak (hutak@cesnet.cz)
 * @brief Manager of Data Record extensions
 * @date February 2020
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

#include <algorithm>
#include <stdexcept>
#include "extensions.hpp"

extern "C" {
#include "../message_ipfix.h"
#include "../verbose.h"
}

/** Identification of this component (for log) */
static const char *comp_str = "Configurator (extensions)";

/**
 * \brief Register extension producer or dependency
 *
 * \param[in] name Name of the plugin instance
 * \param[in] pos  Position of the instance in the pipeline
 * \param[in] ext  Reference to extension definition of the plugin (read only)
 */
void
ipx_cfg_extensions::add_extension(const char *name, unsigned int pos, const struct ipx_ctx_ext *ext)
{
    auto ext_info = &m_extensions[ext->data_type][ext->data_name];
    std::vector<struct plugin_rec> &vec_plugins = (ext->etype == IPX_EXTENSION_PRODUCER)
        ? ext_info->producers
        : ext_info->consumers;
    vec_plugins.push_back({name, pos, ext});
}

/**
 * @brief Check extension definition
 *
 * The function checks if there is exactly one producer and that the producer is placed
 * before all consumers in the collector pipeline.
 * @param[in] ident Identification of the extension (for log only)
 * @param[in] rec   Internal extension record
 * @throw runtime_error if any condition is broken
 */
void
ipx_cfg_extensions::check_dependencies(const std::string &ident, const struct ext_rec &rec)
{
    std::string name_producers;
    std::string name_consumers;

    // Prepare list of produces and consumers for log
    for (const auto &producer : rec.producers) {
        if (!name_producers.empty()) {
            name_producers.append(", ");
        }
        name_producers.append("'" + producer.name + "'");
    }
    for (const auto &consumer : rec.consumers) {
        if (!name_consumers.empty()) {
            name_consumers.append(", ");
        }
        name_consumers.append("'" + consumer.name + "'");
    }

    // No producers?
    if (rec.producers.empty()) {
        throw std::runtime_error("No provider of Data Record extension " + ident + " found. "
            "The extension is required by " + name_consumers);
    }

    // Multiple producers?
    if (rec.producers.size() > 1) {
        throw std::runtime_error("Data Record extension " + ident + " is provided by "
            "multiple instances (" + name_producers + ")");
    }

    // No consumer
    if (rec.consumers.empty()) {
        IPX_WARNING(comp_str, "Extension %s is provided by %s, but no other plugins use it. "
            "The provider can be probably removed.", ident.c_str(), name_producers.c_str());
        return;
    }

    // Check if the producer is placed before all consumers
    const struct plugin_rec &producer_rec = rec.producers.front();
    const auto min_cons = std::min_element(rec.consumers.cbegin(), rec.consumers.cend());
    if (producer_rec.inst_pos > min_cons->inst_pos) {
        throw std::runtime_error("Instance '" + producer_rec.name + "', which is a provider "
            "of Data Record extension " + ident + ", is placed in the collector pipeline "
            "after '" + min_cons->name + "' instance, which depends on the extension. "
            "Please, swap the order of the plugin instances.");
    }
}

void
ipx_cfg_extensions::register_instance(ipx_ctx_t *ctx, size_t pos)
{
    const char *inst_name = ipx_ctx_name_get(ctx);
    struct ipx_ctx_ext *arr_ptr = nullptr;
    size_t arr_size = 0;

    if (m_resolved) {
        throw std::runtime_error("(internal) Instance extensions " + std::string(inst_name)
            + " cannot be registered anymore as extension depencies have been resolved!");
    }

    // Register all extensions
    ipx_ctx_ext_defs(ctx, &arr_ptr, &arr_size);
    for (size_t i = 0; i < arr_size; ++i) {
        add_extension(inst_name, pos, &arr_ptr[i]);
    }
}

void
ipx_cfg_extensions::resolve()
{
    size_t offset = 0;
    uint64_t mask = 1U;

    if (m_resolved) {
        return;
    }

    for (auto &ext_type : m_extensions) {
        for (auto &ext_name : ext_type.second) {
            // Check extension dependencies and update its description
            struct ext_rec &ext = ext_name.second;
            std::string ident = "'" + ext_type.first + "/" + ext_name.first + "'";

            if (!mask) {
                // No more bits in the mask!
                throw std::runtime_error("Maximum number of Data Record extensions has been reached!");
            }

            // Check the extension
            check_dependencies(ident, ext);
            assert(ext.producers.size() == 1 && "Exactly one producer");

            // Determine size, offset and bitset mask
            ext.size = ext.producers[0].rec->size;
            ext.offset = offset;
            ext.mask = mask;

            // Align the offset to multiple of 8
            offset += (ext.size % 8U == 0) ? (ext.size) : (((ext.size / 8U) + 1U) * 8U);
            mask <<= 1U;
        }
    }

    m_size_total = offset;
    m_resolved = true;
}

void
ipx_cfg_extensions::update_instance(ipx_ctx_t *ctx)
{
    struct ipx_ctx_ext *arr_ptr = nullptr;
    size_t arr_size = 0;

    if (!m_resolved) {
        throw std::runtime_error("(internal) Extensions hasn't been resolved yet!");
    }

    // For all instance extensions and dependencies
    ipx_ctx_ext_defs(ctx, &arr_ptr, &arr_size);
    for (size_t i = 0; i < arr_size; ++i) {
        // Find the extension definition and update instance parameters
        struct ipx_ctx_ext *ext = &arr_ptr[i];

        const auto &exts_by_type = m_extensions.at(ext->data_type);
        const auto &ext_def = exts_by_type.at(ext->data_name);

        // In case of the producer, the extension size must be still the same
        assert(ext->etype != IPX_EXTENSION_PRODUCER || ext->size == ext_def.size);
        ext->mask = ext_def.mask;
        ext->offset = ext_def.offset;
        ext->size = ext_def.size;
    }

    // Update size of the Data Record in the plugin context
    ipx_ctx_recsize_set(ctx, IPX_MSG_IPFIX_BASE_REC_SIZE + m_size_total);
}

void
ipx_cfg_extensions::list_extensions()
{
    if (!m_resolved) {
        throw std::runtime_error("(internal) Extensions hasn't been resolved yet!");
    }

    if (m_extensions.empty()) {
        IPX_DEBUG(comp_str, "No Data Record extensions!", '\0');
        return;
    }

    for (auto &ext_type : m_extensions) {
        for (auto &ext_name : ext_type.second) {
            struct ext_rec &ext = ext_name.second;
            std::string ident = "'" + ext_type.first + "/" + ext_name.first + "'";

            IPX_DEBUG(comp_str, "Data Record extension %s (size: %zu, offset: %zu, consumers: %zu)",
                ident.c_str(), ext.size, ext.offset, ext.consumers.size());
        }
    }
}
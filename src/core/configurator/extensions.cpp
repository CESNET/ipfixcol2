

#include <algorithm>
#include "extensions.hpp"

/** Identification of this component (for log) */
static const char *comp_str = "Extensions";

void
ipx_cfg_extensions::add_extension(ipx_instance *inst, unsigned int idx, struct ipx_ctx_ext *ext)
{
    auto ext_info = &m_extensions[ext->data_type][ext->data_name];
    std::vector<struct plugin_rec> &vec_plugins = (ext->etype == IPX_EXTENSION_PRODUCER)
        ? ext_info->producers
        : ext_info->consumers;
    vec_plugins.push_back({inst->get_name(), idx, ext});
}

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
        throw std::runtime_error("No provider of Data Record extension " + ident + "found. "
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
    if (producer_rec.plugin_idx > min_cons->plugin_idx) {
        throw std::runtime_error("Instance '" + producer_rec.name + "', which is a provider "
            "of Data Record extension " + ident + ", is placed in the collector pipeline "
            "after '" + min_cons->name + "' instance, which depends on the extension. "
            "Please, swap the order of the plugin instances");
    }
}


void
ipx_cfg_extensions::resolve(std::vector<ipx_instance *> &plugins)
{
    // Add extensions and dependencies from all plugins
    unsigned int pos = 0;
    for (auto &it : plugins) {
        if (dynamic_cast<ipx_instance_input *>(it)) {
            // Input plugins cannot register extensions
            continue;
        }

        struct ipx_ctx_ext *arr_ptr = nullptr;
        size_t arr_size = 0;

        std::tie(arr_ptr, arr_size) = it->get_extensions();
        for (size_t i = 0; i < arr_size; ++i) {
            add_extension(it, pos, &arr_ptr[i]);
        }

        if (!dynamic_cast<ipx_instance_output *>(it)) {
            // Increment position for all plugins except outputs
            pos++;
        }
    };

    if (m_extensions.empty()) {
        // No extensions, no dependencies
        return;
    }

    size_t offset = 0;
    uint64_t mask = 1U;

    for (auto &ext_type : m_extensions) {
        for (auto &ext_name : ext_type.second) {
            if (!mask) {
                // No more bits in the mask!
                throw std::runtime_error("Maximum number of Data Record extensions has been reached!");
            }

            struct ext_rec &ext = ext_name.second;
            assert(ext.producers.size() == 1 && "Exactly one producer");

            // Check the extension
            std::string ident = "'" + ext_type.first + "/" + ext_name.first + "'";
            check_dependencies(ident, ext_name.second);

            // Determine size, offset and bitset mask
            ext.size = ext.producers[0].rec->size;
            ext.offset = offset;
            ext.mask = mask;

            // Align the offset to multiple of 8
            ext.offset += (ext.size % 8U == 0) ? ext.size : (((ext.size / 8U) + 1U) * 8U);
            ext.mask <<= 1U;

            IPX_DEBUG(comp_str, "Extension %s registered (size: %zu, offset: %zu)",
                ext.size, ext.offset);
        }
    }



    // TODO: save total size of all extensions



}


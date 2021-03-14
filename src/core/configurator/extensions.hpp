/**
 * @file src/code/configurator/extensions.hpp
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

#ifndef IPX_CFG_EXTENSIONS_H
#define IPX_CFG_EXTENSIONS_H

#include <map>
#include <memory>
#include <string>
#include <vector>

extern "C" {
#include "../context.h"
#include "../extension.h"
}

/// Extension manager
class ipx_cfg_extensions {
private:
    /// Auxiliary record of an extension producer or consumer
    struct plugin_rec {
        std::string name;                  ///< Plugin instance identification
        unsigned int inst_pos;             ///< Position in the collector pipeline
        const struct ipx_ctx_ext *rec;     ///< Reference to extension structure of the instance

        /**
         * @brief Sort operator based on pipeline position (from lowest to highest)
         * @param[in] other Another plugin instance
         * @return boolean
         */
        bool operator<(const plugin_rec &other) const {
            return inst_pos < other.inst_pos;
        }
    };

    /// Parameters of an extension (unique for combination of the type and name)
    struct ext_rec {
        std::vector<plugin_rec> producers; ///< Extension producers
        std::vector<plugin_rec> consumers; ///< Extension consumers

        size_t size;                       ///< Size of the extension in each Data Record
        size_t offset;                     ///< Offset of the extension in each Data Record
        uint64_t mask;                     ///< Bitsets mask (indication if ext. value is set)
    };

    /// All extensions are resolved
    bool m_resolved = false;
    /// Total size of all extensions (including alignment)
    size_t m_size_total = 0;
    /// Extensions [extension type][extension name]
    std::map<std::string, std::map<std::string, struct ext_rec>> m_extensions;

    void
    add_extension(const char *name, unsigned int pos, const struct ipx_ctx_ext *ext);
    void
    check_dependencies(const std::string &ident, const struct ext_rec &rec);

public:
    // Default constructor and destructor
    ipx_cfg_extensions() = default;
    ~ipx_cfg_extensions() = default;

    /**
     * @brief Register extensions and dependencies of a plugin instance
     *
     * @warning
     *   The extension definition of context MUST NOT be changed after registration until
     *   this object is destroyed.
     * @param[in] ctx Instance context
     * @param[in] pos Position of the instance in the collector pipeline
     */
    void
    register_instance(ipx_ctx_t *ctx, size_t pos);
    /**
     * @brief Resolve extension and dependencies
     *
     * The function makes sure that all conditions are met (e.g. exactly one producer,
     * consumers placed after the producer in the pipeline, etc.)
     * @note After resolving, it is not possible to register more plugin instances!
     * @throw runtime_error if there is a problem with extension defintions
     */
    void
    resolve();
    /**
     * @brief Update extension definitions of a plugin instance
     *
     * Size, offset and mask of each Data Record extension is updated. Moreover, the size of
     * a Data Record of IPFIX Messages is also updated.
     *
     * @throw runtime_error if the extensions/dependencies hasn't been resolved yet
     * @param[in] ctx Instance context
     */
    void
    update_instance(ipx_ctx_t *ctx);

    /**
     * @brief List all extensions as debug messages
     * @throw runtime_error if the extensions/dependencies hasn't been resolved yet
     */
    void
    list_extensions();
};

#endif // IPX_CFG_EXTENSIONS_H
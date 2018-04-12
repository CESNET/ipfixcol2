/**
 * \file src/core/configurator/plugin_finder.hpp
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Plugin finder (header file)
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
#ifndef IPFIXCOL_PLUGIN_FINDER_H
#define IPFIXCOL_PLUGIN_FINDER_H

#include <vector>
#include <string>

extern "C" {
#include "../context.h"
}

/**
 * \brief Plugin finder
 */
class ipx_plugin_finder {
private:
    /** Search paths (files and directories) */
    std::vector<std::string> paths;

    void *file_find(const std::string &name, uint16_t type, const char *path);
    void *dir_find(const std::string &name, uint16_t type, const char *path);

    bool collector_version_check(std::string min_version);

public:
    // Use default constructor and destructor
    ipx_plugin_finder() = default;
    ~ipx_plugin_finder() = default;

    /**
     * \brief Add path to a plugin or directory with plugins
     * \param[in] pathname Path
     */
    void path_add(const std::string &pathname);

    /**
     * \brief Find a plugin with a given name and type
     *
     * Try to find the plugin and check its version requirements, presence of required callbacks,
     * correctness of its description, etc.
     * \warning
     *   After the plugin is not required anymore, a user free handler using dlclose function.
     *
     * \param[in]  name Name of the plugin
     * \param[in]  type Plugin type (one of #IPX_PT_INPUT, #IPX_PT_INTERMEDIATE, #IPX_PT_OUTPUT)
     * \param[out] cbs  Information about the plugin and callbacks
     * \throw runtime_error
     *   If the function failed to find suitable version of the plugin or version requirements
     *   hasn't been met.
     */
    void find(const std::string &name, uint16_t type, struct ipx_ctx_callbacks &cbs);

    /**
     * \brief List all available modules
     * \note Available modules are printed on standard output
     */
    void list();
};

#endif //IPFIXCOL_PLUGIN_FINDER_H

/**
 * \file src/core/configurator/model.cpp
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Parsed configuration model (source file)
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

#include <stdexcept>
#include <strings.h>
#include <iostream>
#include "model.hpp"

extern "C" {
#include "../verbose.h"
}

/**
 * \brief Check common parameters of an instance
 * \param[in] base Plugin instance
 */
void
ipx_config_model::check_common(struct ipx_plugin_base *base)
{
    if (base->name.empty()) {
        throw std::invalid_argument("Name of an instance ('<name>') is not specified or it is "
            "empty!");
    }

    if (base->plugin.empty()) {
        throw std::invalid_argument("Plugin identification ('<plugin>') of the instance '"
            + base->name + "' cannot be empty");
    }

    // Remove an XML namespace if present
    std::size_t pos = base->plugin.find_first_of(':');
    if (pos != std::string::npos) {
        base->plugin.erase(0, pos + 1);
    }

    if (base->params.empty()) {
        throw std::invalid_argument("Parameters ('<params>') of the instance '"
            + base->name + "' are missing!");
    }

    if (!base->verbosity.empty()
        && strcasecmp(base->verbosity.c_str(), "none") != 0
        && strcasecmp(base->verbosity.c_str(), "error") != 0
        && strcasecmp(base->verbosity.c_str(), "warning") != 0
        && strcasecmp(base->verbosity.c_str(), "info") != 0
        && strcasecmp(base->verbosity.c_str(), "debug") != 0
        && strcasecmp(base->verbosity.c_str(), "default") != 0) {
        throw std::invalid_argument("Verbosity level '" + base->verbosity + "' of the instance '"
            + base->name + "' is not valid type!");
    }
}

void
ipx_config_model::add_instance(struct ipx_plugin_input &instance)
{
    // Check parameters and name collisions
    check_common(&instance);
    for (struct ipx_plugin_input &input : inputs) {
        if (instance.name != input.name) {
            continue;
        }

        throw std::invalid_argument("Multiple input instances with the same <name> '"
            + instance.name + "' are not allowed!");
    }

    inputs.push_back(instance);
}

void
ipx_config_model::add_instance(struct ipx_plugin_inter &instance)
{
    // Check parameters and name collisions
    check_common(&instance);
    for (struct ipx_plugin_inter &inter : inters) {
        if (instance.name != inter.name) {
            continue;
        }

        throw std::invalid_argument("Multiple intermediate instances with the same <name> '"
            + instance.name + "' are not allowed!");
    }

    inters.push_back(instance);
}

void
ipx_config_model::add_instance(struct ipx_plugin_output &instance)
{
    // Check parameters and name collisions
    check_common(&instance);
    for (struct ipx_plugin_output &output : outputs) {
        if (instance.name != output.name) {
            continue;
        }

        throw std::invalid_argument("Multiple output instances with the same <name> '"
            + instance.name + "' are not allowed!");
    }

    // Check output specific parameters
    if (instance.odid_type != IPX_ODID_FILTER_NONE && instance.odid_expression.empty()) {
        throw std::invalid_argument("ODID filter ('<odidOnly>' or '<odidExcept>') of the "
            "output instance '" + instance.name + "' cannot be empty!");
    }

    outputs.push_back(instance);
}

void
ipx_config_model::dump()
{
    // Input plugins
    std::cout << "Input plugins:\n";
    for (auto &in : inputs) {
        std::cout << "\t- " << in.plugin << " / " << in.name << "\n";
    }

    if (inputs.empty()) {
        std::cout << "\t(none)\n";
    }

    std::cout << "\n";

    // Intermediate plugins
    std::cout << "Intermediate plugins:\n";
    for (auto &inter : inters) {
        std::cout << "\t- " << inter.plugin << " / " << inter.name << "\n";
    }

    if (inters.empty()) {
        std::cout << "\t(none)\n";
    }

    std::cout << "\n";

    // Output plugins
    std::cout << "Output plugins:\n";
    for (auto &out : outputs) {
        std::cout << "\t- " << out.plugin << " / " << out.name << "\n";
    }

    if (outputs.empty()) {
        std::cout << "\t(none)\n";
    }

    std::cout << "\n";

}
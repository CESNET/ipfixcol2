//
// Created by lukashutak on 12/04/18.
//

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
        && strcasecmp(base->verbosity.c_str(), "debug") != 0) {
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
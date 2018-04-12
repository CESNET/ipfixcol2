//
// Created by lukashutak on 04/04/18.
//

#include <memory>
#include <iostream>
#include <cstdlib>
#include <dlfcn.h>

#include "configurator.hpp"

extern "C" {
    #include "../plugin_parser.h"
    #include "../plugin_output_mgr.h"
}

/** Description of the internal parser plugin */
static const struct ipx_ctx_callbacks parser_callbacks = {
    // Static plugin, no library handles
    nullptr,
    &ipx_plugin_parser_info,
    // Only basic functions
    &ipx_plugin_parser_init,
    &ipx_plugin_parser_destroy,
    nullptr,
    &ipx_plugin_parser_process,
    nullptr
};

/** Description of the internal output manager */
static const struct ipx_ctx_callbacks output_mgr_callbacks = {
    // Static plugin, no library handles
    nullptr,
    &ipx_plugin_output_mgr_info,
    // Only basic functions
    &ipx_plugin_output_mgr_init,
    &ipx_plugin_output_mgr_destroy,
    nullptr,
    &ipx_plugin_output_mgr_process,
    nullptr
};

/** Component identification (for log) */
static const char *comp_str = "Configurator";

ipx_configurator::ipx_configurator()
{
    running_model = nullptr;
}

ipx_configurator::~ipx_configurator()
{
    if (running_model != nullptr) {
        delete(running_model);
    }
}

void
ipx_configurator::apply(const ipx_config_model &model)
{
    if (running_model != nullptr) {
        throw std::runtime_error("Sorry, runtime reconfiguration is not implemented!");
    }

    if (model.inputs.empty()) {
        throw std::runtime_error("At least one input plugin must be defined!");
    }

    if (model.outputs.empty()) {
        throw std::runtime_error("At least one output plugin must be defined!");
    }

    // Try to find plugins
    for (auto &input : model.inputs) {
        ipx_ctx_callbacks cbs;
        finder.find(input.plugin, IPX_PT_INPUT, cbs);
        dlclose(cbs.handle);
    }

    for (auto &intermediate : model.inters) {
        ipx_ctx_callbacks cbs;
        finder.find(intermediate.plugin, IPX_PT_INTERMEDIATE, cbs);
        dlclose(cbs.handle);
    }

    for (auto &output : model.outputs) {
        ipx_ctx_callbacks cbs;
        finder.find(output.plugin, IPX_PT_OUTPUT, cbs);
        dlclose(cbs.handle);
    }
}

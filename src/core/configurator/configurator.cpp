//
// Created by lukashutak on 04/04/18.
//

#include <memory>
#include <iostream>
#include <cstdlib>

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

Configurator::Configurator()
{
}

Configurator::~Configurator()
{
}

//
// Created by lukashutak on 04/04/18.
//

#include <iostream>

extern "C" {
#include "configurator.h"
}

int
ipx_config_input_add(const struct ipx_cfg_input *cfg)
{
    std::cout << "Request to add input plugin:\n"
        << "\tPlugin:   " << cfg->common.plugin    << "\n"
        << "\tName:     " << cfg->common.name      << "\n"
        << "\tVebosity: " << std::to_string(cfg->common.verb_mode) << "\n"
        << "\tParams:   " << cfg->common.params    << "\n";
    return IPX_OK;
}

int
ipx_config_inter_add(const struct ipx_cfg_inter *cfg)
{
    std::cout << "Request to add intermediate plugin:\n"
        << "\tPlugin:   " << cfg->common.plugin    << "\n"
        << "\tName:     " << cfg->common.name      << "\n"
        << "\tVebosity: " << std::to_string(cfg->common.verb_mode) << "\n"
        << "\tParams:   " << cfg->common.params    << "\n";
    return IPX_OK;
}

int
ipx_config_output_add(const struct ipx_cfg_output *cfg)
{
    std::cout << "Request to add output plugin:\n"
        << "\tPlugin:   " << cfg->common.plugin    << "\n"
        << "\tName:     " << cfg->common.name      << "\n"
        << "\tVebosity: " << std::to_string(cfg->common.verb_mode) << "\n";
    switch (cfg->odid_filter.type) {
    case IPX_CFG_ODID_FILTER_NONE:
        std::cout << "\tODID:     all\n";
        break;
    case IPX_CFG_ODID_FILTER_ONLY:
        std::cout << "\tODID:     only "<< cfg->odid_filter.expression << "\n";
        break;
    case IPX_CFG_ODID_FILTER_EXCEPT:
        std::cout << "\tODID:     except "<< cfg->odid_filter.expression << "\n";
        break;
    }
    std::cout << "\tParams:   " << cfg->common.params    << "\n";
    return IPX_OK;
}
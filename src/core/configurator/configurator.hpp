#ifndef IPFIXCOL_CONFIGURATOR_H
#define IPFIXCOL_CONFIGURATOR_H

#include <stdint.h>
#include <vector>
#include "plugin_finder.hpp"
#include "model.hpp"

extern "C" {
#include <ipfixcol2.h>
#include "../context.h"
}

class ipx_configurator {
private:
    ipx_config_model *running_model;
public:
    /** Constructor */
    ipx_configurator();
    /** Destructor  */
    ~ipx_configurator();

    // Disable copy and move constructors
    ipx_configurator(const ipx_configurator &) = delete;
    ipx_configurator& operator=(const ipx_configurator &) = delete;
    ipx_configurator(ipx_configurator &&) = delete;
    ipx_configurator& operator=(ipx_configurator &&) = delete;

    void apply(const ipx_config_model &model);

    /** Modules finder */
    ipx_plugin_finder finder;
};

#endif //IPFIXCOL_CONFIGURATOR_H

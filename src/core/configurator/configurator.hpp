#ifndef IPFIXCOL_CONFIGURATOR_H
#define IPFIXCOL_CONFIGURATOR_H

#include <stdint.h>
#include <vector>
#include "plugin_finder.hpp"

extern "C" {
#include <ipfixcol2.h>
#include "../context.h"
}

class Configurator {
private:

public:
    /** Constructor */
    Configurator();
    /** Destructor  */
    ~Configurator();

    // Disable copy and move constructors
    Configurator(const Configurator &) = delete;
    Configurator& operator=(const Configurator &) = delete;
    Configurator(Configurator &&) = delete;
    Configurator& operator=(Configurator &&) = delete;

    /** Modules finder */
    plugin_finder finder;
};

#endif //IPFIXCOL_CONFIGURATOR_H

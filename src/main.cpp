/**
 * \file src/main.cpp
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Main body of IPFIXcol
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

#include <iostream>
#include <string>
#include <vector>
#include <unistd.h>

#include <ipfixcol2.h>
#include <iostream>
#include "core/config_file.hpp"
#include "build_config.h"

extern "C" {
#include "core/verbose.h"
}

/**
 * \brief Print information about version of the collector to standard output
 */
static void
print_version()
{
    std::cout
        << "Version:      " << IPX_BUILD_VERSION_FULL_STR << "\n"
        << "GIT hash:     " << IPX_BUILD_GIT_HASH << "\n"
        << "Build type:   " << IPX_BUILD_TYPE << "\n"
        << "Architecture: " << IPX_BUILD_ARCH << "(" << IPX_BUILD_BYTE_ORDER << ")" << "\n"
        << "Compiler:     " << IPX_BUILD_COMPILER << "\n";
}

/**
 * \brief Print help message
 */
static void
print_help()
{
    std::cout
        << "IPFIX Collector daemon\n"
        << "Usage: ipfixcol [-c FILE] [-d PATH] [-vVh]\n"
        << "  -c FILE   Path to the startup configuration file (default: "
            << IPX_DEFAULT_STARTUP_CONFIG << ")\n"
        << "  -p PATH   Add path to a directory with plugins or to a file (default: "
           << IPX_DEFAULT_PLUGINS_DIR << ")\n"
        << "  -h        Show this help message and exit\n"
        << "  -V        Show version information and exit\n"
        << "  -v        Be verbose (in addition, show warning messages)\n"
        << "  -vv       Be more verbose (like previous + info messages)\n"
        << "  -vvv      Be even more verbose (like previous + debug messages)\n";
}

/**
 * \brief Increase global verbosity of the collector
 */
static void
increase_verbosity()
{
    enum ipx_verb_level level = ipx_verb_level_get();
    switch (level) {
    case IPX_VERB_ERROR:   level = IPX_VERB_WARNING; break;
    case IPX_VERB_WARNING: level = IPX_VERB_INFO; break;
    case IPX_VERB_INFO:    level = IPX_VERB_DEBUG; break;
    default: break; // Nothing to do
    }

    ipx_verb_level_set(level);
}

/**
 * \brief Main function
 * \param[in] argc Number of arguments
 * \param[in] argv Vector of the arguments
 * \return On success returns EXIT_SUCCESS. Otherwise returns EXIT_FAILURE.
 */
int main(int argc, char *argv[])
{
    const char *cfg_startup = NULL;
    std::vector<std::string> plugin_paths; // Directories and/or files

    // Parse configuration
    int opt;
    opterr = 0; // Disable default error messages
    while ((opt = getopt(argc, argv, "c:vVhp:")) != -1) {
        switch (opt) {
        case 'c': // Configuration file
            cfg_startup = optarg;
            break;
        case 'v': // Verbosity
            increase_verbosity();
            break;
        case 'V': // Version
            print_version();
            return EXIT_SUCCESS;
        case 'h': // Help
            print_help();
            return EXIT_SUCCESS;
        case 'p': // Plugin search path
            plugin_paths.emplace_back(std::string(optarg));
            break;
        default: // ?
            std::cerr << "Unknown parameter '" << static_cast<char>(optopt) << "'!" << std::endl;
            return EXIT_FAILURE;
        }
    }

    if (!cfg_startup) {
        // Use default startup configuration
        cfg_startup = IPX_DEFAULT_STARTUP_CONFIG;
    }

    // Always use the default directory for looking for plugins, but with the lowest priority
    plugin_paths.emplace_back(IPX_DEFAULT_PLUGINS_DIR);
    // Initialize the pipeline configurator
    // TODO: pass default configuration directory

    // TODO: only list plugins???

    // Pass control to the parser of the configuration file
    int rc;
    try {
        rc = ipx_file_parse(std::string(cfg_startup));
    } catch (std::exception &ex) {
        std::cerr << "An error has occurred during processing configuration: " << ex.what()
            << std::endl;
        return EXIT_FAILURE;
    } catch (...) {
        std::cerr << "Unexpected exception has occurred!" << std::endl;
        return EXIT_FAILURE;
    }

    // Destroy the pipeline configurator
    // TODO: destroy
    return rc;
}


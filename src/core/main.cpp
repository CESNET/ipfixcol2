/**
 * \file src/core/main.cpp
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
#include <sys/types.h> // getpid()
#include <unistd.h>
#include <cstdlib>
#include <cinttypes>

#include <ipfixcol2.h>
#include <iostream>
#include <fstream>
#include "configurator/configurator.hpp"
#include "configurator/controller_file.hpp"

extern "C" {
#include "verbose.h"
#include <build_config.h>
}

/** Internal identification of the module */
static const char *module = "Configurator";

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
        << "Architecture: " << IPX_BUILD_ARCH << " (" << IPX_BUILD_BYTE_ORDER << ")" << "\n"
        << "Compiler:     " << IPX_BUILD_COMPILER << "\n"
        << "Copyright (C) 2018 CESNET z.s.p.o.\n";
}

/**
 * \brief Print help message
 */
static void
print_help()
{
    std::cout
        << "IPFIX Collector daemon\n"
        << "Usage: ipfixcol2 [-c FILE] [-p PATH] [-e DIR] [-P FILE] [-r SIZE] [-vVhLdu]\n"
        << "  -c FILE   Path to the startup configuration file\n"
        << "            (default: " << IPX_DEFAULT_STARTUP_CONFIG << ")\n"
        << "  -p PATH   Add path to a directory with plugins or to a file\n"
        << "            (default: " << IPX_DEFAULT_PLUGINS_DIR << ")\n"
        << "  -e DIR    Path to a directory with definitions of IPFIX Information Elements\n"
        << "            (default: " << fds_api_cfg_dir() << ")\n"
        << "  -P FILE   Path to a PID file (without this option, no PID file is created)\n"
        << "  -d        Run as a standalone daemon process\n"
        << "  -r SIZE   Ring buffer size (default: " << ipx_configurator::RING_DEF_SIZE << ")\n"
        << "  -h        Show this help message and exit\n"
        << "  -V        Show version information and exit\n"
        << "  -L        List all available plugins and exit\n"
        << "  -v        Increase verbosity level (by default, show only error messages)\n"
        << "            (can be used up to 3 times to add warning/info/debug messages)\n"
        << "  -u        Disable plugins unload on exit (only for plugin developers)\n";
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
 * \brief Create a PID file (contains Process ID)
 * \param[in] file PID file
 * \return #IPX_OK on success
 * \return #IPX_ERR_DENIED if it is not possible to create the file
 */
static int
pid_create(const char *file)
{
    IPX_INFO(module, "Creating PID file '%s'", file);
    std::ofstream pid_file(file, std::ofstream::out);
    if (pid_file.fail()) {
        IPX_WARNING(module, "Failed to create a PID file '%s'!", file);
        return IPX_ERR_DENIED;
    }

    pid_file << getpid();
    pid_file.close();
    return IPX_OK;
}

/**
 * \brief Remove a PID file (contains Process ID)
 * \param[in] file PID file
 * \return #IPX_OK on success
 * \return #IPX_ERR_DENIED if it is not possible to remove the file
 */
static int
pid_remove(const char *file)
{
    IPX_INFO(module, "Removing PID file '%s'", file);
    if (unlink(file) == -1) {
        const char *err_str;
        ipx_strerror(errno, err_str);
        IPX_WARNING(module, "Failed to remove a PID file '%s': %s", file, err_str);
        return IPX_ERR_DENIED;
    }

    return IPX_OK;
}

/**
 * \brief Change size of ring buffers
 * \param[in] conf     IPFIXcol configurator
 * \param[in] new_size New size (from command line)
 * \return #IPX_OK on success
 * \return #IPX_ERR_FORMAT if the \p new_size is not valid size
 */
static int
ring_size_change(ipx_configurator &conf, const char *new_size)
{
    char *end_ptr = nullptr;
    errno = 0;
    unsigned int size = std::strtoul(new_size, &end_ptr, 10);
    if (errno != 0 || (end_ptr != nullptr && (*end_ptr) != '\0')) {
        IPX_ERROR(module, "Size '%s' of the ring buffers is not a valid number!", new_size);
        return IPX_ERR_FORMAT;
    }

    const uint32_t min_size = ipx_configurator::RING_MIN_SIZE;
    if (size < min_size) {
        IPX_ERROR(module, "Size of the ring buffers must be at least %" PRIu32 " messages.",
            min_size);
        return IPX_ERR_FORMAT;
    }

    conf.set_buffer_size(size);
    IPX_INFO(module, "Ring buffer size set to %u messages", size);
    return IPX_OK;
}

/**
 * \brief Main function
 * \param[in] argc Number of arguments
 * \param[in] argv Vector of the arguments
 * \return On success returns EXIT_SUCCESS. Otherwise returns EXIT_FAILURE.
 */
int main(int argc, char *argv[])
{
    const char *cfg_startup = nullptr;
    const char *cfg_iedir = nullptr;
    const char *pid_file = nullptr;
    const char *ring_size = nullptr;
    bool daemon_en = false;
    bool list_only = false;
    ipx_configurator configurator;

    // Parse configuration
    int opt;
    opterr = 0; // Disable default error messages
    while ((opt = getopt(argc, argv, "c:vVhLdp:e:P:r:u")) != -1) {
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
        case 'L': // List of available plugins
            list_only = true;
            break;
        case 'd': // Run as a standalone process (daemon)
            daemon_en = true;
            break;
        case 'p': // Plugin search path
            configurator.plugins.path_add(std::string(optarg));
            break;
        case 'e': // Redefine path to Information Elements definition
            cfg_iedir = optarg;
            break;
        case 'P': // Create a PID file
            pid_file = optarg;
            break;
        case 'r': // Change ring size
            ring_size = optarg;
            break;
        case 'u': // Disable automatic plugin unload
            configurator.plugins.auto_unload(false);
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

    if (!cfg_iedir) {
        // Use default directory of the library
        cfg_iedir = fds_api_cfg_dir();
    }

    // Always use the default directory for looking for plugins, but with the lowest priority
    configurator.plugins.path_add(IPX_DEFAULT_PLUGINS_DIR);
    configurator.iemgr_set_dir(cfg_iedir);

    if (list_only) {
        configurator.plugins.plugin_list();
        return EXIT_SUCCESS;
    }

    if (daemon_en) {
        // Run as a standalone daemon process
        ipx_verb_syslog(true);
        if (daemon(1, 0) == -1) {
            const char *err_str;
            ipx_strerror(errno, err_str);
            IPX_ERROR(module, "Failed to start as a standalone daemon: %s", err_str);
            return EXIT_FAILURE;
        }
    }

    if (ring_size != nullptr && ring_size_change(configurator, ring_size) != IPX_OK) {
        // Failed to set the size
        return EXIT_FAILURE;
    }

    // Create a PID file
    if (pid_file != nullptr && pid_create(pid_file) != IPX_OK) {
        pid_file = nullptr; // Prevent removing the file
    }

    // Create a configuration controller and use it to start the collector
    int rc;
    try {
        ipx_controller_file ctrl_file(cfg_startup);
        rc = configurator.run(&ctrl_file);
    } catch (std::exception &ex) {
        std::cerr << "An unexpected error has occurred: " << ex.what() << std::endl;
        return EXIT_FAILURE;
    } catch (...) {
        std::cerr << "An unexpected exception has occurred!" << std::endl;
        return EXIT_FAILURE;
    }

    // Destroy a PID file
    if (pid_file != nullptr) {
        pid_remove(pid_file);
    }

    // Destroy the pipeline configurator
    return rc;
}


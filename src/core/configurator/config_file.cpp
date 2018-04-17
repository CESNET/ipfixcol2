/**
 * \file src/core/configurator/config_file.cpp
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Parser of configuration file (source file)
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

#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <memory>

#include <libfds.h>
#include <iostream>
#include "config_file.hpp"
#include "configurator.hpp"
#include "model.hpp"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <signal.h>

extern "C" {
#include "../utils.h"
#include "../verbose.h"
}

/** Component identification (for log) */
static const char *comp_str = "Configurator";

/** Types of XML configuration nodes   */
enum file_xml_nodes {
    // List of plugin instances
    LIST_INPUTS = 1,
    LIST_INTER,
    LIST_OUTPUT,
    // Instances
    INSTANCE_INPUT,
    INSTANCE_INTER,
    INSTANCE_OUTPUT,
    // Input plugin parameters
    IN_PLUGIN_NAME,
    IN_PLUGIN_PLUGIN,
    IN_PLUGIN_PARAMS,
    IN_PLUGIN_VERBOSITY,
    // Intermediate plugin parameters
    INTER_PLUGIN_NAME,
    INTER_PLUGIN_PLUGIN,
    INTER_PLUGIN_PARAMS,
    INTER_PLUGIN_VERBOSITY,
    // Output plugin parameters
    OUT_PLUGIN_NAME,
    OUT_PLUGIN_PLUGIN,
    OUT_PLUGIN_PARAMS,
    OUT_PLUGIN_VERBOSITY,
    OUT_PLUGIN_ODID_ONLY,
    OUT_PLUGIN_ODID_EXCEPT,
};

/**
 * \brief Definition of the \<input\> node
 * \note Presence of the all required parameters is checked during building of the model
 */
static const struct fds_xml_args args_instance_input[] = {
    FDS_OPTS_ELEM(IN_PLUGIN_NAME,      "name",       FDS_OPTS_T_STRING, FDS_OPTS_P_OPT),
    FDS_OPTS_ELEM(IN_PLUGIN_PLUGIN,    "plugin",     FDS_OPTS_T_STRING, FDS_OPTS_P_OPT),
    FDS_OPTS_ELEM(IN_PLUGIN_VERBOSITY, "verbosity",  FDS_OPTS_T_STRING, FDS_OPTS_P_OPT),
    FDS_OPTS_RAW( IN_PLUGIN_PARAMS,    "params",                        FDS_OPTS_P_OPT),
    FDS_OPTS_END
};

/**
 * \brief Definition of the \<inputPlugins\> node
 * \note The configurator checks later if at least one instance is present
 */
static const struct fds_xml_args args_list_inputs[] = {
    FDS_OPTS_NESTED(INSTANCE_INPUT, "input", args_instance_input, FDS_OPTS_P_OPT | FDS_OPTS_P_MULTI),
    FDS_OPTS_END
};

/**
 * \brief Definition of the \<intermediate\> node
 * \note Presence of the all required parameters is checked during building of the model
 */
static const struct fds_xml_args args_instance_inter[] = {
    FDS_OPTS_ELEM(INTER_PLUGIN_NAME,      "name",       FDS_OPTS_T_STRING, FDS_OPTS_P_OPT),
    FDS_OPTS_ELEM(INTER_PLUGIN_PLUGIN,    "plugin",     FDS_OPTS_T_STRING, FDS_OPTS_P_OPT),
    FDS_OPTS_ELEM(INTER_PLUGIN_VERBOSITY, "verbosity",  FDS_OPTS_T_STRING, FDS_OPTS_P_OPT),
    FDS_OPTS_RAW( INTER_PLUGIN_PARAMS,    "params",                        FDS_OPTS_P_OPT),
    FDS_OPTS_END
};

/** Definition of the \<intermediatePlugins\> node                                               */
static const struct fds_xml_args args_list_inter[] = {
    FDS_OPTS_NESTED(INSTANCE_INTER, "intermediate", args_instance_inter, FDS_OPTS_P_OPT | FDS_OPTS_P_MULTI),
    FDS_OPTS_END
};

/**
 * \brief Definition of the \<output\> node
 * \note Presence of the all required parameters is checked during building of the model
 */
static const struct fds_xml_args args_instance_output[] = {
    FDS_OPTS_ELEM(OUT_PLUGIN_NAME,        "name",       FDS_OPTS_T_STRING, FDS_OPTS_P_OPT),
    FDS_OPTS_ELEM(OUT_PLUGIN_PLUGIN,      "plugin",     FDS_OPTS_T_STRING, FDS_OPTS_P_OPT),
    FDS_OPTS_ELEM(OUT_PLUGIN_VERBOSITY,   "verbosity",  FDS_OPTS_T_STRING, FDS_OPTS_P_OPT),
    FDS_OPTS_ELEM(OUT_PLUGIN_ODID_EXCEPT, "odidExcept", FDS_OPTS_T_STRING, FDS_OPTS_P_OPT),
    FDS_OPTS_ELEM(OUT_PLUGIN_ODID_ONLY,   "odidOnly",   FDS_OPTS_T_STRING, FDS_OPTS_P_OPT),
    FDS_OPTS_RAW( OUT_PLUGIN_PARAMS,      "params",                        FDS_OPTS_P_OPT),
    FDS_OPTS_END
};

/**
 * \brief Definition of the \<outputPlugins\> node
 * \note The configurator checks later if at least one instance is present
 */
static const struct fds_xml_args args_list_output[] = {
    FDS_OPTS_NESTED(INSTANCE_OUTPUT, "output", args_instance_output, FDS_OPTS_P_OPT | FDS_OPTS_P_MULTI),
    FDS_OPTS_END
};

/**
 * \brief Definition of the main \<ipfixcol2\> node
 * \note
 *   Missing an input or output instance is check during starting of a new pipeline in the
 *   configurator.
 */
static const struct fds_xml_args args_main[] = {
    FDS_OPTS_ROOT("ipfixcol2"),
    FDS_OPTS_NESTED(LIST_INPUTS, "inputPlugins",        args_list_inputs, FDS_OPTS_P_OPT),
    FDS_OPTS_NESTED(LIST_INTER,  "intermediatePlugins", args_list_inter,  FDS_OPTS_P_OPT),
    FDS_OPTS_NESTED(LIST_OUTPUT, "outputPlugins",       args_list_output, FDS_OPTS_P_OPT),
    FDS_OPTS_END
};

/**
 * \brief Terminating signal handler
 */
void termination_handler(int sig)
{
    (void) sig;
    static const char *msg = "Another termination signal detected. Quiting without cleanup...\n";
    write(STDOUT_FILENO, msg, strlen(msg));
    abort();
}

/**
 * \brief Parse \<input\> node and add the parsed input instance to the model
 * \param[in] ctx   Parsed XML node
 * \param[in] model Configuration model
 * \throw invalid_argument if the parameters are not valid or missing
 */
static void
file_parse_instance_input(fds_xml_ctx_t *ctx, ipx_config_model &model)
{
    struct ipx_plugin_input input;

    const struct fds_xml_cont *content;
    while (fds_xml_next(ctx, &content) != FDS_EOC) {
        switch (content->id) {
        case IN_PLUGIN_NAME:
            input.name = content->ptr_string;
            break;
        case IN_PLUGIN_PLUGIN:
            input.plugin = content->ptr_string;
            break;
        case IN_PLUGIN_VERBOSITY:
            input.verbosity = content->ptr_string;
            break;
        case IN_PLUGIN_PARAMS:
            input.params = content->ptr_string;
            break;
        default:
            // Unexpected XML node within <input>!
            assert(false);
        }
    }

    model.add_instance(input);
}

/**
 * \brief Parse \<inputPlugins\> node and add the parsed input instances to the model
 * \param[in] ctx   Parsed XML node
 * \param[in] model Configuration model
 * \throw invalid_argument if the parameters are not valid or missing
 */
static void
file_parse_list_input(fds_xml_ctx_t *ctx, ipx_config_model &model)
{
    unsigned int cnt = 0;
    const struct fds_xml_cont *content;

    while (fds_xml_next(ctx, &content) != FDS_EOC) {
        // Process an input plugin
        assert(content->id == INSTANCE_INPUT);
        cnt++;

        try {
            file_parse_instance_input(content->ptr_ctx, model);
        } catch (std::exception &ex) {
            throw std::runtime_error("Failed to parse the configuration of the "
                + std::to_string(cnt) + ". input plugin: " + ex.what());
        }
    }
}

/**
 * \brief Parse \<intermediate\> node and add the parsed intermediate instance to the model
 * \param[in] ctx   Parsed XML node
 * \param[in] model Configuration model
 * \throw invalid_argument if the parameters are not valid or missing
 */
static void
file_parse_instance_inter(fds_xml_ctx_t *ctx, ipx_config_model &model)
{
    struct ipx_plugin_inter inter;

    const struct fds_xml_cont *content;
    while (fds_xml_next(ctx, &content) != FDS_EOC) {
        switch (content->id) {
        case INTER_PLUGIN_NAME:
            inter.name = content->ptr_string;
            break;
        case INTER_PLUGIN_PLUGIN:
            inter.plugin = content->ptr_string;
            break;
        case INTER_PLUGIN_VERBOSITY:
            inter.verbosity = content->ptr_string;
            break;
        case INTER_PLUGIN_PARAMS:
            inter.params = content->ptr_string;
            break;
        default:
            // "Unexpected XML node within <intermediate>!"
            assert(false);
        }
    }

    model.add_instance(inter);
}

/**
 * \brief Parse \<intermediatePlugins\> node and add the parsed intermediate instances to the model
 * \param[in] ctx   Parsed XML node
 * \param[in] model Configuration model
 * \throw invalid_argument if the parameters are not valid or missing
 */
static void
file_parse_list_inter(fds_xml_ctx_t *ctx, ipx_config_model &model)
{
    unsigned int cnt = 0;
    const struct fds_xml_cont *content;

    while (fds_xml_next(ctx, &content) != FDS_EOC) {
        // Process an intermediate plugin
        assert(content->id == INSTANCE_INTER);
        cnt++;

        try {
            file_parse_instance_inter(content->ptr_ctx, model);
        } catch (std::exception &ex) {
            throw std::runtime_error("Failed to parse the configuration of the "
                + std::to_string(cnt) + ". intermediate plugin: " + ex.what());
        }
    }
}

/**
 * \brief Parse \<output\> node and add the parsed output instance to the model
 * \param[in] ctx   Parsed XML node
 * \param[in] model Configuration model
 * \throw invalid_argument if the parameters are not valid or missing
 */
static void
file_parse_instance_output(fds_xml_ctx_t *ctx, ipx_config_model &model)
{
    struct ipx_plugin_output output;
    output.odid_type = IPX_ODID_FILTER_NONE; // default
    bool odid_set = false;

    const struct fds_xml_cont *content;
    while (fds_xml_next(ctx, &content) != FDS_EOC) {
        switch (content->id) {
        case OUT_PLUGIN_NAME:
            output.name = content->ptr_string;
            break;
        case OUT_PLUGIN_PLUGIN:
            output.plugin = content->ptr_string;
            break;
        case OUT_PLUGIN_VERBOSITY:
            output.verbosity= content->ptr_string;
            break;
        case OUT_PLUGIN_PARAMS:
            output.params = content->ptr_string;
            break;
        case OUT_PLUGIN_ODID_EXCEPT:
            if (!odid_set) {
                output.odid_type = IPX_ODID_FILTER_EXCEPT;
                output.odid_expression = content->ptr_string;
                odid_set = true;
                break;
            }
            throw std::runtime_error("Multiple definitions of <odidExcept>/<odidOnly>!");
        case OUT_PLUGIN_ODID_ONLY:
            if (!odid_set) {
                output.odid_type = IPX_ODID_FILTER_ONLY;
                output.odid_expression = content->ptr_string;
                odid_set = true;
                break;
            }
            throw std::runtime_error("Multiple definitions of <odidExcept>/<odidOnly>!");
        default:
            // Unexpected XML node within <output>!
            assert(false);
        }
    }

    model.add_instance(output);
}

/**
 * \brief Parse \<outputPlugins\> node and add the parsed intermediate instances to the model
 * \param[in] ctx   Parsed XML node
 * \param[in] model Configuration model
 * \throw invalid_argument if the parameters are not valid or missing
 */
static void
file_parse_list_output(fds_xml_ctx_t *ctx, ipx_config_model &model)
{
    unsigned int cnt = 0;
    const struct fds_xml_cont *content;

    while (fds_xml_next(ctx, &content) != FDS_EOC) {
        // Process an output plugin
        assert(content->id == INSTANCE_OUTPUT);
        cnt++;

        try {
            file_parse_instance_output(content->ptr_ctx, model);
        } catch (std::exception &ex) {
            throw std::runtime_error("Failed to parse the configuration of the "
                + std::to_string(cnt) + ". output plugin: " + ex.what());
        }
    }
}

/**
 * \brief Parse startup configuration file
 *
 *
 * \param[in] path Path to the startup file
 * \return Parsed model
 * \throw runtime_error if the file doesn't exists, it's not available or it is malformed
 * \throw invalid_argument if some parameters are not valid or missing
 */
static ipx_config_model
file_parse_model(const std::string &path)
{
    // Is it really a configuration file
    struct stat file_info;
    std::unique_ptr<char, decltype(&free)> real_path(realpath(path.c_str(), nullptr), &free);
    if (!real_path || stat(real_path.get(), &file_info) != 0) {
        throw std::runtime_error("Failed to get info about '" + path + "'. Check if the path "
            "exists and the application has permission to access it.");
    }

    if ((file_info.st_mode & S_IFREG) == 0) {
        throw std::runtime_error("The specified path '" + path + "' doesn't lead to a valid "
            "configuration file!");
    }

    // Load content of the configuration file
    std::unique_ptr<FILE, decltype(&fclose)> stream(fopen(path.c_str(), "r"), &fclose);
    if (!stream) {
        // Failed to open the file
        std::string err_msg = "Unable to open the file '" + path + "'";
        throw std::runtime_error(err_msg);
    }

    // Load whole content of the file
    fseek(stream.get(), 0, SEEK_END);
    long fsize = ftell(stream.get());
    if (fsize == -1) {
        throw std::runtime_error("Unable to get the size of the file '" + path + "'");
    }

    rewind(stream.get());
    std::unique_ptr<char[]> fcontent(new char[fsize + 1]);
    if (fread(fcontent.get(), fsize, 1, stream.get()) != 1) {
        throw std::runtime_error("Failed to load startup configuration.");
    }
    fcontent.get()[fsize] = '\0';

    // Create a parser and try try to parse document
    std::unique_ptr<fds_xml_t, decltype(&fds_xml_destroy)> parser(fds_xml_create(), &fds_xml_destroy);
    if (!parser) {
        throw std::runtime_error("fds_xml_create() failed!");
    }

    if (fds_xml_set_args(parser.get(), args_main) != FDS_OK) {
        throw std::runtime_error("fds_xml_set_args() failed: "
            + std::string(fds_xml_last_err(parser.get())));
    }

    fds_xml_ctx_t *ctx = fds_xml_parse_mem(parser.get(), fcontent.get(), true);
    if (!ctx) {
        throw std::runtime_error("Failed to parse configuration: "
            + std::string(fds_xml_last_err(parser.get())));
    }

    ipx_config_model model;
    const struct fds_xml_cont *content;
    while(fds_xml_next(ctx, &content) != FDS_EOC) {
        assert(content->type == FDS_OPTS_T_CONTEXT);
        switch (content->id) {
        case LIST_INPUTS:
            file_parse_list_input(content->ptr_ctx, model);
            break;
        case LIST_INTER:
            file_parse_list_inter(content->ptr_ctx, model);
            break;
        case LIST_OUTPUT:
            file_parse_list_output(content->ptr_ctx, model);
            break;
        default:
            // Unexpected XML node within startup <ipfixcol2>!
            assert(false);
        }
    }

    return model;
}

int
ipx_config_file(ipx_configurator &conf, const std::string &path)
{
    // Try to parse the configuration model and start the pipeline
    ipx_config_model model;
    try {
        model = file_parse_model(path);
        conf.start(model);
    } catch (const std::exception &ex) {
        IPX_ERROR(comp_str, "%s", ex.what());
        return EXIT_FAILURE;
    }

    // Wait for a termination signal
    sigset_t mask_new, mask_old;
    sigemptyset(&mask_new);
    sigaddset(&mask_new, SIGINT);
    sigaddset(&mask_new, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &mask_new, &mask_old);

    while (true) {
        int sig;
        if (sigwait(&mask_new, &sig) != 0) { // Waits for _pending_ signals
            IPX_WARNING(comp_str, "sigwait() failed.", '\0');
            continue;
        }

        if (sig == SIGINT || sig == SIGTERM) {
            break;
        }
    }

    IPX_INFO(comp_str, "Received a termination signal.", '\0');
    pthread_sigmask(SIG_SETMASK, &mask_old, NULL);

    // Register a handler that terminates the collector if it is not responding
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = termination_handler;
    if (sigaction(SIGTERM, &sa, NULL) == -1 || sigaction(SIGINT, &sa, NULL) == -1) {
        IPX_ERROR(comp_str, "Failed to register termination signal handlers!", '\0');
    }

    // Stop the pipeline
    conf.stop();
    return EXIT_SUCCESS;
}
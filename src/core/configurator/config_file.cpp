//
// Created by lukashutak on 03/04/18.
//


#include <cstdio>
#include <stdexcept>
#include <memory>

#include <libfds.h>
#include <iostream>
#include "config_file.hpp"
#include "configurator.hpp"
#include "model.hpp"

extern "C" {
#include "../utils.h"
}

enum FILE_XML_NODES {
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


static const struct fds_xml_args args_instance_input[] = {
    FDS_OPTS_ELEM(IN_PLUGIN_NAME,      "name",       FDS_OPTS_T_STRING, 0),
    FDS_OPTS_ELEM(IN_PLUGIN_PLUGIN,    "plugin",     FDS_OPTS_T_STRING, 0),
    FDS_OPTS_ELEM(IN_PLUGIN_VERBOSITY, "verbosity",  FDS_OPTS_T_STRING, FDS_OPTS_P_OPT),
    FDS_OPTS_RAW( IN_PLUGIN_PARAMS,    "params",                        0),
    FDS_OPTS_END
};

static const struct fds_xml_args args_list_inputs[] = {
    FDS_OPTS_NESTED(INSTANCE_INPUT, "input", args_instance_input, FDS_OPTS_P_MULTI),
    FDS_OPTS_END
};


static const struct fds_xml_args args_instance_inter[] = {
    FDS_OPTS_ELEM(INTER_PLUGIN_NAME,      "name",       FDS_OPTS_T_STRING, 0),
    FDS_OPTS_ELEM(INTER_PLUGIN_PLUGIN,    "plugin",     FDS_OPTS_T_STRING, 0),
    FDS_OPTS_ELEM(INTER_PLUGIN_VERBOSITY, "verbosity",  FDS_OPTS_T_STRING, FDS_OPTS_P_OPT),
    FDS_OPTS_RAW( INTER_PLUGIN_PARAMS,    "params",                        0),
    FDS_OPTS_END
};

static const struct fds_xml_args args_list_inter[] = {
    FDS_OPTS_NESTED(INSTANCE_INTER, "intermediate", args_instance_inter, FDS_OPTS_P_MULTI),
    FDS_OPTS_END
};

static const struct fds_xml_args args_instance_output[] = {
    FDS_OPTS_ELEM(OUT_PLUGIN_NAME,        "name",       FDS_OPTS_T_STRING, 0),
    FDS_OPTS_ELEM(OUT_PLUGIN_PLUGIN,      "plugin",     FDS_OPTS_T_STRING, 0),
    FDS_OPTS_ELEM(OUT_PLUGIN_VERBOSITY,   "verbosity",  FDS_OPTS_T_STRING, FDS_OPTS_P_OPT),
    FDS_OPTS_ELEM(OUT_PLUGIN_ODID_EXCEPT, "odidExcept", FDS_OPTS_T_STRING, FDS_OPTS_P_OPT),
    FDS_OPTS_ELEM(OUT_PLUGIN_ODID_ONLY,   "odidOnly",   FDS_OPTS_T_STRING, FDS_OPTS_P_OPT),
    FDS_OPTS_RAW( OUT_PLUGIN_PARAMS,      "params",                        0),
    FDS_OPTS_END
};

static const struct fds_xml_args args_list_output[] = {
    FDS_OPTS_NESTED(INSTANCE_OUTPUT, "output", args_instance_output, FDS_OPTS_P_MULTI),
    FDS_OPTS_END
};

static const struct fds_xml_args args_main[] = {
    FDS_OPTS_ROOT("ipfixcol2"),
    FDS_OPTS_NESTED(LIST_INPUTS, "inputPlugins",        args_list_inputs, 0),
    FDS_OPTS_NESTED(LIST_INTER,  "intermediatePlugins", args_list_inter, FDS_OPTS_P_OPT),
    FDS_OPTS_NESTED(LIST_OUTPUT, "outputPlugins",       args_list_output, 0),
    FDS_OPTS_END
};

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
            throw std::logic_error("Unexpected XML node within <input>!");
        }
    }

    model.add_instance(input);
}

static void
file_parse_list_input(fds_xml_ctx_t *ctx, ipx_config_model &model)
{
    unsigned int cnt = 0;
    const struct fds_xml_cont *content;

    while (fds_xml_next(ctx, &content) != FDS_EOC) {
        // Process an input plugin
        cnt++;

        if (content->id != INSTANCE_INPUT) {
            throw std::logic_error("Unexpected XML node! Expected <input>.");
        }

        try {
            file_parse_instance_input(content->ptr_ctx, model);
        } catch (std::exception &ex) {
            throw std::runtime_error("Failed to parse configuration of "
                + std::to_string(cnt) + ". input plugin: " + ex.what());
        }
    }
}

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
            throw std::logic_error("Unexpected XML node within <intermediate>!");
        }
    }

    model.add_instance(inter);
}

static void
file_parse_list_inter(fds_xml_ctx_t *ctx, ipx_config_model &model)
{
    unsigned int cnt = 0;
    const struct fds_xml_cont *content;

    while (fds_xml_next(ctx, &content) != FDS_EOC) {
        // Process an intermediate plugin
        cnt++;

        if (content->id != INSTANCE_INTER) {
            throw std::logic_error("Unexpected XML node! Expected <intermediate>.");
        }

        try {
            file_parse_instance_inter(content->ptr_ctx, model);
        } catch (std::exception &ex) {
            throw std::runtime_error("Failed to parse configuration of "
                + std::to_string(cnt) + ". intermediate plugin: " + ex.what());
        }
    }
}


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
            throw std::logic_error("Unexpected XML node within <output>!");
        }
    }

    model.add_instance(output);
}

static void
file_parse_list_output(fds_xml_ctx_t *ctx, ipx_config_model &model)
{
    unsigned int cnt = 0;
    const struct fds_xml_cont *content;

    while (fds_xml_next(ctx, &content) != FDS_EOC) {
        // Process an output plugin
        cnt++;

        if (content->id != INSTANCE_OUTPUT) {
            throw std::logic_error("Unexpected XML node! Expected <output>.");
        }

        try {
            file_parse_instance_output(content->ptr_ctx, model);
        } catch (std::exception &ex) {
            throw std::runtime_error("Failed to parse configuration of "
                + std::to_string(cnt) + ". output plugin: " + ex.what());
        }
    }
}

static ipx_config_model
file_parse_model(const std::string &path)
{
    // Load content of the configuration file
    std::unique_ptr<FILE, decltype(&fclose)> stream(fopen(path.c_str(), "r"), &fclose);
    if (!stream) {
        // Failed to open file
        std::string err_msg = "Unable to open file '" + path + "'";
        throw std::runtime_error(err_msg);
    }

    // Load whole content of the file
    fseek(stream.get(), 0, SEEK_END);
    long fsize = ftell(stream.get());
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
            throw std::logic_error("Unexpected XML node within startup <ipfixcol2>!");
        }
    }

    return model;
}

int
ipx_config_file(Configurator &conf, const std::string &path)
{
    ipx_config_model model = file_parse_model(path);
    model.dump();
    
    // TODO: pass the model to the configuration

    return EXIT_SUCCESS;
}
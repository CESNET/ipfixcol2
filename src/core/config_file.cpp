//
// Created by lukashutak on 03/04/18.
//

#define _GNU_SOURCE
#include <cstring>

#include <cstdio>
#include <stdexcept>
#include <memory>

#include <libfds.h>
#include <iostream>

extern "C" {
    #include "configurator.h"
    #include "utils.h"
    #include "config_file.h"
}

enum FILE_XML_NODES {
    LIST_INPUTS = 1,
    LIST_INTER,
    LIST_OUTPUT,
    INSTANCE_INPUT,
    INSTANCE_INTER,
    INSTANCE_OUTPUT,
    IN_PLUGIN_NAME,
    IN_PLUGIN_PLUGIN,
    IN_PLUGIN_PARAMS,
    IN_PLUGIN_VERBOSITY,
    INTER_PLUGIN_NAME,
    INTER_PLUGIN_PLUGIN,
    INTER_PLUGIN_PARAMS,
    INTER_PLUGIN_VERBOSITY,
    OUT_PLUGIN_NAME,
    OUT_PLUGIN_PLUGIN,
    OUT_PLUGIN_PARAMS,
    OUT_PLUGIN_VERBOSITY,
    OUT_PLUGIN_ODID_ONLY,
    OUT_PLUGIN_ODID_EXC,
};


static const struct fds_xml_args args_instance_input[] = {
    OPTS_ELEM(IN_PLUGIN_NAME,      "name",       OPTS_T_STRING, 0),
    OPTS_ELEM(IN_PLUGIN_PLUGIN,    "plugin",     OPTS_T_STRING, 0),
    OPTS_ELEM(IN_PLUGIN_VERBOSITY, "verbosity",  OPTS_T_STRING, OPTS_P_OPT),
    OPTS_RAW(IN_PLUGIN_PARAMS,     "params",     0),
    OPTS_END
};

static const struct fds_xml_args args_list_inputs[] = {
    OPTS_NESTED(INSTANCE_INPUT, "input", args_instance_input, OPTS_P_MULTI),
    OPTS_END
};


static const struct fds_xml_args args_instance_inter[] = {
    OPTS_ELEM(INTER_PLUGIN_NAME,      "name",       OPTS_T_STRING, 0),
    OPTS_ELEM(INTER_PLUGIN_PLUGIN,    "plugin",     OPTS_T_STRING, 0),
    OPTS_ELEM(INTER_PLUGIN_VERBOSITY, "verbosity",  OPTS_T_STRING, OPTS_P_OPT),
    OPTS_RAW(INTER_PLUGIN_PARAMS,     "params",     0),
    OPTS_END
};

static const struct fds_xml_args args_list_inter[] = {
    OPTS_NESTED(INSTANCE_INTER, "intermediate", args_instance_inter, OPTS_P_MULTI),
    OPTS_END
};

static const struct fds_xml_args args_instance_output[] = {
    OPTS_ELEM(OUT_PLUGIN_NAME,      "name",       OPTS_T_STRING, 0),
    OPTS_ELEM(OUT_PLUGIN_PLUGIN,    "plugin",     OPTS_T_STRING, 0),
    OPTS_ELEM(OUT_PLUGIN_VERBOSITY, "verbosity",  OPTS_T_STRING, OPTS_P_OPT),
    OPTS_ELEM(OUT_PLUGIN_ODID_EXC,  "odidExcept", OPTS_T_STRING, OPTS_P_OPT),
    OPTS_ELEM(OUT_PLUGIN_ODID_ONLY, "odidOnly",   OPTS_T_STRING, OPTS_P_OPT),
    OPTS_RAW(OUT_PLUGIN_PARAMS,     "params",     0),
    OPTS_END
};

static const struct fds_xml_args args_list_output[] = {
    OPTS_NESTED(INSTANCE_OUTPUT, "output", args_instance_output, OPTS_P_MULTI),
    OPTS_END
};

static const struct fds_xml_args args_main[] = {
    OPTS_ROOT("ipfixcol2"),
    OPTS_NESTED(LIST_INPUTS, "inputPlugins", args_list_inputs, 0),
    OPTS_NESTED(LIST_INTER,  "intermediatePlugins", args_list_inter, OPTS_P_OPT),
    OPTS_NESTED(LIST_OUTPUT, "outputPlugins", args_list_output, 0),
    OPTS_END
};

void
ipx_file_parse(const char *pathname)
{
    // Load content of the configuration file
    std::unique_ptr<FILE, decltype(&fclose)> stream(fopen(pathname, "r"), &fclose);
    if (!stream) {
        // Failed to open file
        std::string err_msg = "Unable to open file '" + std::string(pathname) + "'";
        throw std::runtime_error(err_msg);
    }

    // Load whole content of the file
    fseek(stream.get(), 0, SEEK_END);
    long fsize = ftell(stream.get());
    rewind(stream.get());

    std::unique_ptr<char[]> fcontent(new char[fsize + 1]);
    if (fread(fcontent.get(), fsize, 1, stream.get()) != 1) {
        throw std::runtime_error("fread() failed to read configuration file.");
    }
    fcontent.get()[fsize] = '\0';

    fds_xml_t *xml_parser;
    if (fds_xml_create(&xml_parser) != FDS_OK) {
        throw std::runtime_error("fds_xml_create() failed!");
    }

    std::unique_ptr<fds_xml_t, decltype(&fds_xml_destroy)> parser(xml_parser, &fds_xml_destroy);
    if (fds_xml_set_args(args_main, xml_parser) != FDS_OK) {
        throw std::runtime_error("fds_xml_set_args() failed: " + std::string(fds_xml_last_err(xml_parser)));
    }

    fds_xml_ctx_t *ctx = fds_xml_parse_mem(parser.get(), fcontent.get(), true);
    if (!ctx) {
        throw std::runtime_error("Failed to parse configuration: " + std::string(fds_xml_last_err(xml_parser)));
    }



}

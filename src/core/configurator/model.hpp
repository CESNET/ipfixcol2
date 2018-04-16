/**
 * \file src/core/configurator/model.hpp
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Parsed configuration model (header file)
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
#ifndef IPFIXCOL_MODEL_H
#define IPFIXCOL_MODEL_H

#include <string>
#include <vector>
#include <ipfixcol2.h>

extern "C" {
#include "../odid_range.h"
}

/** Common plugin configuration parameters                                  */
struct ipx_plugin_base {
    /** Identification name of the plugin                                   */
    std::string plugin;
    /** Identification name of the instance                                 */
    std::string name;
    /** XML parameters (root node "\<param\>")                              */
    std::string params;
    /** Verbosity mode (if empty, use default)                              */
    std::string verbosity;
};

/** Configuration of an input plugin                                          */
struct ipx_plugin_input  : ipx_plugin_base {};

/** Configuration of an intermediate plugin                                   */
struct ipx_plugin_inter  : ipx_plugin_base {};

/** Configuration of an output plugin                                         */
struct ipx_plugin_output : ipx_plugin_base {
    /** ODID filter type                                                      */
    enum ipx_odid_filter_type odid_type;
    /** ODID filter expression                                                */
    std::string odid_expression;
};

/** Parsed configuration of the collector                                      */
class ipx_config_model {
    friend class ipx_configurator;
private:
    /** List of instances of input plugins                                     */
    std::vector<struct ipx_plugin_input>  inputs;
    /** List of instances of intermediate plugins                              */
    std::vector<struct ipx_plugin_inter>  inters;
    /** List of instances of output plugins                                    */
    std::vector<struct ipx_plugin_output> outputs;

    void check_common(struct ipx_plugin_base *base);
public:
    ipx_config_model() = default;
    ~ipx_config_model() = default;

    /** \brief Dump model to the standard output */
    void dump();

    /**
     * \brief Add an instance of an input plugin
     * \param[in] instance Instance
     * \throw invalid_argument if there is any obvious configuration error
     */
    void add_instance(struct ipx_plugin_input &instance);
    /**
     * \brief Add an instance of an intermediate plugin
     * \param[in] instance Instance
     * \throw invalid_argument if there is any obvious configuration error
     */
    void add_instance(struct ipx_plugin_inter &instance);
    /**
     * \brief Add an instance of an output plugin
     * \param[in] instance Instance
     * \throw invalid_argument if there is any obvious configuration error
     */
    void add_instance(struct ipx_plugin_output &instance);
};

#endif //IPFIXCOL_MODEL_H

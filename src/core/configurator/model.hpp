//
// Created by lukashutak on 12/04/18.
//

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

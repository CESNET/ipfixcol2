/**
 * @file src/core/configurator/controller_file.hpp
 * @author Lukas Hutak <lukas.hutak@cesnet.cz>
 * @brief Configuration controller for file based configuration (header file)
 * @date 2019
 *
 * Copyright(c) 2019 CESNET z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef IPFIXCOL2_CONTROLLER_FILE
#define IPFIXCOL2_CONTROLLER_FILE

#include <libfds.h>

#include "model.hpp"
#include "controller.hpp"

/**
 * @brief Implementation of the controller based on a configuration file
 *
 * The controller is able to parse the file and create a configuration model based on it.
 */
class ipx_controller_file : public ipx_controller {
public:
    /**
     * @brief Controller constructor
     * @param path Path to the configuration file
     */
    ipx_controller_file(std::string path);

    /**
     * @brief Parse the configuration file and create a model
     * @return Configuration model
     * @throw ipx_controller::error if the file is doesn't exist or it is malformed
     */
    ipx_config_model
    model_get() override;

private:
    /// Path to the configuration file
    std::string m_path;

    // Internal functions
    static ipx_config_model
    parse_file(const std::string &path);

    static void
    parse_list_input(fds_xml_ctx_t *ctx, ipx_config_model &model);
    static void
    parse_list_inter(fds_xml_ctx_t *ctx, ipx_config_model &model);
    static void
    parse_list_output(fds_xml_ctx_t *ctx, ipx_config_model &model);

    static void
    parse_instance_input(fds_xml_ctx_t *ctx, ipx_config_model &model);
    static void
    parse_instance_inter(fds_xml_ctx_t *ctx, ipx_config_model &model);
    static void
    parse_instance_output(fds_xml_ctx_t *ctx, ipx_config_model &model);
};

#endif //IPFIXCOL2_CONTROLLER_FILE

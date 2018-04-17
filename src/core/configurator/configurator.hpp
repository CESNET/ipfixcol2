/**
 * \file src/core/configurator/configurator.hpp
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Main pipeline configurator (header file)
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

#ifndef IPFIXCOL_CONFIGURATOR_H
#define IPFIXCOL_CONFIGURATOR_H

#include <stdint.h>
#include <vector>
#include <memory>
#include "plugin_finder.hpp"
#include "model.hpp"
#include "instance.hpp"

extern "C" {
#include <ipfixcol2.h>
#include "../context.h"
}

/** Main configurator of the internal pipeline                                                */
class ipx_configurator {
private:
    /** Size of ring buffers                                                                   */
    uint32_t ring_size;
    /** Directory with definitions of Information Elements                                     */
    std::string iemgr_dir;

    /** Manager of Information Elements                                                        */
    fds_iemgr_t *iemgr;
    /** Vector of running instances of input plugins                                           */
    std::vector<std::unique_ptr<ipx_instance_input> > running_inputs;
    /** Vector of running instances of intermediate plugins                                    */
    std::vector<std::unique_ptr<ipx_instance_intermediate> > running_inter;
    /** Vector of running instances of output plugins                                          */
    std::vector<std::unique_ptr<ipx_instance_output> > running_outputs;

    void model_check(const ipx_config_model &model);
    fds_iemgr_t *iemgr_load(const std::string dir);
    enum ipx_verb_level verbosity_str2level(const std::string &verb);

public:
    /** Minimal size of ring buffers between instances of plugins                              */
    static constexpr uint32_t RING_MIN_SIZE = 128;
    /** Default size of ring buffers between instances of plugins                              */
    static constexpr uint32_t RING_DEF_SIZE = 8096;

    /** Constructor */
    ipx_configurator();
    /** Destructor  */
    ~ipx_configurator();

    // Disable copy constructors
    ipx_configurator(const ipx_configurator &) = delete;
    ipx_configurator& operator=(const ipx_configurator &) = delete;

    /** Modules finder */
    ipx_plugin_finder finder;

    /**
     * \brief Start the configuration with a new model
     *
     * Check if the model is valid (consists of at least one input and one output instance,
     * etc.) and load definitions of Information Elements (i.e. iemgr_set_dir() must be defined).
     * Then try to load all plugins, initialized them and, finally, start their execution threads.
     *
     * \param[in] model Configuration model (description of input/intermediate/output instances)
     * \throw runtime_error in case of any error, e.g. failed to find plugins, failed to load
     *   definitions of Information Elements, failed to initialize an instance, etc.
     */
    void start(const ipx_config_model &model);

    /**
     * \brief Stop and destroy all instances of all plugins
     */
    void stop();

    /**
     * \brief Define a path to the directory of Information Elements definitions
     * \param[in] path Path
     */
     void iemgr_set_dir(const std::string &path);
     /**
      * \brief Define a size of ring buffers
      * \param[in] size Size
      */
     void set_buffer_size(uint32_t size);
};


#endif //IPFIXCOL_CONFIGURATOR_H

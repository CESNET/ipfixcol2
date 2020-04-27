/**
 * \file src/core/configurator/instance_input.hpp
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Input instance wrapper (header file)
 * \date 2018-2020
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

#ifndef IPFIXCOL_INSTANCE_INPUT_HPP
#define IPFIXCOL_INSTANCE_INPUT_HPP

#include <memory>
#include "instance.hpp"

extern "C" {
#include "../fpipe.h"
}

/** Unique pointer type of a feedback pipe     */
using unique_fpipe = std::unique_ptr<ipx_fpipe_t, decltype(&ipx_fpipe_destroy)>;

/**
 * \brief Instance of an input plugin
 *
 * The class takes care of (i.e. initialize, configure and destroy):
 * - a plugin context of an input plugin
 * - a plugin context of the IPFIX message parser (implemented as an internal plugin)
 * - a ring buffer connecting input and parser instances
 * - a feedback pipe (for collector core to send termination/reconfiguration messages
 *   and for the IPFIX message parser to send request to close a Transport Session)
 *
 * \note
 *   The output ring buffer is not defined and MUST be connected before the plugin instances
 *   can be initialized!
 * \note The IPFIX parser is connected to the input feedback pipe only if the input plugin
 *   implements the interface for processing request to close a Transport Session
 *
 * \verbatim
 *                   (optional feedback)
 *                +------------------------+
 *                |                        |
 *                |     +-------+      +---+----+
 *                |     |       |      |        |
 *          +-----v-----> Input +------> Parser +----->
 *             feedback |       | ring |        | (output not set yet)
 *                      +-------+      +--------+
 * \endverbatim
 *
 */
class ipx_instance_input : public ipx_instance {
protected:
    /** Feedback pipe connected to the input instance                                            */
    ipx_fpipe_t *_input_feedback;

    /** Ring buffer between the instance of an input plugin and instance of the parser           */
    ipx_ring_t  *_parser_buffer;
    /** Instance of the parser plugin (internal)                                                 */
    ipx_ctx_t   *_parser_ctx;

    // Disable copy constructors
    ipx_instance_input(const ipx_instance_input &) = delete;
    ipx_instance_input & operator=(const ipx_instance_input &) = delete;

public:
    /**
     * \brief Create an instance of an input plugin
     *
     * \note
     *   The \p ref is plugin reference wrapper of the plugin. The wrapper helps to monitor number
     *   of plugins that use the plugin. The reference will be destroyed during this destruction
     *   of the object of this class.
     * \param[in] name   Name of the instance
     * \param[in] ref    Reference to the plugin (will be automatically delete on destroy)
     * \param[in] bsize  Size of the ring buffer between the input instance and the parser instance
     */
    ipx_instance_input(const std::string &name, ipx_plugin_mgr::plugin_ref *ref, uint32_t bsize);
    /**
     * \brief Destroy the instance
     * \note
     *   If the thread is running (start() has been called), the function blocks until the thread
     *   is terminated.
     */
    ~ipx_instance_input();

    /**
     * \brief Initialize the instance
     *
     * Initialize a context of the input instance and the IPFIX parser.
     * \param[in] params XML parameters of the instance
     * \param[in] iemgr  Reference to the manager of Information Elements
     * \param[in] level  Verbosity level
     * \throw runtime_error if the function fails to initialize all components or if an output
     *   plugin is not connected
     */
    void init(const std::string &params, const fds_iemgr_t *iemgr, ipx_verb_level level);

    /**
     * \brief Start a thread of the instance
     * \throw runtime_error if a thread fails to the start
     */
    void start();

    /**
     * \brief Get a feedback pipe (for writing only)
     * \return Pointer to the pipe
     */
    ipx_fpipe_t *get_feedback();

    /**
     * \brief Connect the the input instance to an instance of an intermediate plugin
     * \param[in] intermediate Intermediate plugin to receive our messages
     */
    void connect_to(ipx_instance_intermediate &intermediate);

    /**
     * \brief Registered extensions and dependencies
     * \param[in] ext_mgr Extension manager
     */
    void
    extensions_register(ipx_cfg_extensions *ext_mgr, size_t pos) override;

    /**
     * \brief Resolve definition of the extension/dependency definitions of the instance
     *
     * Description of each extension/dependency is updated to contain size, offset, etc.
     * \param[in] ext_mgr Extension manager
     */
    void
    extensions_resolve(ipx_cfg_extensions *ext_mgr) override;

    /**
     * \brief Enable/disable processing of data messages by the plugin (IPFIX and Transport Session)
     *
     * \warning This doesn't affect NetFlow/IPFIX Message parser, see set_parser_processing()
     * \note By default, data processing is enabled.
     * \see ipx_ctx_processing() for more details
     * \param[in] en Enable/disable processing
     */
    void
    set_processing(bool en) override;

    /**
     * \brief Enable/disable processing of data messages by the parser (IPFIX and Transport Session)
     *
     * \warning This doesn't affect the input plugin, see set_processing()
     * \note By default, data processing is enabled.
     * \see ipx_ctx_processing() for more details
     * \param[in] en Enable/disable processing
     */
    void
    set_parser_processing(bool en);
};

#endif //IPFIXCOL_INSTANCE_INPUT_HPP

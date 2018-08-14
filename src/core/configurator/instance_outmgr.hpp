/**
 * \file src/core/configurator/instance_outmgr.hpp
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Output manager instance wrapper (header file)
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

#ifndef IPFIXCOL_INSTANCE_OUTMGR_HPP
#define IPFIXCOL_INSTANCE_OUTMGR_HPP

#include "instance_intermediate.hpp"
#include "instance_output.hpp"

extern "C" {
#include "../plugin_output_mgr.h"
}

/**
 * \brief Instance of the output manager
 *
 * The class takes care of (i.e. initialize, configure and destroy):
 * - a plugin context of the output manager (implemented as an internal plugin)
 * - an input ring buffer (inherited from the base class)
 * - a list of connected output instances (with optional ODID filters)
 *
 * \note
 *   The output manager must be connected to at least one output instance before it can be
 *   initialized!
 *
 * \verbatim
 *                +--------+
 *                |        +-->
 *         +------> Output |    list
 *          ring  |  mgr.  +-->
 *                +--------+
 * \endverbatim
 */
class ipx_instance_outmgr : public ipx_instance_intermediate {
private:
    /** List of output plugins                                                                   */
    ipx_output_mgr_list_t *_list;
    /**
     * \brief Output plugin cannot be connected to any other instance
     * \param[in] intermediate Intermediate plugin
     */
    void connect_to(ipx_instance_intermediate &intermediate);
public:
    /**
     * \brief Create an instance of the internal output manager plugin
     * \param[in] bsize  Size of the input ring buffer
     */
    explicit ipx_instance_outmgr(uint32_t bsize);
    /**
     * \brief Destroy the instance
     *   If the thread is running (start() has been called), the function blocks until the thread
     *   is exited.
     */
    ~ipx_instance_outmgr();

    // Disable copy constructors
    ipx_instance_outmgr(const ipx_instance_outmgr &) = delete;
    ipx_instance_outmgr & operator=(const ipx_instance_outmgr &) = delete;

    /**
     * \brief Initialize the instance
     *
     * Initialize a context of the plugin and an input ring buffer.
     * \note
     *   The instance MUST be connected connect_to() to at least one output plugin!
     * \param[in] iemgr  Reference to the manager of Information Elements
     * \param[in] level  Verbosity level
     * \throw runtime_error if the function fails to initialize all components or if no output
     *   plugins are connected.
     */
    void init(const fds_iemgr_t *iemgr, ipx_verb_level level);

    /**
     * \brief Connect the the output manager to an instance of an output plugin
     * \param[in] output Output plugin to receive our messages
     * \throw runtime_error if creating of the connection fails
     */
    void connect_to(ipx_instance_output &output);
};

#endif //IPFIXCOL_INSTANCE_OUTMGR_HPP

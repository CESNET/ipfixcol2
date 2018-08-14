/**
 * \file src/core/configurator/instance_output.hpp
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Output instance wrapper (header file)
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

#ifndef IPFIXCOL_INSTANCE_OUTPUT_HPP
#define IPFIXCOL_INSTANCE_OUTPUT_HPP

#include <memory>
#include "instance.hpp"

extern "C" {
#include "../odid_range.h"
}

/** Unique pointer type of an ODID filter      */
using unique_orange = std::unique_ptr<ipx_orange_t, decltype(&ipx_orange_destroy)>;

/**
 * \brief Instance of the output plugin
 *
 * The class takes care of (i.e. initialize, configure and destroy):
 * - a plugin context of an output plugin
 * - an input ring buffer
 * - an ODID filter (only if configured)
 *
 * \verbatim
 *              +--------+
 *              |        |
 *       +------> Output |
 *         ring |        |
 *              +--------+
 * \endverbatim
 */
class ipx_instance_output : public ipx_instance {
protected:
    /** Input ring buffer                                                                        */
    ipx_ring_t *_instance_buffer;

    /** ODID filter type                                                                         */
    enum ipx_odid_filter_type _type;
    /** ODID filter (nullptr, if type == IPX_ODID_FILTER_NONE                                    */
    ipx_orange_t *_filter;
public:
    /**
     * \brief Create an instance of an output plugin
     *
     * \note
     *   The \p ref is plugin reference wrapper of the plugin. The wrapper helps to monitor number
     *   of plugins that use the plugin. The reference will be destroyed during this destruction
     *   of the object of this class.
     * \param[in] name   Name of the instance
     * \param[in] ref    Reference to the plugin (will be automatically delete on destroy)
     * \param[in] bsize  Size of the input ring buffer
     */
    ipx_instance_output(const std::string &name, ipx_plugin_mgr::plugin_ref *ref,
        uint32_t bsize);
    /**
     * \brief Destroy the instance
     * \note
     *   If the thread is running (start() has been called), the function blocks until the thread
     *   is exited.
     */
    ~ipx_instance_output();

    // Disable copy constructors
    ipx_instance_output(const ipx_instance_output &) = delete;
    ipx_instance_output & operator=(const ipx_instance_output &) = delete;

    /**
     * \brief Set ODID filter expression (disabled by default)
     * \param[in] type Filter type
     * \param[in] expr Filter expression
     */
    void set_filter(ipx_odid_filter_type type, const std::string &expr);

    /**
     * \brief Initialize the instance
     *
     * Initialize a context of the plugin and an input ring buffer.
     * \note The ODID filter MUST be set before calling this initialization!
     * \param[in] params XML parameters of the instance
     * \param[in] iemgr Reference to the manager of Information Elements
     * \param[in] level Verbosity level
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
     * \brief Get the input ring buffer (for writing only)
     * \warning
     *   Do NOT use if there is already another active writer.
     * \return Pointer to the ring buffer and the ODID filter.
     */
    std::tuple<ipx_ring_t *, enum ipx_odid_filter_type, const ipx_orange_t *>
    get_input();
};

#endif //IPFIXCOL_INSTANCE_OUTPUT_HPP

/**
 * \file src/core/configurator/instance_intermediate.hpp
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Intermediate instance wrapper (header file)
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

#ifndef IPFIXCOL_INSTANCE_INTERMEDIATE_HPP
#define IPFIXCOL_INSTANCE_INTERMEDIATE_HPP

#include "instance.hpp"
#include "instance_input.hpp"

/**
 * \brief Instance of an intermediate plugin
 *
 * The class takes care of (i.e. initialize, configure and destroy):
 * - a plugin context of an intermediate plugin
 * - an input ring buffer
 *
 * \note
 *   The output ring buffer is not defined and MUST be connected before the plugin instance
 *   can be initialized!
 * \warning
 *   As an input one intermediate plugin or multiple input plugins can be connected
 *   However, never try to combine input and intermediate plugins at the same time!
 *
 * \verbatim
 *                  +-------+
 *                  |       |
 *            +-----> Inter +----->
 *             ring |       | (output not set yet)
 *                  +-------+
 * \endverbatim
 */
class ipx_instance_intermediate : public ipx_instance {
private:
    void internals_init(const struct ipx_ctx_callbacks *cbs, uint32_t bsize);
protected:
    /** Allow connector to enable multi-write mode                                               */
    friend void ipx_instance_input::connect_to(ipx_instance_intermediate &intermediate);

    /** Input ring buffer                                                                        */
    ipx_ring_t *_instance_buffer;
    /** Number of connection inputs                                                              */
    unsigned int _inputs_cnt;
public:
    /**
     * \brief Create an instance of an intermediate plugin
     *
     * \note
     *   The \p ref is plugin reference wrapper of the plugin. The wrapper helps to monitor number
     *   of plugins that use the plugin. The reference will be destroyed during this destruction
     *   of the object of this class.
     * \param[in] name  Name of the instance
     * \param[in] ref   Reference to the plugin (will be automatically delete on destroy)
     * \param[in] bsize Size of the input ring buffer
     */
    ipx_instance_intermediate(const std::string &name, ipx_plugin_mgr::plugin_ref *ref,
        uint32_t bsize);

    /**
     * \brief Create an instance of an intermediate plugin (static internal plugins only)
     *
     * \note This constructor is required by the derived class ipx_instance_outmgr, which doesn't
     *   depend on dynamically loaded plugins. The main difference is that instead of accepting
     *   a plugin reference, the constructor takes directly plugin callbacks.
     * \param[in] name  Name of the instance
     * \param[in] cbs   Plugin callbacks
     * \param[in] bsize Size of the input ring buffer
     */
    ipx_instance_intermediate(const std::string &name, const ipx_ctx_callbacks *cbs,
        uint32_t bsize);

    /**
     * \brief Destroy the instance
     * \note
     *   If the thread is running (start() has been called), the function blocks until the thread
     *   is exited.
     */
    virtual ~ipx_instance_intermediate();

    // Disable copy constructors
    ipx_instance_intermediate(const ipx_instance_intermediate &) = delete;
    ipx_instance_intermediate &operator=(const ipx_instance_intermediate &) = delete;

    /**
     * \brief Initialize the instance
     *
     * Initialize a context of the plugin and an input ring buffer.
     * \note
     *   The instance MUST be connected connect_to() to an intermediate plugin before initialization
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
     * \brief Get the input ring buffer (for writing only)
     * \warning
     *   Do NOT use if there is already another active writer and the ring buffer is not defined
     *   with enabled multi-writer mode!
     * \return Pointer to the ring buffer.
     */
    ipx_ring_t *get_input();

    /**
     * \brief Connect the the intermediate instance to another instance of an intermediate plugin
     * \param[in] intermediate Intermediate plugin to receive our messages
     */
    virtual void connect_to(ipx_instance_intermediate &intermediate);

    /**
     * \brief Get the plugin context (read only)
     */
    virtual const ipx_ctx_t *
    get_ctx() {
        return _ctx;
    }
};

#endif //IPFIXCOL_INSTANCE_INTERMEDIATE_HPP

/**
 * \file src/core/configurator/instance_intermediate.hpp
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Intermediate instance wrapper (source file)
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

#include "instance_intermediate.hpp"

/**
 * \brief Internal components initializer
 *
 * The function prepares plugin context and input ring buffer to be prepared for start.
 * \param[in] cbs   Callback function
 * \param[in] bsize Size of the input ring buffer
 * \throw runtime_error if any component fails to initialize
 */
void
ipx_instance_intermediate::internals_init(const struct ipx_ctx_callbacks *cbs, uint32_t bsize)
{
    unique_ring ring_wrap(ipx_ring_init(bsize, false), &ipx_ring_destroy);
    unique_ctx  inter_wrap(ipx_ctx_create(_name.c_str(), cbs), &ipx_ctx_destroy);
    if (!ring_wrap || !inter_wrap) {
        throw std::runtime_error("Failed to create components of an intermediate instance!");
    }

    // Configure the components (connect them)
    ipx_ctx_ring_src_set(inter_wrap.get(), ring_wrap.get());
    _instance_buffer = ring_wrap.release();
    _ctx = inter_wrap.release();
    _inputs_cnt = 0;
}

ipx_instance_intermediate::ipx_instance_intermediate(const std::string &name,
    ipx_plugin_mgr::plugin_ref *ref, uint32_t bsize)
    : ipx_instance(name, ref) // The base class takes care of the plugin reference
{
    // Get the plugin callbacks
    const ipx_plugin_mgr::plugin *plugin = _plugin_ref->get_plugin();
    const struct ipx_ctx_callbacks *cbs = plugin->get_callbacks();
    assert(cbs != nullptr && plugin->get_type() == IPX_PT_INTERMEDIATE);
    internals_init(cbs, bsize);
}

ipx_instance_intermediate::ipx_instance_intermediate(const std::string &name,
    const ipx_ctx_callbacks *cbs, uint32_t bsize)
    : ipx_instance(name, nullptr) // No plugin reference is passed to the base class
{
    // Pass user defined callbacks
    internals_init(cbs, bsize);
}

ipx_instance_intermediate::~ipx_instance_intermediate()
{
    // Output manager might already destroyed the context
    if (_ctx != nullptr) {
        // Destroy context (if running, wait for termination of threads)
        ipx_ctx_destroy(_ctx);
    }
    // Now we can destroy buffers
    ipx_ring_destroy(_instance_buffer);
}

void
ipx_instance_intermediate::init(const std::string &params, const fds_iemgr_t *iemgr,
    ipx_verb_level level)
{
    assert(iemgr != nullptr);
    assert(_state == state::NEW); // Only not initialized instance can be initialized

    // Configure
    ipx_ctx_verb_set(_ctx, level);
    ipx_ctx_iemgr_set(_ctx, iemgr);

    // Initialize
    if (ipx_ctx_init(_ctx, params.c_str()) != IPX_OK) {
        throw std::runtime_error("Failed to initialize the instance of the intermediate plugin!");
    }
    _state = state::INITIALIZED;
}

void
ipx_instance_intermediate::start()
{
    assert(_state == state::INITIALIZED); // Only initialized instances can start
    if (ipx_ctx_run(_ctx) != IPX_OK) {
        throw std::runtime_error("Failed to start a thread of the intermediate instance.");
    }
    _state = state::RUNNING;
}

ipx_ring_t *
ipx_instance_intermediate::get_input()
{
    return _instance_buffer;
}

void
ipx_instance_intermediate::connect_to(ipx_instance_intermediate &intermediate)
{
    assert(_state == state::NEW); // Only configuration of an uninitialized instance can be changed!
    ipx_ctx_ring_dst_set(_ctx, intermediate.get_input());
}
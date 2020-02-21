/**
 * \file src/core/configurator/instance_output.hpp
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Output instance wrapper (source file)
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

#include "instance_output.hpp"


ipx_instance_output::ipx_instance_output(const std::string &name,
    ipx_plugin_mgr::plugin_ref *ref, uint32_t bsize) : ipx_instance(name, ref)
{
    // Get the plugin callbacks
    const ipx_plugin_mgr::plugin *plugin = _plugin_ref->get_plugin();
    const struct ipx_ctx_callbacks *cbs = plugin->get_callbacks();
    assert(cbs != nullptr && plugin->get_type() == IPX_PT_OUTPUT);

    unique_ring ring_wrap(ipx_ring_init(bsize, false), &ipx_ring_destroy);
    unique_ctx  output_wrap(ipx_ctx_create(name.c_str(), cbs), &ipx_ctx_destroy);
    if (!ring_wrap || !output_wrap) {
        throw std::runtime_error("Failed to create components of an output instance!");
    }

    // Configure the components (connect them)
    ipx_ctx_ring_src_set(output_wrap.get(), ring_wrap.get());
    _instance_buffer = ring_wrap.release();
    _ctx = output_wrap.release();

    // Default parameters
    _type = IPX_ODID_FILTER_NONE;
    _filter = nullptr;
}

ipx_instance_output::~ipx_instance_output()
{
    // Destroy context (if running, wait for termination of threads)
    ipx_ctx_destroy(_ctx);
    // Now we can destroy buffers
    ipx_ring_destroy(_instance_buffer);

    if (_filter == nullptr) {
        return;
    }

    ipx_orange_destroy(_filter);
}

void
ipx_instance_output::set_filter(ipx_odid_filter_type type, const std::string &expr)
{
    assert(_state == state::NEW); // Only configuration of an uninitialized instance can be changed!

    // Delete the previous filter
    if (_filter != nullptr) {
        ipx_orange_destroy(_filter);
        _filter = nullptr;
    }

    if (type == IPX_ODID_FILTER_NONE) {
        return;
    }

    // Parse the expression
    unique_orange filter_wrap(ipx_orange_create(), &ipx_orange_destroy);
    if (!filter_wrap) {
        throw std::runtime_error("Failed to create the ODID filter!");
    }

    if (ipx_orange_parse(filter_wrap.get(), expr.c_str()) != IPX_OK) {
        throw std::runtime_error("Failed to parse the ODID filter expression '" + expr + "'");
    }

    _type = type;
    _filter = filter_wrap.release();
}

void ipx_instance_output::init(const std::string &params, const fds_iemgr_t *iemgr,
    ipx_verb_level level)
{
    assert(_state == state::NEW); // Only not initialized instance can be initialized
    assert(iemgr != nullptr);

    // Configure
    ipx_ctx_verb_set(_ctx, level);
    ipx_ctx_iemgr_set(_ctx, iemgr);

    // Initialize
    if (ipx_ctx_init(_ctx, params.c_str()) != IPX_OK) {
        throw std::runtime_error("Failed to initialize the instance of the output plugin!");
    }
    _state = state::INITIALIZED;
}

void
ipx_instance_output::start()
{
    assert(_state == state::INITIALIZED); // Only initialized instances can start
    if (ipx_ctx_run(_ctx) != IPX_OK) {
        throw std::runtime_error("Failed to start a thread of the output instance.");
    }
    _state = state::RUNNING;
}

std::tuple<ipx_ring_t *, enum ipx_odid_filter_type, const ipx_orange_t *>
ipx_instance_output::get_input()
{
    return std::make_tuple(_instance_buffer, _type, _filter);
}
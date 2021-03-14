/**
 * \file src/core/configurator/instance_input.cpp
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Input instance wrapper (source file)
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

#include "instance_input.hpp"
#include "instance_intermediate.hpp"

extern "C" {
#include "../plugin_parser.h"
}

/** Description of the internal parser plugin                                                    */
static const struct ipx_ctx_callbacks parser_callbacks = {
    // Static plugin, no library handles
    nullptr,
    &ipx_plugin_parser_info,
    // Only basic functions
    &ipx_plugin_parser_init,
    &ipx_plugin_parser_destroy,
    nullptr, // No getter
    &ipx_plugin_parser_process,
    nullptr  // No feedback
};

ipx_instance_input::ipx_instance_input(const std::string &name, ipx_plugin_mgr::plugin_ref *ref,
    uint32_t bsize) : ipx_instance(name, ref)
{
    // Get the plugin callbacks
    const ipx_plugin_mgr::plugin *plugin = _plugin_ref->get_plugin();
    const struct ipx_ctx_callbacks *cbs = plugin->get_callbacks();
    assert(cbs != nullptr && plugin->get_type() == IPX_PT_INPUT);

    // Create all components
    std::string pname = name + " (parser)";
    unique_fpipe feedback(ipx_fpipe_create(), &ipx_fpipe_destroy);
    unique_ring  ring_wrap(ipx_ring_init(bsize, false), &ipx_ring_destroy);
    unique_ctx   input_wrap(ipx_ctx_create(name.c_str(), cbs), &ipx_ctx_destroy);
    unique_ctx   parser_wrap(ipx_ctx_create(pname.c_str(), &parser_callbacks), &ipx_ctx_destroy);
    if (!feedback || !ring_wrap || !parser_wrap || !input_wrap) {
        throw std::runtime_error("Failed to create components of an input instance!");
    }

    // Configure the components (connect them)
    ipx_ctx_fpipe_set(input_wrap.get(), feedback.get());
    ipx_ctx_ring_dst_set(input_wrap.get(), ring_wrap.get());
    ipx_ctx_ring_src_set(parser_wrap.get(), ring_wrap.get());

    // If the input plugin supports processing of request to close a Transport Session
    if (cbs->ts_close != nullptr) {
        // Allow the parser sending request to close a Transport Session
        ipx_ctx_fpipe_set(parser_wrap.get(), feedback.get());
    }

    // Success
    _ctx = input_wrap.release();
    _input_feedback = feedback.release();
    _parser_ctx = parser_wrap.release();
    _parser_buffer = ring_wrap.release();
}

ipx_instance_input::~ipx_instance_input()
{
    // Destroy context (if running, wait for termination of threads)
    ipx_ctx_destroy(_ctx);
    ipx_ctx_destroy(_parser_ctx);

    // Now we can destroy buffers
    ipx_fpipe_destroy(_input_feedback);
    ipx_ring_destroy(_parser_buffer);
}

void
ipx_instance_input::init(const std::string &params, const fds_iemgr_t *iemgr, ipx_verb_level level)
{
    assert(iemgr != nullptr);
    assert(_state == state::NEW); // Only not initialized instance can be initialized

    // Configure
    ipx_ctx_verb_set(_ctx, level);
    ipx_ctx_iemgr_set(_ctx, iemgr);
    ipx_ctx_verb_set(_parser_ctx, level);
    ipx_ctx_iemgr_set(_parser_ctx, iemgr);

    // Initialize
    if (ipx_ctx_init(_parser_ctx, nullptr) != IPX_OK) {
        throw std::runtime_error("Failed to initialize the parser of IPFIX Messages!");
    }

    if (ipx_ctx_init(_ctx, params.c_str()) != IPX_OK) {
        throw std::runtime_error("Failed to initialize the instance of the input plugin!");
    }

    _state = state::INITIALIZED;
}

void
ipx_instance_input::start()
{
    assert(_state == state::INITIALIZED); // Only initialized instances can start

    /* FIXME: if the parser has stared but input plugin fails to start, stop the parser
     *        (probably by sending a termination message)                              */
    if (ipx_ctx_run(_parser_ctx) != IPX_OK || ipx_ctx_run(_ctx) != IPX_OK) {
        throw std::runtime_error("Failed to start a thread of the input instance.");
    }

    _state = state::RUNNING;
}

ipx_fpipe_t *
ipx_instance_input::get_feedback()
{
    return _input_feedback;
}

void
ipx_instance_input::connect_to(ipx_instance_intermediate &intermediate)
{
    // Only configuration of uninitialized instances can be changed!
    assert(_state == state::NEW && intermediate._state == state::NEW);
    ipx_ctx_ring_dst_set(_parser_ctx, intermediate.get_input());

    intermediate._inputs_cnt++;
    if (intermediate._inputs_cnt > 1) {
        // Multiple writers (it's OK to check these values because instances are not running)
        ipx_ring_mw_mode(intermediate.get_input(), true);
        ipx_ctx_term_cnt_set(intermediate._ctx, intermediate._inputs_cnt);
    }
}

void
ipx_instance_input::extensions_register(ipx_cfg_extensions *ext_mgr, size_t pos)
{
    ext_mgr->register_instance(_ctx, pos);
    ext_mgr->register_instance(_parser_ctx, pos);
}

void
ipx_instance_input::extensions_resolve(ipx_cfg_extensions *ext_mgr)
{
    ext_mgr->update_instance(_ctx);
    ext_mgr->update_instance(_parser_ctx);
}

void
ipx_instance_input::set_processing(bool en)
{
    ipx_ctx_processing_set(_ctx, en);

}

void
ipx_instance_input::set_parser_processing(bool en)
{
    ipx_ctx_processing_set(_parser_ctx, en);
}
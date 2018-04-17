/**
 * \file src/core/configurator/instance.cpp
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Pipeline instance wrappers (source file)
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

#include <memory>
#include "instance.hpp"

extern "C" {
#include "../plugin_parser.h"
#include "../plugin_output_mgr.h"
}

/** Unique pointer type of an instance context */
using unique_ctx = std::unique_ptr<ipx_ctx_t, decltype(&ipx_ctx_destroy)>;
/** Unique pointer type of a feedback pipe     */
using unique_fpipe = std::unique_ptr<ipx_fpipe_t, decltype(&ipx_fpipe_destroy)>;
/** Unique pointer type of a ring buffer       */
using unique_ring = std::unique_ptr<ipx_ring_t, decltype(&ipx_ring_destroy)>;
/** Unique pointer type of an ODID filter      */
using unique_orange = std::unique_ptr<ipx_orange_t, decltype(&ipx_orange_destroy)>;

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

/** Description of the internal output manager                                                   */
static const struct ipx_ctx_callbacks output_mgr_callbacks = {
    // Static plugin, no library handles
    nullptr,
    &ipx_plugin_output_mgr_info,
    // Only basic functions
    &ipx_plugin_output_mgr_init,
    &ipx_plugin_output_mgr_destroy,
    nullptr, // No getter
    &ipx_plugin_output_mgr_process,
    nullptr  // No feedback
};

// -------------------------------------------------------------------------------------------------

ipx_instance::ipx_instance(const std::string &name) : _name(name), _ctx(nullptr)
{
    _state = state::NEW;
}

// -------------------------------------------------------------------------------------------------

ipx_instance_input::ipx_instance_input(const std::string &name, const struct ipx_ctx_callbacks *cbs,
    uint32_t bsize) : ipx_instance(name)
{
    assert(cbs != nullptr);

    /* Connection schema:
     *                    (optional feedback)
     *                +------------------------+
     *                |                        |
     *                |     +-------+      +---+----+
     *                |     |       |      |        |
     *          +-----v-----> Input +------> Parser +----->
     *             feedback |       | ring |        | (output not set yet)
     *                      +-------+      +--------+
     */

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

    // TODO: first parser, send input, ... in case of error ... cancel parser!!!
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
    assert(_state == state::NEW); // Only configuration of an uninitialized instance can be changed!
    ipx_ctx_ring_dst_set(_parser_ctx, intermediate.get_input());
}

// -------------------------------------------------------------------------------------------------

ipx_instance_intermediate::ipx_instance_intermediate(const std::string &name,
    const struct ipx_ctx_callbacks *cbs, uint32_t bsize) : ipx_instance(name)
{
    assert(cbs != nullptr);

    /* Connection schema:
     *                  +-------+
     *                  |       |
     *            +-----> Inter +----->
     *             ring |       | (output not set yet)
     *                  +-------+
     */
    unique_ring ring_wrap(ipx_ring_init(bsize, false), &ipx_ring_destroy);
    unique_ctx  inter_wrap(ipx_ctx_create(name.c_str(), cbs), &ipx_ctx_destroy);
    if (!ring_wrap || !inter_wrap) {
        throw std::runtime_error("Failed to create components of an intermediate instance!");
    }

    // Configure the components (connect them)
    ipx_ctx_ring_src_set(inter_wrap.get(), ring_wrap.get());
    _instance_buffer = ring_wrap.release();
    _ctx = inter_wrap.release();
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
ipx_instance_intermediate::multiwrite_input(bool enable)
{
    assert(_state == state::NEW); // Only configuration of an uninitialized instance can be changed!
    ipx_ring_mw_mode(_instance_buffer, enable);
}

void
ipx_instance_intermediate::connect_to(ipx_instance_intermediate &intermediate)
{
    assert(_state == state::NEW); // Only configuration of an uninitialized instance can be changed!
    ipx_ctx_ring_dst_set(_ctx, intermediate.get_input());
}

// -------------------------------------------------------------------------------------------------

ipx_instance_outmgr::ipx_instance_outmgr(uint32_t bsize)
    : ipx_instance_intermediate("Output manager", &output_mgr_callbacks, bsize)
{
    /* Connection schema:
     *                     +--------+
     *                     |        +-->
     *              +------> Output |    list
     *               ring  |  mgr.  +-->
     *                     +--------+
     */

    _list = ipx_output_mgr_list_create();
    if (!_list) {
        throw std::runtime_error("Failed to initialize a list of output destinations!");
    }
}

ipx_instance_outmgr::~ipx_instance_outmgr()
{
    // The plugins must be terminated first
    ipx_ctx_destroy(_ctx);
    _ctx = nullptr;

    // Now we can destroy its private data
    ipx_output_mgr_list_destroy(_list);
}

void ipx_instance_outmgr::init(const std::string &params, const fds_iemgr_t *iemgr,
    ipx_verb_level level)
{
    assert(_state == state::NEW); // Only not initialized instance can be initialized
    if (ipx_output_mgr_list_empty(_list)) {
        throw std::runtime_error("Output manager is not connected to any output instances!");
    }

    // Pass the list of destinations
    ipx_ctx_private_set(_ctx, _list);
    ipx_instance_intermediate::init(params, iemgr, level);
    _state = state::INITIALIZED;
}

void
ipx_instance_outmgr::connect_to(ipx_instance_intermediate &intermediate)
{
    (void) intermediate;
    throw std::runtime_error("Output manager cannot pass data to another intermediate instance!");
}

void
ipx_instance_outmgr::connect_to(ipx_instance_output &output)
{
    assert(_state == state::NEW); // Only configuration of an uninitialized instance can be changed!
    auto connection = output.get_input();

    ipx_ring_t *ring = std::get<0>(connection);
    enum ipx_odid_filter_type filter_type = std::get<1>(connection);
    const ipx_orange_t *filter = std::get<2>(connection);

    if (ipx_output_mgr_list_add(_list, ring, filter_type, filter) != IPX_OK) {
        throw std::runtime_error("Failed to connect an output instance to the output manager!");
    }
}

// -------------------------------------------------------------------------------------------------

ipx_instance_output::ipx_instance_output(const std::string &name,
    const struct ipx_ctx_callbacks *cbs, uint32_t bsize) : ipx_instance(name)
{
    assert(cbs != nullptr);

    /* Connection schema:
     *                      +--------+
     *                      |        |
     *               +------> Output |
     *                 ring |        |
     *                      +--------+
     */
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

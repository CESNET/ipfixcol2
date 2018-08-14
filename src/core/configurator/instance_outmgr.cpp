/**
 * \file src/core/configurator/instance_outmgr.cpp
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Output manager instance wrapper (source file)
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

#include "instance_outmgr.hpp"

extern "C" {
#include "../plugin_output_mgr.h"
}

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


ipx_instance_outmgr::ipx_instance_outmgr(uint32_t bsize)
    : ipx_instance_intermediate("Output manager", &output_mgr_callbacks, bsize)
{
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

void ipx_instance_outmgr::init(const fds_iemgr_t *iemgr,
    ipx_verb_level level)
{
    assert(_state == state::NEW); // Only not initialized instance can be initialized
    if (ipx_output_mgr_list_empty(_list)) {
        throw std::runtime_error("Output manager is not connected to any output instances!");
    }

    // Pass the list of destinations
    ipx_ctx_private_set(_ctx, _list);
    ipx_instance_intermediate::init("", iemgr, level);
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
/**
 * \file src/core/configurator/configurator.cpp
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Main pipeline configurator (source file)
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
#include <iostream>
#include <cstdlib>
#include <dlfcn.h>
#include <signal.h>

#include "configurator.hpp"

extern "C" {
#include "../message_terminate.h"
#include "../plugin_parser.h"
#include "../plugin_output_mgr.h"
#include "../verbose.h"
}

/** Component identification (for log) */
static const char *comp_str = "Configurator";

ipx_configurator::ipx_configurator()
{
    iemgr = nullptr;
    ring_size = RING_DEF_SIZE;
}

ipx_configurator::~ipx_configurator()
{
    // First, stop all instances
    stop();

    if (iemgr != nullptr) {
        fds_iemgr_destroy(iemgr);
    }
}

/**
 * \brief Check if the model is valid for application
 *
 * The model must include at least one instance of an input plugin and one instance of an output
 * plugin
 * \param[in] model Model to check
 */
void
ipx_configurator::model_check(const ipx_config_model &model)
{
    if (model.inputs.empty()) {
        throw std::runtime_error("At least one input plugin must be defined!");
    }

    if (model.outputs.empty()) {
        throw std::runtime_error("At least one output plugin must be defined!");
    }
}

/**
 * \brief Create a new manager of Information Elements and load definitions
 * \param[in] dir Directory
 * \return Pointer to the new manager
 * \throw runtime_error if the function fails to load definitions of Information Elements
 */
fds_iemgr_t *
ipx_configurator::iemgr_load(const std::string dir)
{
    // Get a manager of Information Elements
    if (dir.empty()) {
        throw std::runtime_error("A directory of Information Elements definitions is not defined!");
    }

    std::unique_ptr<fds_iemgr_t, decltype(&fds_iemgr_destroy)> new_iemgr(fds_iemgr_create(),
        &fds_iemgr_destroy);
    if (!new_iemgr) {
        throw std::runtime_error("Failed to create a manager of Information Elements!");
    }

    // Load the IE manager
    if (fds_iemgr_read_dir(new_iemgr.get(), dir.c_str()) != FDS_OK) {
        throw std::runtime_error("Failed to load Information Elements from '" + dir + "': "
            + fds_iemgr_last_err(new_iemgr.get()));
    }

    return new_iemgr.release();
}

void
ipx_configurator::iemgr_set_dir(const std::string path)
{
    iemgr_dir = path;
}

void
ipx_configurator::set_buffer_size(uint32_t size)
{
    if (size < RING_MIN_SIZE) {
        throw std::invalid_argument("Size of ring buffers must be at least "
            + std::to_string(RING_MIN_SIZE) + " records.");
    }

    ring_size = size;
}

void
ipx_configurator::start(const ipx_config_model &model)
{
    // Check the model and prepare Information Elements (may throw an exception)
    model_check(model);
    iemgr = iemgr_load(iemgr_dir);

    IPX_INFO(comp_str, "Information Elements have been successfully loaded from '%s'.",
        iemgr_dir.c_str());

    // In case of an exception, smart pointers make sure that all instances are destroyed
    std::vector<std::unique_ptr<ipx_instance_output> > outputs;
    std::vector<std::unique_ptr<ipx_instance_intermediate> > inters;
    std::vector<std::unique_ptr<ipx_instance_input> > inputs;

    // Phase 1. Create all instances (i.e. find plugins)
    for (const auto &output : model.outputs) {
        const ipx_plugin_data *data = finder.find(output.plugin, IPX_PT_OUTPUT);
        outputs.emplace_back(new ipx_instance_output(output.name, &data->cbs, ring_size));
    }

    for (const auto &inter : model.inters) {
        const ipx_plugin_data *data = finder.find(inter.plugin, IPX_PT_INTERMEDIATE);
        inters.emplace_back(new ipx_instance_intermediate(inter.name, &data->cbs, ring_size));
    }

    for (const auto &input : model.inputs) {
        const ipx_plugin_data *data = finder.find(input.plugin, IPX_PT_INPUT);
        inputs.emplace_back(new ipx_instance_input(input.name, &data->cbs, ring_size));
    }

    // Insert the output manager as the last intermediate plugin
    ipx_instance_outmgr *output_manager = new ipx_instance_outmgr(ring_size);
    inters.emplace_back(output_manager);

    IPX_DEBUG(comp_str, "All plugins have been successfully loaded.");

    // Phase 2. Connect instances (input -> inter -> ... -> inter -> output manager -> output)
    ipx_instance_intermediate *first_inter = inters.front().get(); // TODO: set term. ref_num
    for (auto &input : inputs) {
        input->connect_to(*first_inter);
    }

    for (size_t i = 0; i < inters.size() - 1; ++i) { // Skip the last element
        ipx_instance_intermediate *now = inters[i].get();
        ipx_instance_intermediate *next = inters[i + 1].get();
        now->connect_to(*next);
    }

    for (auto &output : outputs) {
        output_manager->connect_to(*output);
    }

    // Phase 3. Initialize all instances (call constructors)
    for (size_t i = 0; i < model.outputs.size(); ++i) {
        ipx_instance_output *instance = outputs[i].get();
        const ipx_plugin_output &cfg = model.outputs[i];
        if (cfg.odid_type != IPX_ODID_FILTER_NONE) {
            // Define the ODID filter
            instance->set_filter(cfg.odid_type, cfg.odid_expression);
        }

        instance->init(cfg.params, iemgr, ipx_verb_level_get()); // TODO: get level
    }

    output_manager->init("", iemgr, ipx_verb_level_get());
    for (size_t i = 0; i < model.inters.size(); ++i) {
        ipx_instance_intermediate *instance = inters[i].get();
        const ipx_plugin_inter &cfg = model.inters[i];
        instance->init(cfg.params, iemgr, ipx_verb_level_get()); // TODO: get level
    }

    for (size_t i = 0; i < model.inputs.size(); ++i) {
        ipx_instance_input *instance = inputs[i].get();
        const ipx_plugin_input &cfg = model.inputs[i];
        instance->init(cfg.params, iemgr, ipx_verb_level_get()); // TODO: get level
    }

    IPX_DEBUG(comp_str, "All instances have been successfully initialized.");

    // Start threads of all plugins
    for (auto &output : outputs) {
        output->start();
    }

    for (auto &inter : inters) {
        inter->start();
    }

    for (auto &input : inputs) {
        input->start();
    }

    IPX_DEBUG(comp_str, "All threads of instances has been successfully started.");
    running_inputs = std::move(inputs);
    running_inter = std::move(inters);
    running_outputs = std::move(outputs);
}

void ipx_configurator::stop()
{
    if (running_inputs.empty()) {
        // No instances -> nothing to do
        return;
    }

    // Send a request to terminate to all input plugins
    for (auto &input : running_inputs) {
        ipx_msg_terminate_t *msg = ipx_msg_terminate_create(IPX_MSG_TERMINATE_INSTANCE);
        if (!msg) {
            IPX_ERROR(comp_str, "Failed to create a termination message. The plugins cannot be "
                "properly terminated! (%s:%d)", __FILE__, __LINE__);
            continue;
        }
        ipx_fpipe_write(input->get_feedback(), ipx_msg_terminate2base(msg));
    }

    IPX_DEBUG(comp_str, "Requests to terminate the pipeline sent! Waiting for instances to "
        "terminate.");

    // Wait for termination (destructor of smart pointers will call instance destructor)
    running_inputs.clear();
    running_inter.clear();
    running_outputs.clear();

    IPX_DEBUG(comp_str, "All instances successfully terminated.");
}

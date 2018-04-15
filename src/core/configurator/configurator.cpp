//
// Created by lukashutak on 04/04/18.
//

#include <memory>
#include <iostream>
#include <cstdlib>
#include <dlfcn.h>
#include <signal.h>

#include "configurator.hpp"
#include "instance.hpp"

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
    running_model = nullptr;
    iemgr = nullptr;
    ring_size = RING_DEF_SIZE;
}

ipx_configurator::~ipx_configurator()
{
    if (running_model != nullptr) {
        delete(running_model);
    }

    if (iemgr != nullptr) {
        fds_iemgr_destroy(iemgr);
    }
}

void
ipx_configurator::set_iemgr_dir(const std::string path)
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
ipx_configurator::apply(const ipx_config_model &model)
{
    if (running_model != nullptr) {
        throw std::runtime_error("Sorry, runtime reconfiguration is not implemented!");
    }

    if (model.inputs.empty()) {
        throw std::runtime_error("At least one input plugin must be defined!");
    }

    if (model.outputs.empty()) {
        throw std::runtime_error("At least one output plugin must be defined!");
    }

    // Get a manager of Information Elements
    if (iemgr_dir.empty()) {
        throw std::runtime_error("A directory of Information Elements definitions is not defined!");
    }

    std::unique_ptr<fds_iemgr_t, decltype(&fds_iemgr_destroy)> new_iemgr(fds_iemgr_create(),
        &fds_iemgr_destroy);
    if (!new_iemgr) {
        throw std::runtime_error("Failed to create a manager of Information Elements!");
    }

    // Load the IE manager
    if (fds_iemgr_read_dir(new_iemgr.get(), iemgr_dir.c_str()) != FDS_OK) {
        throw std::runtime_error("Failed to load Information Elements from '" + iemgr_dir + "': "
            + fds_iemgr_last_err(new_iemgr.get()));
    }

    IPX_INFO(comp_str, "Information Elements have been successfully loaded from '%s'.",
        iemgr_dir.c_str());


    // Create an output manager
    std::vector<std::unique_ptr<ipx_instance_output> > output_instances;
    std::vector<std::unique_ptr<ipx_instance_intermediate> > inter_instances;
    std::vector<std::unique_ptr<ipx_instance_input> > input_instances;

    // Phase 1. Create all instances (i.e. find plugins)
    for (const auto &output : model.outputs) {
        const ipx_plugin_data *data = finder.find(output.plugin, IPX_PT_OUTPUT);
        output_instances.emplace_back(new ipx_instance_output(output.name, &data->cbs, ring_size));
    }

    for (const auto &inter : model.inters) {
        const ipx_plugin_data *data = finder.find(inter.plugin, IPX_PT_INTERMEDIATE);
        inter_instances.emplace_back(new ipx_instance_intermediate(inter.name, &data->cbs, ring_size));
    }

    for (const auto &input : model.inputs) {
        const ipx_plugin_data *data = finder.find(input.plugin, IPX_PT_INPUT);
        input_instances.emplace_back(new ipx_instance_input(input.name, &data->cbs, ring_size));
    }

    // Insert the output manager as the last intermediate plugin
    ipx_instance_outmgr *output_manager = new ipx_instance_outmgr(ring_size);
    inter_instances.emplace_back(output_manager);

    // Phase 2. Connect instances (input -> inter -> ... -> inter -> output manager -> output)
    ipx_instance_intermediate *first_inter = inter_instances.front().get(); // TODO: set term. ref_num
    for (auto &input : input_instances) {
        input->connect_to(*first_inter);
    }

    for (size_t i = 0; i < inter_instances.size() - 1; ++i) { // Skip the last element
        ipx_instance_intermediate *now = inter_instances[i].get();
        ipx_instance_intermediate *next = inter_instances[i + 1].get();
        now->connect_to(*next);
    }

    for (auto &output : output_instances) {
        output_manager->connect_to(*output);
    }

    // Phase 3. Initialize all instances (call constructors)
    for (size_t i = 0; i < model.outputs.size(); ++i) {
        ipx_instance_output *instance = output_instances[i].get();
        const ipx_plugin_output &cfg = model.outputs[i];
        if (cfg.odid_type != IPX_ODID_FILTER_NONE) {
            // Define the ODID filter
            instance->set_filter(cfg.odid_type, cfg.odid_expression);
        }

        instance->init(cfg.params, new_iemgr.get(), ipx_verb_level_get()); // TODO: get level
    }

    output_manager->init("", new_iemgr.get(), ipx_verb_level_get());
    for (size_t i = 0; i < model.inters.size(); ++i) {
        ipx_instance_intermediate *instance = inter_instances[i].get();
        const ipx_plugin_inter &cfg = model.inters[i];
        instance->init(cfg.params, new_iemgr.get(), ipx_verb_level_get()); // TODO: get level
    }

    for (size_t i = 0; i < model.inputs.size(); ++i) {
        ipx_instance_input *instance = input_instances[i].get();
        const ipx_plugin_input &cfg = model.inputs[i];
        instance->init(cfg.params, new_iemgr.get(), ipx_verb_level_get()); // TODO: get level
    }

    // Start threads of all plugins
    for (auto &output : output_instances) {
        output->start();
    }

    for (auto &inter : inter_instances) {
        inter->start();
    }

    for (auto &input : input_instances) {
        input->start();
    }

    // Dummy wait for an interrupt...
    sigset_t set;
    sigfillset(&set);

    int sig_num;
    sigwait(&set, &sig_num);


    //Send a terminate message and wait // TODO: fixme .. send to all inputs
    ipx_msg_terminate_t *term_msg = ipx_msg_terminate_create(IPX_MSG_TERMINATE_INSTANCE);
    ipx_fpipe_write(input_instances.front()->get_feedback(), ipx_msg_terminate2base(term_msg));
    std::cout << "Signal sent!" << std::endl;
}

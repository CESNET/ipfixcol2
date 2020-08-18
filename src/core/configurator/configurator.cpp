/**
 * \file src/core/configurator/configurator.cpp
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Main pipeline configurator (source file)
 * \date 2018-2020
 */

/* Copyright (C) 2018-2020 CESNET, z.s.p.o.
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

#include <unistd.h> // STDOUT_FILENO
#include <memory>
#include <iostream>
#include <string>
#include <cstdlib>
#include <dlfcn.h>
#include <signal.h>

#include "configurator.hpp"
#include "extensions.hpp"

extern "C" {
#include "../message_terminate.h"
#include "../plugin_parser.h"
#include "../plugin_output_mgr.h"
#include "../verbose.h"
#include "../context.h"
#include "cpipe.h"
}

/** Component identification (for log) */
static const char *comp_str = "Configurator";

/**
 * @brief Terminating signal handler
 * @param[in] sig Signal
 */
static void
termination_handler(int sig)
{
    (void) sig;

    // In case we change 'errno' (e.g. write())
    int errno_backup = errno;
    static int cnt = 0;

    if (cnt > 0) {
        static const char *msg = "Another termination signal detected. Quiting without cleanup...\n";
        write(STDOUT_FILENO, msg, strlen(msg));
        abort();
    }
    cnt++;

    // Send a termination request to the configurator
    int rc = ipx_cpipe_send_term(NULL, IPX_CPIPE_TYPE_TERM_FAST);
    if (rc != IPX_OK) {
        static const char *msg = "ERROR: Signal handler: failed to send a termination request";
        write(STDOUT_FILENO, msg, strlen(msg));
    }

    errno = errno_backup;
}

ipx_configurator::ipx_configurator()
{
    m_iemgr = nullptr;
    m_ring_size = RING_DEF_SIZE;

    // Create a configuration pipe
    if (ipx_cpipe_init() != IPX_OK) {
        throw std::runtime_error("Failed to initialize configurator (ipx_cpipe_init() failed)");
    }

    // Register default signal handlers
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGINT);
    sigaddset(&sa.sa_mask, SIGTERM);
    sa.sa_flags |= SA_RESTART;
    sa.sa_handler = termination_handler;
    if (sigaction(SIGTERM, &sa, NULL) == -1 || sigaction(SIGINT, &sa, NULL) == -1) {
        throw std::runtime_error("Failed to register termination signal handlers!");
    }
}

ipx_configurator::~ipx_configurator()
{
    // Make sure that all threads are terminated
    cleanup();

    // Disable the signal handler
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGINT);
    sigaddset(&sa.sa_mask, SIGTERM);
    sa.sa_handler = SIG_DFL;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    // Destroy the configuration pipe
    ipx_cpipe_destroy();

    if (m_iemgr != nullptr) {
        fds_iemgr_destroy(m_iemgr);
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

/**
 * \brief Convert a string to corresponding verbosity level
 * \param[in] verb String
 * \return Verbosity level
 */
enum ipx_verb_level
ipx_configurator::verbosity_str2level(const std::string &verb)
{
    if (verb.empty() || strcasecmp(verb.c_str(), "default") == 0) {
        // Default
        return ipx_verb_level_get();
    }

    if (strcasecmp(verb.c_str(), "none") == 0) {
        return IPX_VERB_NONE;
    } else if (strcasecmp(verb.c_str(), "error") == 0) {
        return IPX_VERB_ERROR;
    } else if (strcasecmp(verb.c_str(), "warning") == 0) {
        return IPX_VERB_WARNING;
    } else if (strcasecmp(verb.c_str(), "info") == 0) {
        return IPX_VERB_INFO;
    } else if (strcasecmp(verb.c_str(), "debug") == 0) {
        return IPX_VERB_DEBUG;
    } else {
        throw std::invalid_argument("Invalid verbosity level!");
    }
}

void
ipx_configurator::iemgr_set_dir(const std::string &path)
{
    m_iemgr_dir = path;
}

void
ipx_configurator::set_buffer_size(uint32_t size)
{
    if (size < RING_MIN_SIZE) {
        throw std::invalid_argument("Size of ring buffers must be at least "
            + std::to_string(RING_MIN_SIZE) + " records.");
    }

    m_ring_size = size;
}

void
ipx_configurator::startup(const ipx_config_model &model)
{
    // Check the model and prepare Information Elements (may throw an exception)
    model_check(model);
    m_iemgr = iemgr_load(m_iemgr_dir);

    IPX_INFO(comp_str, "Information Elements have been successfully loaded from '%s'.",
        m_iemgr_dir.c_str());

    // In case of an exception, smart pointers make sure that all instances are destroyed
    std::vector<std::unique_ptr<ipx_instance_output> > outputs;
    std::vector<std::unique_ptr<ipx_instance_intermediate> > inters;
    std::vector<std::unique_ptr<ipx_instance_input> > inputs;

    // Phase 1. Create all instances (i.e. find plugins)
    for (const auto &output : model.outputs) {
        ipx_plugin_mgr::plugin_ref *ref = plugins.plugin_get(IPX_PT_OUTPUT, output.plugin);
        outputs.emplace_back(new ipx_instance_output(output.name, ref, m_ring_size));
    }

    for (const auto &inter : model.inters) {
        ipx_plugin_mgr::plugin_ref *ref = plugins.plugin_get(IPX_PT_INTERMEDIATE, inter.plugin);
        inters.emplace_back(new ipx_instance_intermediate(inter.name, ref, m_ring_size));
    }

    for (const auto &input : model.inputs) {
        ipx_plugin_mgr::plugin_ref *ref = plugins.plugin_get(IPX_PT_INPUT, input.plugin);
        inputs.emplace_back(new ipx_instance_input(input.name, ref, m_ring_size));
    }

    // Insert the output manager as the last intermediate plugin
    ipx_instance_outmgr *output_manager = new ipx_instance_outmgr(m_ring_size);
    inters.emplace_back(output_manager);

    IPX_DEBUG(comp_str, "All plugins have been successfully loaded.", '\0');

    // Phase 2. Connect instances (input -> inter -> ... -> inter -> output manager -> output)
    ipx_instance_intermediate *first_inter = inters.front().get();
    for (auto &input : inputs) {
        input->connect_to(*first_inter); // This can enable multi-writer mode
    }

    for (size_t i = 0; i < inters.size() - 1; ++i) { // Skip the last element
        ipx_instance_intermediate *from = inters[i].get();
        ipx_instance_intermediate *to = inters[i + 1].get();
        from->connect_to(*to);
    }

    for (size_t i = 0; i < model.outputs.size(); ++i) {
        // First initialize ODID filter, if necessary
        ipx_instance_output *instance = outputs[i].get();
        const ipx_plugin_output &cfg = model.outputs[i];
        if (cfg.odid_type != IPX_ODID_FILTER_NONE) {
            instance->set_filter(cfg.odid_type, cfg.odid_expression);
        }

        // Connect the output manager and the output instance
        output_manager->connect_to(*instance);
    }

    // Phase 3. Initialize all instances (call constructors)
    for (size_t i = 0; i < model.outputs.size(); ++i) {
        ipx_instance_output *instance = outputs[i].get();
        const ipx_plugin_output &cfg = model.outputs[i];
        instance->init(cfg.params, m_iemgr, verbosity_str2level(cfg.verbosity));
    }

    output_manager->init(m_iemgr, ipx_verb_level_get());
    for (size_t i = 0; i < model.inters.size(); ++i) {
        ipx_instance_intermediate *instance = inters[i].get();
        const ipx_plugin_inter &cfg = model.inters[i];
        instance->init(cfg.params, m_iemgr, verbosity_str2level(cfg.verbosity));
    }

    for (size_t i = 0; i < model.inputs.size(); ++i) {
        ipx_instance_input *instance = inputs[i].get();
        const ipx_plugin_input &cfg = model.inputs[i];
        instance->init(cfg.params, m_iemgr, verbosity_str2level(cfg.verbosity));
    }

    IPX_DEBUG(comp_str, "All instances have been successfully initialized.", '\0');

    // Phase 4. Register and resolved Data Record extensions and dependencies
    ipx_cfg_extensions ext_mgr;
    size_t pos = 0; // Position of an instance in the collector pipeline

    for (auto &input : inputs) {
        input->extensions_register(&ext_mgr, pos);
    }

    pos++;
    for (auto &inter : inters) {
        inter->extensions_register(&ext_mgr, pos);
        pos++;
    }

    for (auto &output : outputs) {
        output->extensions_register(&ext_mgr, pos);
    }

    ext_mgr.resolve();
    ext_mgr.list_extensions();

    // Phase 5. Start threads of all plugins and update definitions of extensions
    for (auto &output : outputs) {
        output->extensions_resolve(&ext_mgr);
        output->start();
    }

    for (auto &inter : inters) {
        inter->extensions_resolve(&ext_mgr);
        inter->start();
    }

    for (auto &input : inputs) {
        input->extensions_resolve(&ext_mgr);
        input->start();
    }

    IPX_DEBUG(comp_str, "All threads of instances has been successfully started.", '\0');
    m_running_inputs = std::move(inputs);
    m_running_inter = std::move(inters);
    m_running_outputs = std::move(outputs);
}

void ipx_configurator::cleanup()
{
    // Wait for termination (destructor of smart pointers will call instance destructor)
    m_running_inputs.clear();
    m_running_inter.clear();
    m_running_outputs.clear();

    IPX_DEBUG(comp_str, "Cleanup complete!", '\0');
}

bool
ipx_configurator::termination_handle(const struct ipx_cpipe_req &req, ipx_controller *ctrl)
{
    // First of all, check if termination process has been completed
    if (req.type == IPX_CPIPE_TYPE_TERM_DONE) {
        if (m_state != STATUS::STOP_SLOW && m_state != STATUS::STOP_FAST) {
            IPX_ERROR(comp_str, "[internal] Got a termination done notification, but the "
                "termination process is not in progress!", '\0');
            return false;
        }

        if (m_term_sent == 0) {
            IPX_ERROR(comp_str, "[internal] Unexpected termination message", '\0');
            abort();
        }

        if (--m_term_sent != 0) {
            // There is still at least one termination message in the pipeline
            return false;
        }

        return true;
    }

    // Format a status message and notify the controller
    assert(req.type == IPX_CPIPE_TYPE_TERM_SLOW || req.type == IPX_CPIPE_TYPE_TERM_FAST);
    std::string msg = (req.type == IPX_CPIPE_TYPE_TERM_SLOW) ? "Slow" : "Fast";
    msg += " termination request has been received";
    if (req.ctx != nullptr) {
        const struct ipx_plugin_info *info = ipx_ctx_plugininfo_get(req.ctx);
        const char *plugin_type;
        switch (info->type) {
        case IPX_PT_INPUT:
            plugin_type = "input plugin";
            break;
        case IPX_PT_INTERMEDIATE:
            plugin_type = "intermediate plugin";
            break;
        case IPX_PT_OUTPUT:
            plugin_type = "output plugin";
            break;
        default:
            plugin_type = "<unknown plugin type>";
            break;
        }

        msg += " from " + std::string(plugin_type) + " '" + std::string(info->name) + "' by "
            + "instance '"+ ipx_ctx_name_get(req.ctx) + "'";
    } else {
        msg += " from an external source";
    }
    ctrl->terminate_on_request(req, msg);

    // Next state
    STATUS next_state = (req.type == IPX_CPIPE_TYPE_TERM_FAST)
        ? STATUS::STOP_FAST : STATUS::STOP_SLOW;

    /* Send a termination message (if it hasn't been send yet) and stop processing Data and Session
     * request by selected instances (if any) */
    switch (m_state) {
    case STATUS::RUNNING:
        /* The first request to stop the collector...
         * A termination message must be delivered to all input plugins and some plugins must
         * immediately stop processing all IPFIX messages.
         */
        termination_send_msg();
        // fall through

    case STATUS::STOP_SLOW:
        /* Another request to stop the collector has been received...
         * There might be more plugins that should immediately stop processing all IPFIX messages.
         */
        if (next_state == STATUS::STOP_FAST) {
            termination_stop_all();
        } else {
            termination_stop_partly(req.ctx);
        }

        m_state = next_state;
        break;

    case STATUS::STOP_FAST:
        /* Another request to stop the collector has been received...
         * However, processing of IPFIX Messages by all plugins have been already stopped.
         */
        break;
    default:
        assert(false && "Unimplemented behavior for the current state!");
    }

    return false;
}

/**
 * \brief Disable data processing by all plugins
 *
 * Calling the getter callbacks (input plugin instances) and processing callbacks (intermediate
 * and output plugin instances) will be disabled and all plugin contexts will immediately
 * drop IPFIX and Transport Session messages on arrival. Only garbage and configuration
 * pipeline messages are still processed.
 */
void
ipx_configurator::termination_stop_all()
{
    for (auto &it : m_running_inputs) {
        it->set_processing(false);
        it->set_parser_processing(false);
    };
    for (auto &it : m_running_inter) {
        it->set_processing(false);
    }
    for (auto &it : m_running_outputs) {
        it->set_processing(false);
    }
}

/**
 * \brief Disabled data processing by all plugins before the given context (including)
 *
 * Calling the getter callbacks (input plugin instances) and processing callbacks (intermediate
 * and output plugin instances) of the plugins before the given context (including) will be
 * disabled. Only garbage and configuration pipeline messages are still processed by these
 * particular instances.
 *
 * The plugin instances after the given context are untouched.
 * \note If the \p ctx is nullptr, no plugins are immediately terminated!
 * \param[in] ctx Context of the plugin which invoked the termination sequence
 */
void
ipx_configurator::termination_stop_partly(const ipx_ctx_t *ctx)
{
    if (!ctx) {
        // Nothing to do...
        return;
    }

    // Stop all input plugins
    const struct ipx_plugin_info *ctx_info = ipx_ctx_plugininfo_get(ctx);
    for (auto &it : m_running_inputs) {
        it->set_processing(false);
    }

    if (ctx_info->type == IPX_PT_INPUT) {
        return;
    }

    // Stop all NetFlow/IPFIX message parsers
    for (auto &it : m_running_inputs) {
        it->set_parser_processing(false);
    }

    if (ctx_info == &ipx_plugin_parser_info) {
        return;
    }

    // Stop intermediate plugins
    for (auto &it : m_running_inter) {
        it->set_processing(false);
        if (it->get_ctx() == ctx) {
            return;
        }
    }

    // Stop output plugins
    assert(ctx_info->type == IPX_PT_OUTPUT && "Output context expected!");
    for (auto &it : m_running_outputs) {
        it->set_processing(false);
    }
}

/**
 * \brief Send a termination message to all input plugins
 *
 * The termination message will call instance destructor and stop the instance thread.
 * This function must be called only once!
 */
void
ipx_configurator::termination_send_msg()
{
    assert(m_term_sent == 0 && "The termination message counter must be zero!");

    for (auto &input : m_running_inputs) {
        ipx_msg_terminate_t *msg = ipx_msg_terminate_create(IPX_MSG_TERMINATE_INSTANCE);
        if (!msg) {
            IPX_ERROR(comp_str, "Failed to create a termination message. The plugins cannot be "
                "properly terminated! (%s:%d)", __FILE__, __LINE__);
            abort();
        }
        ipx_fpipe_write(input->get_feedback(), ipx_msg_terminate2base(msg));
    }

    IPX_DEBUG(comp_str, "Request to terminate the pipeline sent! Waiting for instances to "
        "terminate.", '\0');
    m_term_sent = m_running_inputs.size();
}

int
ipx_configurator::run(ipx_controller *ctrl)
{
    // Status variables
    std::string msg = "Success";
    ipx_controller::OP_STATUS status = ipx_controller::OP_STATUS::SUCCESS;

    // Try to start the collector (get the configuration model, initialize and start plugins)
    ctrl->start_before();
    try {
        ipx_config_model model = ctrl->model_get();
        startup(model);
    } catch (const std::exception &ex) {
        msg = ex.what();
        status = ipx_controller::OP_STATUS::FAILED;
    }

    ctrl->start_after(status, msg);
    if (status == ipx_controller::OP_STATUS::FAILED) {
        // Something went wrong -> bye
        cleanup();
        return EXIT_FAILURE;
    }

    // Collector is running -> process termination/reconfiguration requests
    m_state = STATUS::RUNNING;
    bool terminate = false;
    while (!terminate) {
        struct ipx_cpipe_req req;
        if (ipx_cpipe_receive(&req) != IPX_OK) {
            // This is really bad -> we cannot even safely terminate the collector
            IPX_ERROR(comp_str, "Configuration pipe is broken. Terminating...", '\0');
            abort();
        }

        switch (req.type) {
        case IPX_CPIPE_TYPE_TERM_SLOW:
        case IPX_CPIPE_TYPE_TERM_FAST:
        case IPX_CPIPE_TYPE_TERM_DONE:
            terminate = termination_handle(req, ctrl);
            break;
        default:
            IPX_ERROR(comp_str, "Ignoring unknown configuration request!", '\0');
            continue;
        }
    };

    // The collector has been terminated
    ctrl->terminate_after();
    return EXIT_SUCCESS;
}
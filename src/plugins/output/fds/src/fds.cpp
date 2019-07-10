/**
 * \file src/plugins/output/dummy/dummy.c
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Example output plugin for IPFIXcol 2
 * \date 2018
 *
 * Copyright(c) 2019 CESNET z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <inttypes.h>
#include <ipfixcol2.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "Config.hpp"
#include "Storage.hpp"

/// Plugin description
IPX_API struct ipx_plugin_info ipx_plugin_info = {
    // Plugin identification name
    "fds",
    // Brief description of plugin
    "Flow Data Storage output plugin",
    // Plugin type
    IPX_PT_OUTPUT,
    // Configuration flags (reserved for future use)
    0,
    // Plugin version string (like "1.2.3")
    "2.0.0",
    // Minimal IPFIXcol version string (like "1.2.3")
    "2.1.0"
};

/// Instance
struct Instance {
    /// Parsed configuration
    std::unique_ptr<Config> config_ptr = nullptr;
    /// Storage file
    std::unique_ptr<Storage> storage_ptr = nullptr;
    /// Start of the current window
    time_t window_start = 0;
};

static void
window_check(struct Instance &inst)
{
    const Config &cfg = *inst.config_ptr;

    // Decide whether close file and create a new time window
    time_t now = time(NULL);
    if (difftime(now, inst.window_start) < cfg.m_window.size) {
        // Nothing to do
        return;
    }

    if (cfg.m_window.align) {
        const uint32_t window_size = cfg.m_window.size;
        now /= window_size;
        now *= window_size;
    }

    inst.window_start = now;
    inst.storage_ptr->window_new(now);
}

int
ipx_plugin_init(ipx_ctx_t *ctx, const char *params)
{
    try {
        // Parse configuration, try to create a storage and time window
        std::unique_ptr<Instance> instance(new Instance);
        instance->config_ptr.reset(new Config(params));
        instance->storage_ptr.reset(new Storage(ctx, *instance->config_ptr));
        window_check(*instance);
        // Everything seems OK
        ipx_ctx_private_set(ctx, instance.release());
    } catch (const FDS_exception &ex) {
        IPX_CTX_ERROR(ctx, "Initialization failed: %s", ex.what());
        return IPX_ERR_DENIED;
    } catch (...) {
        IPX_CTX_ERROR(ctx, "Unknown error has occurred!", '\0');
        return IPX_ERR_DENIED;
    }

    return IPX_OK;
}

void
ipx_plugin_destroy(ipx_ctx_t *ctx, void *cfg)
{
    (void) ctx; // Suppress warnings

    try {
        auto inst = reinterpret_cast<Instance *>(cfg);
        inst->storage_ptr.reset();
        inst->config_ptr.reset();
        delete inst;
    } catch (...) {
        IPX_CTX_ERROR(ctx, "Something bad happened during plugin destruction");
    }
}

int
ipx_plugin_process(ipx_ctx_t *ctx, void *cfg, ipx_msg_t *msg)
{
    auto *inst = reinterpret_cast<Instance *>(cfg);
    bool failed = false;

    try {
        // Check if the current time window should be closed
        window_check(*inst);
        ipx_msg_ipfix_t *msg_ipfix = ipx_msg_base2ipfix(msg);
        inst->storage_ptr->process_msg(msg_ipfix);
    } catch (const FDS_exception &ex) {
        IPX_CTX_ERROR(ctx, "%s", ex.what());
        failed = true;
    } catch (std::exception &ex) {
        IPX_CTX_ERROR(ctx, "Unexpected error has occurred: %s", ex.what());
        failed = true;
    } catch (...) {
        IPX_CTX_ERROR(ctx, "Unknown error has occurred!");
        failed = true;
    }

    if (failed) {
        IPX_CTX_ERROR(ctx, "Due to the previous error(s), the output file is possibly corrupted. "
            "Therefore, no flow records are stored until a new file is automatically opened "
            "after current window expiration.");
        inst->storage_ptr->window_close();
    }

    return IPX_OK;
}

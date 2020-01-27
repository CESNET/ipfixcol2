/**
 * \file src/plugins/output/json/src/json.cpp
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief JSON output plugin (source file)
 * \date 2018-2019
 */

/* Copyright (C) 2018-2019 CESNET, z.s.p.o.
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

#include <libfds.h>
#include <ipfixcol2.h>
#include <memory>

#include "Config.hpp"
#include "Storage.hpp"
#include "Printer.hpp"
#include "File.hpp"
#include "Server.hpp"
#include "Sender.hpp"
#include "Kafka.hpp"

/** Plugin description */
IPX_API struct ipx_plugin_info ipx_plugin_info = {
    // Plugin identification name
    "json",
    // Brief description of plugin
    "Conversion of IPFIX data into JSON format",
    // Plugin type
    IPX_PT_OUTPUT,
    // Configuration flags (reserved for future use)
    0,
    // Plugin version string (like "1.2.3")
    "2.2.0",
    // Minimal IPFIXcol version string (like "1.2.3")
    "2.1.0"
};

/** JSON instance data                                                                           */
struct Instance {
    /** Parser configuration                                                                     */
    Config *config;
    /** Storage (output manager)                                                                 */
    Storage *storage;
};

/**
 * \brief Initialize outputs
 *
 * For each definition of an output in a plugin configuration, call its constructor and add it to
 * an output manager.
 * \param[in] ctx     Instance context
 * \param[in] storage Manager of output
 * \param[in] cfg     Parsed configuration of the instance
 */
static void
outputs_initialize(ipx_ctx_t *ctx, Storage *storage, Config *cfg)
{
    for (const auto &print : cfg->outputs.prints) {
        storage->output_add(new Printer(print, ctx));
    }

    for (const auto &file : cfg->outputs.files) {
        storage->output_add(new File(file, ctx));
    }

    for (const auto &server : cfg->outputs.servers) {
        storage->output_add(new Server(server, ctx));
    }

    for (const auto &send : cfg->outputs.sends) {
        storage->output_add(new Sender(send, ctx));
    }

    for (const auto &kafka : cfg->outputs.kafkas) {
        storage->output_add(new Kafka(kafka, ctx));
    }
}

int
ipx_plugin_init(ipx_ctx_t *ctx, const char *params)
{
    struct Instance *data = nullptr;
    try {
        // Create and parse the configuration
        std::unique_ptr<Instance> ptr(new Instance);
        std::unique_ptr<Config> cfg(new Config(params));
        std::unique_ptr<Storage> storage(new Storage(ctx, cfg.get()->format));

        // Initialize outputs
        outputs_initialize(ctx, storage.get(), cfg.get());

        // Success
        data = ptr.release();
        data->config = cfg.release();
        data->storage = storage.release();

    } catch (std::exception &ex) {
        IPX_CTX_ERROR(ctx, "%s", ex.what());
        return IPX_ERR_DENIED;
    } catch (...) {
        IPX_CTX_ERROR(ctx, "Unexpected exception has occurred!", '\0');
        return IPX_ERR_DENIED;
    }

    ipx_ctx_private_set(ctx, data);
    return IPX_OK;
}

void
ipx_plugin_destroy(ipx_ctx_t *ctx, void *cfg)
{
    (void) ctx;

    struct Instance *data = reinterpret_cast<struct Instance *>(cfg);
    delete data->storage;
    delete data->config;
    delete data;
}

int
ipx_plugin_process(ipx_ctx_t *ctx, void *cfg, ipx_msg_t *msg)
{
    (void) ctx;

    int ret_code = IPX_OK;
    const fds_iemgr_t *iemgr = ipx_ctx_iemgr_get(ctx);

    try {
        struct Instance *data = reinterpret_cast<struct Instance *>(cfg);
        ret_code = data->storage->records_store(ipx_msg_base2ipfix(msg), iemgr);
    } catch (std::exception &ex) {
        IPX_CTX_ERROR(ctx, "%s", ex.what());
        ret_code = FDS_ERR_DENIED;
    } catch (...) {
        IPX_CTX_ERROR(ctx, "Unexpected exception has occurred!", '\0');
        ret_code = FDS_ERR_DENIED;
    }

    return ret_code;
}

/**
 * \file src/plugins/input/fds/fds.cpp
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief IPFIX File input plugin for IPFIXcol
 * \date 2020
 */

/* Copyright (C) 2020 CESNET, z.s.p.o.
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

#include <cerrno>
#include <climits>
#include <cstdlib>
#include <glob.h>
#include <ipfixcol2.h>
#include <libfds.h>
#include <map>
#include <memory>
#include <netinet/in.h>
#include <string>

#include "config.h"
#include "Exception.hpp"
#include "Reader.hpp"

/// Plugin description
IPX_API struct ipx_plugin_info ipx_plugin_info = {
    // Plugin identification name
    "fds",
    // Brief description of plugin
    "Input plugin for FDS File format.",
    // Plugin type
    IPX_PT_INPUT,
    // Configuration flags (reserved for future use)
    0,
    // Plugin version string (like "1.2.3")
    "2.0.0",
    // Minimal IPFIXcol version string (like "1.2.3")
    "2.2.0"
};

/// Plugin instance
struct Instance {
    /// Plugin context
    ipx_ctx_t *m_ctx;
    /// Parsed plugin configuration
    std::unique_ptr<fds_config, decltype(&config_destroy)> m_cfg = {nullptr, &config_destroy};

    /// List of files to read
    glob_t m_list;
    /// Index of the next file to read
    size_t m_next_file = 0;

    // Current file reader
    std::unique_ptr<Reader> m_file = nullptr;
};

/**
 * @brief Check if path is a directory
 *
 * @note Since we use GLOB_MARK flag, all directories ends with a slash.
 * @param[in] filename Path
 * @return True or false
 */
static inline bool
file_is_dir(const char *filename)
{
    size_t len = strlen(filename);
    return (filename[len - 1] == '/');
}

/**
 * @brief Initialize a list of files to read
 *
 * @param[in] inst    Plugin instance
 * @param[in] pattern Pattern of files to read
 * @throw FDS_exception in case of a failure (or zero file matches)
 */
void
file_list_init(Instance *inst, const char *pattern)
{
#ifndef GLOB_TILDE_CHECK
#define GLOB_TILDE_CHECK GLOB_TILDE
#endif
    int glob_flags = GLOB_MARK | GLOB_BRACE | GLOB_TILDE_CHECK;
    size_t file_cnt;
    int ret;

    ret = glob(pattern, glob_flags, NULL, &inst->m_list);
    switch (ret) {
    case 0: // Success
        break;
    case GLOB_NOSPACE:
        throw FDS_exception("Failed to list files to process due memory allocation error!");
    case GLOB_ABORTED:
        throw FDS_exception("Failed to list files to process due read error");
    case GLOB_NOMATCH:
        throw FDS_exception("No file matches the given file pattern!");
    default:
        throw FDS_exception("glob() failed and returned unexpected value!");
    }

    file_cnt = 0;
    for (size_t i = 0; i < inst->m_list.gl_pathc; ++i) {
        const char *filename = inst->m_list.gl_pathv[i];
        if (file_is_dir(filename)) {
            continue;
        }
        file_cnt++;
    }

    if (!file_cnt) {
        globfree(&inst->m_list);
        throw FDS_exception("No FDS Files matches the given file pattern!");
    }

    inst->m_next_file = 0;
}

/**
 * @brief Destroy list of files to read
 * @param[in] inst Plugin instance
 */
void
file_list_clean(Instance *inst)
{
    globfree(&inst->m_list);
}

/**
 * Open the next file for reading
 *
 * If any file is already opened, it will be closed and several Session Messages
 * (close notification) will be send too. The function will try to open the next file
 * in the list and makes sure that it is valid FDS File. Otherwise, it will be skipped
 * and another file will be used.
 *
 * @warning
 *   As the function sends notification to other plugins further in the pipeline, it
 *   must have permission to pass messages. Therefore, this function cannot be called
 *   within ipx_plugin_init().
 * @param[in] inst Plugin instance
 * @return #IPX_OK on success
 * @return #IPX_ERR_EOF if no more files are available
 */
static int
file_next(Instance *inst)
{
    std::unique_ptr<Reader> reader_new = nullptr;
    const char *file_name = nullptr;
    size_t idx_next;
    size_t idx_max = inst->m_list.gl_pathc;

    // Close the previous file (if any)
    inst->m_file.reset();

    // Open new file
    for (idx_next = inst->m_next_file; idx_next < idx_max; ++idx_next) {
        file_name = inst->m_list.gl_pathv[idx_next];
        if (file_is_dir(file_name)) {
            continue;
        }

        try {
            reader_new.reset(new Reader(inst->m_ctx, inst->m_cfg.get(), file_name));
        } catch (const FDS_exception &ex) {
            IPX_CTX_ERROR(inst->m_ctx, "%s", ex.what());
            continue;
        }

        // Success
        break;
    }

    inst->m_next_file = idx_next + 1;
    if (!reader_new) {
        return IPX_ERR_EOF;
    }

    IPX_CTX_INFO(inst->m_ctx, "Reading from file '%s'...", file_name);
    inst->m_file = std::move(reader_new);
    return IPX_OK;
}

// -------------------------------------------------------------------------

int
ipx_plugin_init(ipx_ctx_t *ctx, const char *params)
{
    try {
        std::unique_ptr<Instance> inst(new Instance);
        inst->m_ctx = ctx;
        inst->m_cfg.reset(config_parse(ctx, params));
        if (!inst->m_cfg) {
            throw FDS_exception("Failed to parse the instance configuration!");
        }
        file_list_init(inst.get(), inst->m_cfg->path);
        // Everything seems OK
        ipx_ctx_private_set(ctx, inst.release());
    } catch (const FDS_exception &ex) {
        IPX_CTX_ERROR(ctx, "Initialization failed: %s", ex.what());
        return IPX_ERR_DENIED;
    } catch (const std::exception &ex) {
        IPX_CTX_ERROR(ctx, "Unexpected error has occurred: %s", ex.what());
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
    try {
        auto *inst = reinterpret_cast<Instance *>(cfg);
        file_list_clean(inst);
        delete inst;
    } catch (...) {
        IPX_CTX_ERROR(ctx, "Something bad happened during plugin destruction");
    }
}

int
ipx_plugin_get(ipx_ctx_t *ctx, void *cfg)
{
    // The plugin MUST NOT throw any exception!
    try {
        auto inst = reinterpret_cast<Instance *>(cfg);

        while (true) {
            // Try to send an IPFIX Message with batch of Data Records
            int ret = IPX_ERR_EOF;
            if (inst->m_file) {
                ret = inst->m_file->send_batch();
            }

            switch (ret) {
            case IPX_OK:
                return IPX_OK;
            case IPX_ERR_EOF:
                break;
            default:
                throw FDS_exception("[internal] send_batch() returned unexpected value!");
            }

            // Try to open the next file
            ret = file_next(inst);
            switch (ret) {
            case IPX_OK:
                continue;
            case IPX_ERR_EOF:
                return IPX_ERR_EOF;
            default:
                throw FDS_exception("[internal] file_next() returned unexpected value!");
            }
        }
    } catch (const FDS_exception &ex) {
        IPX_CTX_ERROR(ctx, "Unable to extract data from a FDS file: %s", ex.what());
        return IPX_ERR_DENIED;
    } catch (const std::exception &ex) {
        IPX_CTX_ERROR(ctx, "Unexpected error has occurred: %s", ex.what());
        return IPX_ERR_DENIED;
    } catch (...) {
        IPX_CTX_ERROR(ctx, "Unknown error has occurred!", '\0');
        return IPX_ERR_DENIED;
    }

    return IPX_OK;
}

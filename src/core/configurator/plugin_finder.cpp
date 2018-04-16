/**
 * \file src/core/configurator/plugin_finder.cpp
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Plugin finder (source file)
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
#include <functional>

#include <cstdlib>      // realpath
#include <sys/types.h>  // stat
#include <sys/stat.h>   // stat
#include <unistd.h>     // stat
#include <climits>
#include <dlfcn.h>      // dlopen, dlsym,...
#include <dirent.h>     // opendir, readdir, ...
#include <sstream>      // istringstream
#include <build_config.h>

#include "plugin_finder.hpp"

extern "C" {
#include "../verbose.h"
#include <ipfixcol2.h>
}

/** Component identification (for log) */
static const char *comp_str = "Configurator (plugin finder)";

ipx_plugin_finder::~ipx_plugin_finder()
{
    // Unload all plugins
    for (struct ipx_plugin_data *plugin : loaded_plugins) {
        dlclose(plugin->cbs.handle);
        delete plugin;
    }
}

void
ipx_plugin_finder::path_add(const std::string &pathname)
{
    std::size_t pos = pathname.find_first_of('/');
    if (pos == std::string::npos) {
        // Relative path without '/' cannot be used due to the linker (see dlopen)
        paths.emplace_back("./" + pathname);
    } else {
        paths.emplace_back(pathname);
    }
}

const struct ipx_plugin_data *
ipx_plugin_finder::find(const std::string &name, uint16_t type)
{
    struct ipx_plugin_data *result = nullptr;

    // First, try to find it among already loaded plugins
    for (struct ipx_plugin_data *plugin : loaded_plugins) {
        if (plugin->type != type || plugin->name != name) {
            continue;
        }

        // Found
        result = plugin;
        break;
    }

    if (result != nullptr) {
        result->ref_cnt++;
        return result;
    }

    // Not found, try to load it
    std::unique_ptr<ipx_plugin_data> new_plugin(new ipx_plugin_data);
    new_plugin->name = name;
    new_plugin->type = type;
    new_plugin->ref_cnt = 0;

    find_in_paths(name, type, new_plugin->cbs); // Throws an exception on failure
    new_plugin->ref_cnt = 1;

    result = new_plugin.release();
    loaded_plugins.push_back(result);
    return result;
}

/**
 * \brief Try to find plugin in paths defined by a user
 * \param[in]  name Name of the plugin
 * \param[in]  type Plugin type
 * \param[out] cbs  Callbacks
 * \throw runtime_error
 *   If the plugin exists, but an error has occurred - incompatible version, etc.
 */
void
ipx_plugin_finder::find_in_paths(const std::string &name, uint16_t type, struct ipx_ctx_callbacks &cbs)
{
    void *handle = nullptr;

    for (const std::string &path : paths) {
        // Get real path and information about a directory/file
        struct stat file_info;
        std::unique_ptr<char, decltype(&free)> real_path(realpath(path.c_str(), nullptr), &free);

        if (!real_path || stat(real_path.get(), &file_info) != 0) {
            IPX_WARNING(comp_str, "Failed to get info about '%s'. Check if the path exists and "
                "the application has permissions to access it. The module path will be ignored.",
                path.c_str());
            continue;
        }

        // Find  content of the directory/file
        switch (file_info.st_mode & S_IFMT) {
        case S_IFDIR:
            handle = find_in_dir(name, type, real_path.get());
            break;
        case S_IFREG:
            handle = find_in_file(name, type, real_path.get());
            break;
        default:
            handle = nullptr;
            IPX_WARNING(comp_str, "Module path '%s' is not a file or directory. The path will be "
                "ignored.", real_path.get());
            break;
        }

        if (handle != nullptr) {
            // Found
            break;
        }
    }

    if (handle == nullptr) {
        throw std::runtime_error("Unable to find the plugin. Is the plugin installed?");
    }

    // Find required symbols
    auto delete_fn = [](void *handle) {dlclose(handle);};
    std::unique_ptr<void, std::function<void(void*)>> handle_wrap(handle, delete_fn);
    memset(&cbs, 0, sizeof(cbs));

    // Try to find common properties (description, constructor and destructor of an instance)
    *(void **) (&cbs.info) = dlsym(handle_wrap.get(), "ipx_plugin_info");
    if (!cbs.info) {
        throw std::runtime_error("Unable to find the ipx_plugin_info symbol in the plugin!");
    }

    *(void **) (&cbs.init) = dlsym(handle_wrap.get(), "ipx_plugin_init");
    if (!cbs.init) {
        throw std::runtime_error("Unable to find the ipx_plugin_init() function in the plugin!");
    }
    *(void **) (&cbs.destroy) = dlsym(handle_wrap.get(), "ipx_plugin_destroy");
    if (!cbs.destroy) {
        throw std::runtime_error("Unable to find the ipx_plugin_destroy() function in the plugin!");
    }

    if (type == IPX_PT_INPUT) {
        // Try to find the getter function
        *(void **) (&cbs.get) = dlsym(handle_wrap.get(), "ipx_plugin_get");
        if (!cbs.get) {
            throw std::runtime_error("Unable to find the ipx_plugin_get() function in the plugin!");
        }

        // Optional feedback function
        *(void **) (&cbs.ts_close) = dlsym(handle_wrap.get(), "ipx_plugin_session_close");
        IPX_DEBUG(comp_str, "Input plugin '%s' %s requests to close a Transport Session.",
            name.c_str(), (!cbs.ts_close) ? "does not support" : "supports");
    }

    if (type == IPX_PT_OUTPUT || type == IPX_PT_INTERMEDIATE) {
        // Try to find the process function
        *(void **) (&cbs.process) = dlsym(handle_wrap.get(), "ipx_plugin_process");
        if (!cbs.process) {
            throw std::runtime_error("Unable to find the ipx_plugin_process() function in the "
                "plugin!");
        }
    }

    cbs.handle = handle_wrap.release();
}

/**
 * \brief Compare the current version of the collector and a version required by a plugin
 *
 * Compare the current collector version and the version required by the plugin
 * \param[in] required Version required by the plugin
 * \return True if compatible. False otherwise.
 */
bool
ipx_plugin_finder::collector_version_check(std::string required)
{
    constexpr int ARRAY_SIZE = 3;
    int parsed_collector[ARRAY_SIZE] = {
        IPX_BUILD_VERSION_MAJOR,
        IPX_BUILD_VERSION_MINOR,
        IPX_BUILD_VERSION_PATCH
    };

    // Parse the required version
    int parsed_required[ARRAY_SIZE] = {0};
    std::istringstream parser(required);
    parser >> parsed_required[0];
    for(int idx = 1; idx < ARRAY_SIZE; idx++) {
        parser.get(); //Skip period
        parser >> parsed_required[idx];
    }

    // Compare
    if (parsed_collector[0] != parsed_required[0]) {
        return false;
    }

    if (std::lexicographical_compare(parsed_collector, parsed_collector + ARRAY_SIZE,
            parsed_required, parsed_required + ARRAY_SIZE)) {
        return false;
    }

    return true;
}

/**
 * \brief Try to find a plugin in a file
 * \param[in] name Name of the plugin
 * \param[in] type Type of the plugin
 * \param[in] path File path
 * \return Pointer to the handle (found) or nullptr (not found)
 * \throw runtime_error
 *   If the plugin exists, but an error has occurred - incompatible version, etc.
 */
void *
ipx_plugin_finder::find_in_file(const std::string &name, uint16_t type, const char *path)
{
    // Clear previous errors
    dlerror();

    auto delete_fn = [](void *handle) {dlclose(handle);};
    const int flags = RTLD_LAZY | RTLD_LOCAL;
    std::unique_ptr<void, std::function<void(void*)>> handle(dlopen(path, flags), delete_fn);
    if (!handle) {
        IPX_DEBUG(comp_str, "Failed to open plugin in file '%s': %s", path, dlerror());
        return nullptr;
    }

    // Find description
    dlerror();
    void *sym = dlsym(handle.get(), "ipx_plugin_info");
    if (!sym) {
        IPX_DEBUG(comp_str, "Unable to find the plugin description in the file '%s': %s",
            path, dlerror());
        return nullptr;
    }

    struct ipx_plugin_info *info = reinterpret_cast<struct ipx_plugin_info *>(sym);
    if (!info->name || !info->dsc || !info->ipx_min || !info->version) {
        IPX_DEBUG(comp_str, "Description of a plugin in the file '%s' is not valid! Ignoring.",
            path);
        return nullptr;
    }

    if (info->type != type) {
        // Type doesn't match
        return nullptr;
    }

    if (name != info->name) {
        // Name doesn't match
        return nullptr;
    }

    // Found!
    IPX_INFO(comp_str, "Plugin '%s' found in file '%s'.", name.c_str(), path);

    // Check the version string
    if (collector_version_check(std::string(info->ipx_min)) == false) {
        std::string err_msg = "The plugin is not compatible with this version of the collector "
            "(current: " + std::string(IPX_BUILD_VERSION_FULL_STR) + ", required: " + name + ")";
        throw std::runtime_error(err_msg);
    }

    // Now, try to bind all symbols of the plugin
    handle.reset(nullptr); // this calls dlclose()

    dlerror();
    void *new_handle = dlopen(path, RTLD_LOCAL | RTLD_NOW);
    if (!new_handle) {
        std::string err_msg = "Failed to load the plugin: " + std::string(dlerror());
        throw std::runtime_error(err_msg);
    }

    return new_handle;
}

/**
 * \brief Try to find a plugin in a directory
 * \param[in] name Name of the plugin
 * \param[in] type Type of the plugin
 * \param[in] path Directory path
 * \return Pointer to the handle (found) or nullptr (not found)
 * \throw runtime_error
 *   If the plugin exists, but an error has occurred - incompatible version, etc.
 */
void *
ipx_plugin_finder::find_in_dir(const std::string &name, uint16_t type, const char *path)
{
    auto delete_fn = [](DIR *dir) {closedir(dir);};
    std::unique_ptr<DIR, std::function<void(DIR*)>> dir_stream(opendir(path), delete_fn);
    if (!dir_stream) {
        IPX_WARNING(comp_str, "Unable to open directory '%s'. This plugin path will be ignored.",
            path);
        return nullptr;
    }

    struct dirent *rec;
    while ((rec = readdir(dir_stream.get())) != nullptr) {
        if (rec->d_name[0] == '.') {
            // Ignore hidden files
            continue;
        }

        // Create a full path
        std::string file;
        file += path;
        file += "/";
        file += rec->d_name;

        // Get real path and information about a directory/file
        struct stat file_info;
        std::unique_ptr<char, decltype(&free)> path(realpath(file.c_str(), nullptr), &free);
        if (!path || stat(path.get(), &file_info) != 0) {
            IPX_WARNING(comp_str, "Failed to get info about '%s'. Check if the path exists and "
                "the application has permission to access it.", file.c_str());
            continue;
        }

        if ((file_info.st_mode & S_IFREG) == 0) {
            IPX_DEBUG(comp_str, "Non regular file '%s' skipped.", path.get());
            continue;
        }

        void *handle = find_in_file(name, type, path.get());
        if (!handle) {
            // Not found
            continue;
        }

        // Found
        return handle;
    }

    return nullptr;
}
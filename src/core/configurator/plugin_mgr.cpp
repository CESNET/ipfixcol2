/**
 * \file src/core/configurator/plugin_mgr.cpp
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Plugin manager (header file)
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

#include <functional>
#include <memory>
#include <sstream>

#include <cstdlib>      // realpath
#include <sys/types.h>  // stat
#include <sys/stat.h>   // stat
#include <unistd.h>     // stat
#include <dlfcn.h>      // dlopen
#include <dirent.h>     // opendir
#include <algorithm>
#include <cstring>      // memset
#include <iostream>
#include "plugin_mgr.hpp"

extern "C" {
#include "../verbose.h"
#include <ipfixcol2.h>
#include <build_config.h>
}

/** Component identification (for log) */
static const char *comp_str = "Configurator (plugin manager)";

ipx_plugin_mgr::ipx_plugin_mgr()
{
    unload = true; // Unload plugins on exit
}

ipx_plugin_mgr::~ipx_plugin_mgr()
{
    // Delete all loaded plugins
    for (ipx_plugin_mgr::plugin *plugin : loaded) {
        delete plugin;
    }
}

void
ipx_plugin_mgr::path_add(const std::string &pathname)
{
    paths.emplace_back(pathname);
}

void
ipx_plugin_mgr::auto_unload(bool enabled)
{
    unload = enabled;

    // Change the behaviour of all loaded plugins
    for (ipx_plugin_mgr::plugin *plugin : loaded) {
        plugin->auto_unload(enabled);
    }
}

void
ipx_plugin_mgr::plugin_unload_unused()
{
    auto func = [](ipx_plugin_mgr::plugin *plugin) {
        if (plugin->ref_cnt == 0) {
            // Delete the plugin and remove it from the vector
            delete plugin;
            return true;
        }

        return false;
    };

    loaded.erase(std::remove_if(loaded.begin(), loaded.end(), func), loaded.end());
}

/**
 * \brief Reload plugin cache
 *
 * The function tries to find all available plugins in paths specified by user. Information
 * about a type, a name and a path to each plugin is stored into the cache.
 */
void ipx_plugin_mgr::cache_reload()
{
    cache.clear();

    for (const std::string &path : paths) {
        // Get the absolute path and information about a directory/file
        struct stat file_info;
        std::unique_ptr<char, decltype(&free)> abs_path(realpath(path.c_str(), nullptr), &free);
        if (!abs_path || stat(abs_path.get(), &file_info) == -1) {
            const char *err_str;
            ipx_strerror(errno, err_str);
            IPX_WARNING(comp_str, "Unable to access to plugin(s) in '%s': %s", path.c_str(), err_str);
            continue;
        }

        switch (file_info.st_mode & S_IFMT) {
        case S_IFDIR:
            cache_add_dir(abs_path.get());
            break;
        case S_IFREG:
            cache_add_file(abs_path.get());
            break;
        default:
            IPX_WARNING(comp_str, "Unable to access to plugin(s) in '%s': Not a file or directory",
                path.c_str());
            break;
        }
    }

    IPX_INFO(comp_str, "%zu plugins found", cache.size());
}

/**
 * \brief Add plugins in a directory to the plugin cache (auxiliary function)
 * \param[in] path Plugin directory
 */
void
ipx_plugin_mgr::cache_add_dir(const char *path)
{
    auto delete_fn = [](DIR *dir) {closedir(dir);};
    std::unique_ptr<DIR, std::function<void(DIR*)>> dir_stream(opendir(path), delete_fn);
    if (!dir_stream) {
        const char *err_str;
        ipx_strerror(errno, err_str);
        IPX_WARNING(comp_str, "Unable to open directory '%s': %s", path, err_str);
        return;
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

        // Get the absolute path and information about a directory/file
        struct stat file_info;
        std::unique_ptr<char, decltype(&free)> abs_path(realpath(file.c_str(), nullptr), &free);
        if (!abs_path || stat(abs_path.get(), &file_info) == -1) {
            const char *err_str;
            ipx_strerror(errno, err_str);
            IPX_WARNING(comp_str, "Unable to access to a plugin in '%s': %s", file.c_str(),
                err_str);
            continue;
        }

        if ((file_info.st_mode & S_IFREG) == 0) {
            IPX_WARNING(comp_str, "Non-regular file '%s' skipped.", abs_path.get());
            continue;
        }

        cache_add_file(abs_path.get());
    }
}

/**
 * \brief Add a plugin to the plugin cache (auxiliary function)
 * \param[in] path Absolute plugin path
 */
void
ipx_plugin_mgr::cache_add_file(const char *path)
{
    // Clear previous errors
    dlerror();

    // Try to load the plugin (and unload it automatically)
    const int flags = RTLD_LAZY | RTLD_LOCAL;
    auto delete_fn = [](void *handle) {dlclose(handle);};
    std::unique_ptr<void, std::function<void(void*)>> handle(dlopen(path, flags), delete_fn);
    if (!handle) {
        IPX_WARNING(comp_str, "Failed to open file '%s' as plugin: %s", path, dlerror());
        return;
    }

    // Find a description and check it
    dlerror();
    void *sym = dlsym(handle.get(), "ipx_plugin_info");
    if (!sym) {
        IPX_WARNING(comp_str, "Unable to get a plugin description of '%s': %s", path, dlerror());
        return;
    }

    struct ipx_plugin_info *info = reinterpret_cast<struct ipx_plugin_info *>(sym);
    if (!info->name || !info->dsc || !info->ipx_min || !info->version) {
        IPX_WARNING(comp_str, "Description of a plugin in the file '%s' is not valid!", path);
        return;
    }

    uint16_t type = info->type;
    if (type != IPX_PT_INPUT && type != IPX_PT_INTERMEDIATE && type != IPX_PT_OUTPUT) {
        IPX_WARNING(comp_str, "Plugin type of a plugin in the file '%s' is not valid!", path);
        return;
    }

    // Add the plugin to cache
    struct cache_entry entry = {type, info->name, path};
    cache.push_back(entry);
}

ipx_plugin_mgr::plugin_ref *
ipx_plugin_mgr::plugin_get(uint16_t type, const std::string &name)
{
    // Check if the plugin has been loaded earlier
    for (ipx_plugin_mgr::plugin *plugin : loaded) {
        if (plugin->get_type() != type || plugin->get_name() != name) {
            continue;
        }

        return new ipx_plugin_mgr::plugin_ref(plugin);
    }

    //  Try to find the plugin in the cache
    if (cache.empty()) {
        cache_reload();
    }

    const std::string *plugin_path = nullptr;
    for (const struct cache_entry &entry : cache) {
        if (entry.type != type || entry.name != name) {
            continue;
        }

        // Found
        plugin_path = &entry.path;
        break;
    }

    if (plugin_path == nullptr) {
        throw ipx_plugin_mgr::error("Unable to find the '" + name + "' plugin.");
    }

    /* Load the plugin and make sure it is the plugin we wanted (i.e. nothing has changed since
     * the plugin cache reload) */
    ipx_plugin_mgr::plugin *new_plugin = new plugin(*plugin_path, unload);
    if (new_plugin->get_name() != name || new_plugin->get_type() != type) {
        // Someone changed the name or the type of a plugin
        delete new_plugin;
        throw ipx_plugin_mgr::error("Invalid record in a plugin cache (type or name of the "
            "plugin mismatch)");
    }

    // Add it to the list of loaded plugins
    loaded.push_back(new_plugin);
    return new plugin_ref(new_plugin);
}

void
ipx_plugin_mgr::plugin_list()
{
    cache_reload();

    std::vector<struct list_entry> plugins_input;
    std::vector<struct list_entry> plugins_inter;
    std::vector<struct list_entry> plugins_output;

    // Prepare descriptions of all plugins in the cache
    for (const cache_entry &cache_entry : cache) {
        // Copy basic parameters of plugins
        struct list_entry plugin_entry;
        plugin_entry.type = cache_entry.type;
        plugin_entry.name = cache_entry.name;
        plugin_entry.path = cache_entry.path;

        // Try to open the plugin
        dlerror();

        const char *path = cache_entry.path.c_str();
        const int flags = RTLD_LAZY | RTLD_LOCAL;
        auto delete_fn = [](void *handle) {dlclose(handle);};

        std::unique_ptr<void, std::function<void(void*)>> handle(dlopen(path, flags), delete_fn);
        if (!handle) {
            IPX_WARNING(comp_str, "Failed to open file '%s' as plugin: %s", path, dlerror());
            continue;
        }

        void *sym = dlsym(handle.get(), "ipx_plugin_info");
        if (!sym) {
            IPX_WARNING(comp_str, "Unable to get a plugin description of '%s': %s", path, dlerror());
            continue;
        }

        struct ipx_plugin_info *info = reinterpret_cast<struct ipx_plugin_info *>(sym);
        if (!info->name || !info->dsc || !info->ipx_min || !info->version) {
            IPX_WARNING(comp_str, "Description of a plugin in the file '%s' is not valid!", path);
            continue;
        }

        // Load other parameters (version, limitations. etc.)
        if (cache_entry.type != info->type || cache_entry.name != plugin_entry.name) {
            IPX_WARNING(comp_str, "Mismatch between a cache entry and a plugin description of "
                "the plugin in the file '%s'. Skipping.", path);
            continue;
        }

        plugin_entry.description = info->dsc;
        plugin_entry.version = info->version;
        bool version_ok;

        try {
            version_ok = version_check(info->ipx_min);
        } catch (const ipx_plugin_mgr::error &ex) {
            IPX_WARNING(comp_str, "Failed to check the minimal required version of the collector "
                "of a plugin in the file '%s': %s", path, ex.what());
            continue;
        }

        if (info->flags & IPX_PF_DEEPBIND) {
#ifdef HAVE_RTLD_DEEPBIND
            plugin_entry.msg_notes.emplace_back("Deep bind (RTLD_DEEPBIND) required");
#else
            plugin_entry.msg_warning.emplace_back(
                "Deep bind (RTLD_DEEPBIND) required but not supported by C library");
#endif
        }

        if (!version_ok) {
            std::stringstream ss;
            ss << "incompatible with this collector version (min. required: ";
            ss << info->ipx_min << ", current: " << IPX_BUILD_VERSION_FULL_STR << ")";
            plugin_entry.msg_warning.emplace_back(ss.str());
        }

        std::vector<struct list_entry> *list;
        switch (plugin_entry.type) {
        case IPX_PT_INPUT:        list = &plugins_input;  break;
        case IPX_PT_INTERMEDIATE: list = &plugins_inter;  break;
        case IPX_PT_OUTPUT:       list = &plugins_output; break;
        default:
            IPX_WARNING(comp_str, "Unexpected type of a plugin in the file '%s'. Skipping.", path);
            continue;
        }

        // Ignore the plugins if there is already another plugins with the same name
        bool ignore = false;
        for (const struct list_entry &entry : *list) {
            if (entry.name != plugin_entry.name) {
                continue;
            }

            // Found
            IPX_WARNING(comp_str, "Plugin '%s' in the file '%s' ignored, because another plugin "
                "with the same type and name was previously found!", plugin_entry.name.c_str(), path);
            ignore = true;
            break;
        }

        if (ignore) {
            continue;
        }

        list->emplace_back(plugin_entry);
    }

    // Print information about available plugins on the standard output
    auto sort_fn = [](const struct list_entry &rhs, const struct  list_entry &lhs) {
        return (rhs.name < lhs.name);
    };
    std::sort(plugins_input.begin(), plugins_input.end(), sort_fn);
    std::sort(plugins_inter.begin(), plugins_inter.end(), sort_fn);
    std::sort(plugins_output.begin(), plugins_output.end(), sort_fn);

    plugin_list_print("INPUT PLUGINS", plugins_input);
    plugin_list_print("INTERMEDIATE PLUGINS", plugins_inter);
    plugin_list_print("OUTPUT PLUGINS", plugins_output);
}

/**
 * \brief Print all plugins in a catalog (auxiliary function)
 * \param[in] name Name of the category
 * \param[in] list List of plugin descriptions
 */
void
ipx_plugin_mgr::plugin_list_print(const std::string &name, const std::vector<struct list_entry> &list)
{
    constexpr const char *color_reset =  "\x1b[0m";
    constexpr const char *color_red =    "\x1b[31m";
    constexpr const char *color_green =  "\x1b[32m";

    std::cout << " " << name << "\n";
    for (size_t i = 0; i < name.length() + 2; ++i) {
        std::cout << "=";
    }
    std::cout << "\n";

    if (list.empty()) {
        std::cout << "  (no plugins found)\n" << std::endl;
        return;
    }

    // Print all plugins
    for (const struct list_entry &plugin : list) {
        std::cout
            << "- Name :       " << color_green << plugin.name << color_reset << "\n"
            << "  Description: " << plugin.description << "\n"
            << "  Path:        " << plugin.path << "\n"
            << "  Version:     " << plugin.version << "\n";

        if (!plugin.msg_notes.empty()) {
            std::cout << "  Notes:" << "\n";
            for (const std::string &str : plugin.msg_notes) {
                std::cout << "  - " << str << "\n";
            }
        }

        if (!plugin.msg_warning.empty()) {
            std::cout << "  " << color_red << "Warnings:" << color_reset << "\n";
            for (const std::string &str : plugin.msg_warning) {
                std::cout << "  - " << str << "\n";
            }
        }

        std::cout << std::endl;
    }
}

/**
 * \brief Compare the current version of the collector and a version required by a plugin
 * \param[in] min_version Version required by the plugin
 * \return True if compatible. False otherwise.
 * \throw ipx_plugin_mgr::error if the version string \p min_version is not a valid version string
 */
bool
ipx_plugin_mgr::version_check(const std::string &min_version)
{
    static const int FIELDS_CNT = 3;
    int version_col[FIELDS_CNT] = {
        IPX_BUILD_VERSION_MAJOR,
        IPX_BUILD_VERSION_MINOR,
        IPX_BUILD_VERSION_PATCH
    };

    // Parse the required version
    int version_req[FIELDS_CNT] = {0};
    std::istringstream parser(min_version);

    parser >> version_req[0];
    for (int idx = 1; idx < FIELDS_CNT; idx++) {
        parser.get(); //Skip period
        parser >> version_req[idx];
        if (!parser.eof() && parser.fail()) {
            throw std::invalid_argument("Invalid version string");
        }
    }

    if (!parser.eof()) {
        throw std::invalid_argument("Invalid version string");
    }

    // Compare version
    if (version_col[0] != version_req[0]) {
        return false; // Major version must match!
    }

    if (std::lexicographical_compare(version_col, version_col + FIELDS_CNT,
        version_req, version_req + FIELDS_CNT)) {
        return false;
    }

    return true;
}

// -------------------------------------------------------------------------------------------------

/**
 * \brief Class constructor
 *
 * Load a plugin from a file and find all required symbols.
 * \param[in] path        Path to the plugin to load
 * \param[in] auto_unload Automatically unload the plugin on destroy (can be changed later)
 */
ipx_plugin_mgr::plugin::plugin(const std::string &path, bool auto_unload)
    : unload(auto_unload)
{
    auto delete_fn = [](void *handle) {dlclose(handle);};
    std::unique_ptr<void, std::function<void(void*)>> handle_wrap(nullptr, delete_fn);

    const struct ipx_plugin_info *info;
    int load_flags = RTLD_NOW | RTLD_LOCAL;

    // Initialize default parameters
    ref_cnt = 0;
    std::memset(&cbs, 0, sizeof(cbs));

    // Determine whether or not to use deep bind
    dlerror();
    handle_wrap.reset(dlopen(path.c_str(), RTLD_LAZY | RTLD_LOCAL));
    if (!handle_wrap) {
        // Failed to open the file
        std::string err_msg = "Failed to load a plugin from '" + path + "': " + dlerror();
        throw ipx_plugin_mgr::error(err_msg);
    }

    *(void **)(&info) = symbol_get(handle_wrap.get(), "ipx_plugin_info", false);
    if (info->flags & IPX_PF_DEEPBIND) {
#ifdef HAVE_RTLD_DEEPBIND
        IPX_DEBUG(comp_str, "Loading plugin from '%s' using RTLD_DEEPBIND flag!", path.c_str());
        load_flags |= RTLD_DEEPBIND;
#else
        std::string err_msg = "Deep bind (RTLD_DEEPBIND) required but not supported by C library";
        throw ipx_plugin_mgr::error("Failed to load a plugin from '" + path + "': " + err_msg);
#endif
    }

    // Close the plugin and reload it
    handle_wrap.reset();

    dlerror();
    handle_wrap.reset(dlopen(path.c_str(), load_flags));
    if (!handle_wrap) {
        // Failed to open the file
        std::string err_msg = "Failed to load a plugin from '" + path + "': " + dlerror();
        throw ipx_plugin_mgr::error(err_msg);
    }

    // Try to load all symbols
    try {
        symbol_load_all(handle_wrap.get(), cbs);
    } catch (const ipx_plugin_mgr::error &ex) {
        throw ipx_plugin_mgr::error("Failed to load a plugin from '" + path + "': " + ex.what());
    }

    name = cbs.info->name;
    type = cbs.info->type;
    cbs.handle = handle_wrap.release(); // The handle is now part of the callback structure

    IPX_DEBUG(comp_str, "Plugin '%s' has been successfully loaded from '%s'.", name.c_str(),
        path.c_str());
}

/**
 * \brief Class destructor
 */
ipx_plugin_mgr::plugin::~plugin()
{
    if (ref_cnt > 0) {
        IPX_WARNING(comp_str, "Internal reference counter of '%s' plugin is not zero!",
            name.c_str());
    }

    if (unload) {
        dlclose(cbs.handle);

        const char *type_str;
        switch (type) {
        case IPX_PT_INPUT:        type_str = "Input plugin"; break;
        case IPX_PT_INTERMEDIATE: type_str = "Intermediate plugin"; break;
        case IPX_PT_OUTPUT:       type_str = "Output plugin"; break;
        default:                  type_str = "Plugin"; break;
        }

        IPX_DEBUG(comp_str, "%s '%s' unloaded.", type_str, name.c_str());
    }
}

/**
 * \brief Enable/disable automatically unload of the plugin object on destroy
 * \param[in] enable True to enable, false to disable
 */
void
ipx_plugin_mgr::plugin::auto_unload(bool enable)
{
    unload = enable;
}

/**
 * \brief Get a symbol of a plugin
 *
 * Try to find the symbol using plugin object handle. The symbol cannot be nullptr.
 * \param[in] handle Handle of the dynamic loaded plugin
 * \param[in] name   Name of the symbol to find
 * \param[in] opt    Symbol is optional (i.e. if the symbol is not available, do not throw and
 *   exception and return nullptr)
 * \return Pointer to the symbol or nullptr
 * \throw ipx_plugin_finder::error if the symbol is not available
 */
void *
ipx_plugin_mgr::plugin::symbol_get(void *handle, const char *name, bool opt)
{
    dlerror(); // Clear the last error message
    void *result = dlsym(handle, name);
    const char *err_msg = dlerror();

    if (err_msg != nullptr) {
        // Symbol not found
        if (opt) {
            return nullptr;
        }

        std::string msg = "Unable to find '" + std::string(name) + "' symbol: " + err_msg;
        throw ipx_plugin_mgr::error(msg);
    }

    if (result == nullptr) {
        std::string msg = "Symbol '" + std::string(name) + "' is available, but is NULL";
        throw ipx_plugin_mgr::error(msg);
    }

    return result;
}

/**
 * \brief Load all symbols and initialize callbacks structure
 *
 * \note The \p cbs structure is erased and filled.
 * \param[in]  handle File handler
 * \param[out] cbs    Callback structure
 * \throw ipx_plugin_finder::error if any symbol is missing or an internal description is invalid
 */
void
ipx_plugin_mgr::plugin::symbol_load_all(void *handle, struct ipx_ctx_callbacks &cbs)
{
    // Clear the result structure
    memset(&cbs, 0, sizeof(cbs));
    cbs.handle = handle;

    // First, try to load common symbols (a description, an instance constructor/destructor)
    *(void **) (&cbs.info) = symbol_get(handle, "ipx_plugin_info");
    *(void **) (&cbs.init) = symbol_get(handle, "ipx_plugin_init");
    *(void **) (&cbs.destroy) = symbol_get(handle, "ipx_plugin_destroy");

    // Check the loaded symbols
    const ipx_plugin_info *p_info = cbs.info;
    if (!p_info->name || !p_info->version || !p_info->dsc || !p_info->ipx_min) {
        throw ipx_plugin_mgr::error("Plugin description structure is not valid!");
    }

    if (!ipx_plugin_mgr::version_check(std::string(p_info->ipx_min))) {
        std::string err_msg = "Plugin is not compatible with this version of the collector ";
        err_msg += "(current: " + std::string(IPX_BUILD_VERSION_FULL_STR);
        err_msg += ", required: " + std::string(p_info->ipx_min) + ")";
        throw ipx_plugin_mgr::error(err_msg);
    }

    // Load plugin specific functions
    uint16_t type = p_info->type;
    if (type != IPX_PT_INPUT && type != IPX_PT_OUTPUT && type != IPX_PT_INTERMEDIATE) {
        throw ipx_plugin_mgr::error("Invalid type of the plugin!");
    }

    if (type == IPX_PT_INPUT) {
        // Try to find the getter function
        *(void **) (&cbs.get) = symbol_get(handle, "ipx_plugin_get");
        *(void **) (&cbs.ts_close) = symbol_get(handle, "ipx_plugin_session_close", true);

        IPX_DEBUG(comp_str, "Input plugin '%s' %s requests to close a Transport Session.",
            p_info->name, (!cbs.ts_close) ? "does not support" : "supports");
    }

    if (type == IPX_PT_INTERMEDIATE || type == IPX_PT_OUTPUT) {
        // Try to find the process function
        *(void **) (&cbs.process) = symbol_get(handle, "ipx_plugin_process");
    }
}

// -------------------------------------------------------------------------------------------------

ipx_plugin_mgr::plugin_ref::plugin_ref(ipx_plugin_mgr::plugin *plugin)
{
    ref = plugin;
    ref->ref_cnt++;
}

ipx_plugin_mgr::plugin_ref::~plugin_ref()
{
    ref->ref_cnt--;
}

ipx_plugin_mgr::plugin_ref::plugin_ref(const ipx_plugin_mgr::plugin_ref &others)
{
    ref = others.ref;
    ref->ref_cnt++;
}

ipx_plugin_mgr::plugin_ref &
ipx_plugin_mgr::plugin_ref::operator=(const ipx_plugin_mgr::plugin_ref &others)
{
    if (this == &others) {
        // Nothing to do
        return *this;
    }

    // Decrement the previous reference
    ref->ref_cnt--;

    // Increment the new reference
    ref = others.ref;
    ref->ref_cnt++;
    return *this;
}

/**
 * \file src/core/configurator/plugin_mgr.hpp
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

#ifndef IPX_PLUGIN_MGR
#define IPX_PLUGIN_MGR

#include <string>
#include <vector>
#include <stdexcept>

extern "C" {
#include "../context.h"
}

/**
 * \brief Plugin finder
 *
 * The class performs plugin lookup, loads and unloads plugins and manages them.
 */
class ipx_plugin_mgr {
public:
    // Forward declarations
    class error;
    class plugin;
    class plugin_ref;
private:
    /** Plugin cache entry (information about an available plugin)                     */
    struct cache_entry {
        /** Plugin type (one of #IPX_PT_INPUT, #IPX_PT_INTERMEDIATE, #IPX_PT_OUTPUT)   */
        uint16_t type;
        /** Plugin name                                                                */
        std::string name;
        /** Path to the plugin                                                         */
        std::string path;
    };

    /** Description of a plugin (for output)                                           */
    struct list_entry {
        /** Plugin type (one of #IPX_PT_INPUT, #IPX_PT_INTERMEDIATE, #IPX_PT_OUTPUT)   */
        uint16_t type;
        /** Identification name                                                        */
        std::string name;
        /** Plugin description                                                         */
        std::string description;
        /** Version string                                                             */
        std::string version;
        /** Absolute path to the plugin                                                */
        std::string path;
        /** Informational messages                                                     */
        std::vector<std::string> msg_notes;
        /** Error messages (blocks start of the plugin)                                */
        std::vector<std::string> msg_warning;
    };

    /** Unload unused plugins                                                          */
    bool unload;
    /** Search paths paths (directories or files)                                      */
    std::vector<std::string> paths;
    /** Loaded plugins                                                                 */
    std::vector<ipx_plugin_mgr::plugin *> loaded;
    /** Plugin cache (list of available plugins)                                       */
    std::vector<struct cache_entry> cache;

    // Internal functions
    void cache_reload();
    void cache_add_dir(const char *path);
    void cache_add_file(const char *path);
    static bool version_check(const std::string &min_version);
    void plugin_load(const char *path, int type, const std::string name);
    void plugin_list_print(const std::string &name, const std::vector<struct list_entry> &list);

public:
    /** \brief Class constructor    */
    ipx_plugin_mgr();
    /**
     * \brief Class destructor
     * By default, all loaded plugins are automatically unloaded. This behavior can be changed
     * by the auto_unload() function.
     */
    virtual ~ipx_plugin_mgr();

    // Disable copy and move constructors
    ipx_plugin_mgr(const ipx_plugin_mgr &other) = delete;
    ipx_plugin_mgr& operator=(const ipx_plugin_mgr &other) = delete;
    ipx_plugin_mgr(ipx_plugin_mgr &&other) = delete;
    ipx_plugin_mgr& operator=(ipx_plugin_mgr &&other) = delete;

    /**
     * \brief Find a plugin with a given name and type
     *
     * Try to find the plugin and check its version requirements, presence of required callbacks,
     * correctness of its description, etc. If the plugin is available, return a reference
     * to the plugin.
     *
     * \note
     *   Returned plugin reference works as an internal reference counter. This allows the manager
     *   to automatically release plugins that are not required anymore.
     * \note On the first call, an internal plugin cache is build i.e. names and locations
     *   of all available plugins is stored.
     *
     * \param[in]  name Name of the plugin
     * \param[in]  type Plugin type (one of #IPX_PT_INPUT, #IPX_PT_INTERMEDIATE, #IPX_PT_OUTPUT)
     * \return Plugin reference (contains callbacks, description etc.). User MUST destroy the
     *   reference when it is not required anymore.
     * \throw ipx_plugin_mgr::error
     *   If the function failed to find a suitable version of the plugin or version requirements
     *   hasn't been met.
     */
    plugin_ref *
    plugin_get(uint16_t type, const std::string &name);

    /**
     * \brief List all available plugins
     *
     * The function loads information about all available plugins in paths defined by a user
     * and print them on the standard output.
     */
    void
    plugin_list();

    /**
     * \brief Unload loaded plugins that are not used anymore
     */
    void
    plugin_unload_unused();

    /**
     * \brief Invalidate internal plugin cache
     *
     * Next time when a plugin_get() is called and a required plugin is not already loaded,
     * the plugin cache will be rebuild. This allows you to load plugins that haven't been
     * available during previous cache build.
     */
    void
    cache_invalidate() {cache.clear();};

    /**
     * \brief Add path to a plugin or directory with plugins
     *
     * \note
     *   Order of added paths matters. If multiple plugins with the same names and types are
     *   present in defined paths, only the first match is used during plugin lookup. Other matches
     *   are ignored! Therefore, first add the most specific paths and later generic paths.
     * \param[in] pathname Path to
     */
    void
    path_add(const std::string &pathname);

    /**
     * \brief Enable/disable automatically unload of all plugins on destroy
     *
     * This options allows plugin developers to disable automatic unload of plugins. Disabled
     * unload leaves plugin symbols available even after the collector shutdown. This is necessary
     * for analysis of performance and memory leaks. For example, valgrind is unable to identify
     * position of memory leaks of unloaded plugins.
     * \note By default, automatic unload is enabled.
     * \param[in] enabled True to enable, false to disable
     */
    void
    auto_unload(bool enabled);
};

/**
 * \brief Plugin finder custom error
 */
class ipx_plugin_mgr::error : public std::runtime_error {
public:
    /**
     * \brief Manager error constructor (string constructor)
     * \param[in] msg An error message
     */
    explicit error(const std::string &msg) : std::runtime_error(msg) {};
    /**
     * \brief Manager error constructor (char constructor)
     * \param[in] msg An error message
     */
    explicit error(const char *msg) : std::runtime_error(msg) {};
};

/**
 * \brief Main plugin handler
 *
 * The class performs loading of the plugin from a file and parse all required symbols
 * (such as instance constructor, destructor, etc.).
 */
class ipx_plugin_mgr::plugin {
private:
    /** Plugin type                                              */
    uint16_t type;
    /** Plugin name                                              */
    std::string name;
    /** Prepared callbacks etc.                                  */
    struct ipx_ctx_callbacks cbs;
    /** Automatically unload the plugin object on destruction    */
    bool unload;
    /** Reference counter (number of active instances)           */
    int ref_cnt;

    // Private constructor and destructor accessible only from the manager
    plugin(const std::string &path, bool auto_unload = true);
    virtual ~plugin();

    // Disable copy and move constructors
    plugin(const plugin &other) = delete;
    plugin& operator=(const plugin &other) = delete;
    plugin(plugin &&other) = delete;
    plugin& operator=(plugin &&other) = delete;

    // Other internal functions
    void *symbol_get(void *handle, const char *name, bool opt = false);
    void  symbol_load_all(void *handle, struct ipx_ctx_callbacks &cbs);
    void  auto_unload(bool enable);

    friend class ipx_plugin_mgr;
public:
    /**
     * \brief Get plugin callbacks
     * \return Reference to callbacks
     */
    const struct ipx_ctx_callbacks *
    get_callbacks() const {return &cbs;};

    /**
     * \brief Get the name of the plugin
     * \return Name
     */
    const std::string &
    get_name() const {return name;};

    /**
     * \brief Get the type of the plugin
     * \return Type
     */
    uint16_t
    get_type() const {return type;};
};

/**
 * \brief Plugin reference
 *
 * The plugin manager uses this class to monitor how many references of a plugin exist.
 * Thus, if there are no references, the manager can safely release the plugin.
 */
class ipx_plugin_mgr::plugin_ref {
private:
    /** Reference to a plugin */
    ipx_plugin_mgr::plugin *ref;
    /**
     * \brief Private constructor
     * \param[in] plugin Plugin
     */
    plugin_ref(ipx_plugin_mgr::plugin *plugin);

    friend class ipx_plugin_mgr;
public:
    /**
     * \brief Reference destructor
     * \note Automatically decrement reference counter
     */
    ~plugin_ref();
    /**
     * \brief Copy constructor
     * \param[in] others Source reference
     */
    plugin_ref(const plugin_ref &others);
    /**
     * \brief Copy assign constructor
     * \param[in] others Source reference
     * \return A new plugin reference
     */
    plugin_ref& operator=(const plugin_ref &others);

    /**
     * \brief Get a reference to the plugin
     * \return Plugin reference
     */
    const ipx_plugin_mgr::plugin *
    get_plugin() {return ref;};
};

#endif // IPX_PLUGIN_MGR

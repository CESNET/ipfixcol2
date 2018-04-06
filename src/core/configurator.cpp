//
// Created by lukashutak on 04/04/18.
//

#include <memory>
#include <iostream>
#include <functional>
#include <dlfcn.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <climits>
#include <cstdlib>
#include <dirent.h>

#include "configurator.hpp"
extern "C" {
    #include "verbose.h"
}

/** Component identification (for log) */
static const char *comp_str = "Configurator";

/** Plugin structure */
struct plugin_handler {
    /** Library handle                                               */
    void *lib_handle;
    /** Number of instances that use this plugin                     */
    unsigned int instance_cnt;

    struct {
        /** Description of the module                                */
        struct ipx_plugin_info *plugin_info;

        /** Plugin instance initialization                           */
        int (*plugin_init)(ipx_ctx_t *, const char *);
        /** Plugin instance destruction                              */
        void (*plugin_destroy)(ipx_ctx_t *, void *);
        /** Get an IPFIX (or NetFlow) message (Input plugins ONLY)   */
        int (*plugin_get)(ipx_ctx_t *, void *);
        /** Process a message (Intermediate and Output plugins ONLY) */
        int (*plugin_process)(ipx_ctx_t *, void *, ipx_msg_t *);
    } symbols;
};


Configurator::Configurator()
{
    plugin_finder = new Plugin_finder();
}

Configurator::~Configurator()
{
    delete plugin_finder;
}

void
Configurator::Plugin_finder::path_add(const std::string &pathname)
{
    std::size_t pos = pathname.find_first_of('/');
    if (pos == std::string::npos) {
        // Relative path without '/' cannot be used due to the linker
        paths.emplace_back("./" + pathname);
    } else {
        paths.emplace_back(pathname);
    }
}

void
Configurator::Plugin_finder::list()
{
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

        // Show content of the directory/file
        switch (file_info.st_mode & S_IFMT) {
        case S_IFDIR:
            list_dir(real_path.get());
            break;
        case S_IFREG:
            list_file(real_path.get());
            break;
        default:
            IPX_WARNING(comp_str, "Module path '%s' is not a file or directory. The path will be "
                "ignored.", real_path.get());
            break;
        }
    }
}

void
Configurator::Plugin_finder::list_dir(const char *dir)
{
    auto delete_fn = [](DIR *dir) {closedir(dir);};
    std::unique_ptr<DIR, std::function<void(DIR*)>> dir_stream(opendir(dir), delete_fn);
    if (!dir_stream) {
        IPX_WARNING(comp_str, "Unable to open directory '%s'. The module path will be ignored.",
            dir);
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
        file += dir;
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
            IPX_INFO(comp_str, "Non regular file '%s' skipped.", path.get());
            continue;
        }

        list_file(path.get());
    }
}

/**
 * \brief Show information about a module in a file
 * \param[in] file Path to a file
 */
void
Configurator::Plugin_finder::list_file(const char *file)
{
    // Clear previous errors
    dlerror();

    auto delete_fn = [](void *handle) {dlclose(handle);};
    const int flags = RTLD_LAZY | RTLD_LOCAL;
    std::unique_ptr<void, std::function<void(void*)>> handle(dlopen(file, flags), delete_fn);
    if (!handle) {
        IPX_WARNING(comp_str, "Failed to open module '%s': %s", file, dlerror());
        return;
    }

    // Find description
    dlerror();
    void *sym = dlsym(handle.get(), "ipx_plugin_info");
    if (!sym) {
        IPX_WARNING(comp_str, "Unable to find the plugin description in the module '%s': %s",
            file, dlerror());
        return;
    }

    struct ipx_plugin_info *info = reinterpret_cast<struct ipx_plugin_info *>(sym);
    switch (info->type) {
    case IPX_PT_INPUT:        std::cout << "Input plugin '";        break;
    case IPX_PT_INTERMEDIATE: std::cout << "Intermediate plugin '"; break;
    case IPX_PT_OUTPUT:       std::cout << "Output plugin '";       break;
    default:
        IPX_WARNING(comp_str, "Unknown type of a plugin in the module '%s'", file);
        return;
    }

    std::cout << info->name << "'\n";
    std::cout << "Description: " << info->dsc << "\n";
    std::cout << "Version:     " << info->version << "\n";
    std::cout << "Path:        " << file << "\n";
}








int
ipx_config_input_add(const struct ipx_cfg_input *cfg)
{
    std::cout << "Request to add input plugin:\n"
        << "\tPlugin:   " << cfg->common.plugin    << "\n"
        << "\tName:     " << cfg->common.name      << "\n"
        << "\tVebosity: " << std::to_string(cfg->common.verb_mode) << "\n"
        << "\tParams:   " << cfg->common.params    << "\n";
    return IPX_OK;
}

int
ipx_config_inter_add(const struct ipx_cfg_inter *cfg)
{
    std::cout << "Request to add intermediate plugin:\n"
        << "\tPlugin:   " << cfg->common.plugin    << "\n"
        << "\tName:     " << cfg->common.name      << "\n"
        << "\tVebosity: " << std::to_string(cfg->common.verb_mode) << "\n"
        << "\tParams:   " << cfg->common.params    << "\n";
    return IPX_OK;
}

int
ipx_config_output_add(const struct ipx_cfg_output *cfg)
{
    std::cout << "Request to add output plugin:\n"
        << "\tPlugin:   " << cfg->common.plugin    << "\n"
        << "\tName:     " << cfg->common.name      << "\n"
        << "\tVebosity: " << std::to_string(cfg->common.verb_mode) << "\n";
    switch (cfg->odid_filter.type) {
    case IPX_CFG_ODID_FILTER_NONE:
        std::cout << "\tODID:     all\n";
        break;
    case IPX_CFG_ODID_FILTER_ONLY:
        std::cout << "\tODID:     only "<< cfg->odid_filter.expression << "\n";
        break;
    case IPX_CFG_ODID_FILTER_EXCEPT:
        std::cout << "\tODID:     except "<< cfg->odid_filter.expression << "\n";
        break;
    }
    std::cout << "\tParams:   " << cfg->common.params    << "\n";
    return IPX_OK;
}
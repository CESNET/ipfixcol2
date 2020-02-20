
#ifndef IPX_CFG_EXTENSIONS_H
#define IPX_CFG_EXTENSIONS_H

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "instance.hpp"
#include "instance_intermediate.hpp"
#include "instance_output.hpp"

extern "C" {
#include "../extension.h"
#include "../verbose.h"
}

class ipx_cfg_extensions {
private:
    struct plugin_rec {
        std::string name;
        unsigned int plugin_idx;
        const struct ipx_ctx_ext *rec;

        bool operator<(const plugin_rec &other) const {
            return plugin_idx < other.plugin_idx;
        }
    };

    struct ext_rec {
        std::vector<plugin_rec> producers;
        std::vector<plugin_rec> consumers;

        size_t size;
        size_t offset;
        uint64_t mask;
    };

    // Extensions [extension type][extension name]
    std::map<std::string, std::map<std::string, struct ext_rec>> m_extensions;

    void
    add_extension(ipx_instance *inst, unsigned int idx, struct ipx_ctx_ext *ext);
    void
    check_dependencies(const std::string &ident, const struct ext_rec &rec);

public:
    ipx_cfg_extensions() = default;
    ~ipx_cfg_extensions() = default;

    void
    resolve(std::vector<ipx_instance *> &plugins);
};

#endif // IPX_CFG_EXTENSIONS_H
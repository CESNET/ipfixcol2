/**
 * \file src/plugins/output/fds/src/Config.hpp
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \author Michal Sedlak <sedlakm@cesnet.cz>
 * \brief Parser of XML configuration (header file)
 * \date 2019
 *
 * Copyright(c) 2019 CESNET z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef IPFIXCOL2_FDS_CONFIG_HPP
#define IPFIXCOL2_FDS_CONFIG_HPP

#include <string>
#include <vector>
#include <libfds.h>

/**
 * @brief Plugin configuration parser
 */
class Config {
public:
    /**
     * @brief Parse configuration of the plugin
     * @param[in] params XML parameters to parse
     * @param[in] iemgr Information elements manager
     * @throw runtime_exception on error
     */
    Config(const char *params, const fds_iemgr_t *iemgr);
    ~Config() = default;

    enum class calg {
        NONE, ///< Do not use compression
        LZ4,  ///< LZ4 compression
        ZSTD  ///< ZSTD compression
    };

    /// Storage path
    std::string m_path;
    /// Compression algorithm
    calg m_calg;
    /// Asynchronous I/O enabled
    bool m_async;

    struct {
        bool     align;   ///< Enable/disable window alignment
        uint32_t size;    ///< Time window size
    } m_window;   ///< Window alignment

    struct element {
        uint32_t pen;
        uint16_t id;
    };
    bool m_selection_used;
    std::vector<element> m_selection;

private:
    /// Default window size
    static const uint32_t WINDOW_SIZE = 300U;

    void
    set_default();
    void
    validate();

    void
    parse_root(fds_xml_ctx_t *ctx, const fds_iemgr_t *iemgr);
    void
    parse_dump(fds_xml_ctx_t *ctx);
    void
    parse_selection(fds_xml_ctx_t *ctx, const fds_iemgr_t *iemgr);
};


#endif // IPFIXCOL2_FDS_CONFIG_HPP

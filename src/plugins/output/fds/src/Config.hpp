/**
 * \file src/plugins/output/fds/src/Config.hpp
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Parser of XML configuration (header file)
 * \date 2019
 *
 * Copyright(c) 2019 CESNET z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef IPFIXCOL2_FDS_CONFIG_HPP
#define IPFIXCOL2_FDS_CONFIG_HPP

#include <string>
#include <libfds.h>

/**
 * @brief Plugin configuration parser
 */
class Config {
public:
    /**
     * @brief Parse configuration of the plugin
     * @param[in] params XML parameters to parse
     * @throw runtime_exception on error
     */
    Config(const char *params);
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

private:
    /// Default window size
    static const uint32_t WINDOW_SIZE = 300U;

    void
    set_default();
    void
    validate();

    void
    parse_root(fds_xml_ctx_t *ctx);
    void
    parse_dump(fds_xml_ctx_t *ctx);
};


#endif // IPFIXCOL2_FDS_CONFIG_HPP

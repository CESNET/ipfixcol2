/**
 * \file src/plugins/output/ipfix/src/config.cpp
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * \brief Config for ipfix output plugin (header file)
 * \date 2019
 */

/* Copyright (C) 2019 CESNET, z.s.p.o.
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

#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <string>

#include <ipfixcol2.h>
#include <libfds.h>

/// Plugin configuration
class Config {
private:
    /**
     * @brief Set default configuration parameters
     */
    void set_defaults();
    /**
     * @brief Parse XML tree
     * @param[in] params XML tree
     * @throws invalid_argument if unexpected node is detected
     */
    void parse_params(fds_xml_ctx_t *params);
    /**
     * @brief Check validity of configuration
     */
    void check_validity();

public:
    /// Output file pattern
    std::string filename;
    /// Use local time instead of UTC time
    bool use_localtime;
    /// Time interval to rotate files (in seconds)
    uint64_t window_size;
    /// Rotate files on multiple of time interval
    bool align_windows;
    /// Preserve original IPFIX Message (i.e. don't skip Data Sets with undefined templates)
    bool preserve_original;
    /// Split on IPFIX Export Time instead on system time
    bool split_on_export_time;

    /**
     * @brief Parse configuration of the IPFIX plugin
     * @param[in] params Plugin part of the configuration of the collector
     * @throws runtime_error on error or invalid configuration
     */
    Config(const char *params);
    /// Configuration destructor
    ~Config();
};

#endif // CONFIG_HPP
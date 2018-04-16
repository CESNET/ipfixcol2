/**
 * \file src/core/configurator/config_file.hpp
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Parser of configuration file (header file)
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

#ifndef IPFIXCOL_CONFIG_FILE_H
#define IPFIXCOL_CONFIG_FILE_H

#include <ipfixcol2/api.h>
#include "configurator.hpp"

/**
 * \brief Pass control to the file parser
 *
 * The function tries to load and parse a startup configuration from a file defined by \p path.
 * The parsed configuration is passed to the configurator \p conf and the pipeline is established.
 *
 * \note The function blocks until the collector is supposed to run.
 * \param[in] conf Collector configurator
 * \param[in] path Startup file
 * \return EXIT_SUCCESS or EXIT_FAILURE
 */
IPX_API int
ipx_config_file(ipx_configurator &conf, const std::string &path);

#endif //IPFIXCOL_CONFIG_FILE_H

//
// Created by lukashutak on 03/04/18.
//

#ifndef IPFIXCOL_CONFIG_FILE_H
#define IPFIXCOL_CONFIG_FILE_H

#include <ipfixcol2/api.h>
#include "configurator.hpp"

/**
 * \brief Pass control to the file parser
 * \param[in] conf Collector configurator
 * \param[in] path Startup file
 * \return EXIT_SUCCESS or EXIT_FAILURE
 */
IPX_API int
ipx_config_file(ipx_configurator &conf, const std::string &path);

#endif //IPFIXCOL_CONFIG_FILE_H

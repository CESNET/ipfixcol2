/**
 * \file configuration.c
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Configuration parser (header file)
 */
/* Copyright (C) 2016-2018 CESNET, z.s.p.o.
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
* This software is provided ``as is, and any express or implied
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
*/

#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include <stdint.h>
#include <stdbool.h>
#include <ipfixcol2.h>

/**
 * \brief Structure for configuration parsed from XML
 *
 * \note
 *   Files are generated based on following rules. LNF (and Index) files have
 *   always filename based on the template 'file_lnf.prefix'.'files.suffix'
 *   (and 'file_index.prefix'.'files.suffix').
 *   When profiles i.e. 'profiles.en' are disabled, all records are stored
 *   into the directory 'files.path'/YY/MM/DD/.  When the profiles are enabled,
 *   records for each channel are stored into the directory
 *   'profile_dir'/'channel_name'/YY/MM/DD/ of an appropriate profile
 *   where the channel belongs.
 */
struct conf_params {
    ipx_ctx_t *ctx;     /**< Context of the instance (only for log!)         */

    struct {
        char *path;     /**< Storage directory template.
                          *  This can be NULL only when profiles.en == true  */
        char *suffix;   /**< Common file suffix                              */
    } files;   /**< Common storage templates */

    struct {
        char *prefix;     /**< File prefix (can be NULL)                     */
        char *ident;      /**< Internal file identification (can be NULL)    */
        bool  compress;   /**< Enable/disable LZO compression                */
    } file_lnf; /**< LNF configuration */

    struct {
        bool  en;          /**< Enable/disable indexing. When disabled, other
                             *  parameters in this structure are undefined. */
        char *prefix;      /**< File prefix                                  */
        bool  autosize;    /**< Enable autosize                              */

        uint64_t est_cnt;  /**< Estimated item count in the filter           */
        double fp_prob;    /**< False positive probability of the filter     */
    } file_index;          /**< Bloom Filter Index configuration  */

    struct {
        bool     align;   /**< Enable/disable window alignment               */
        uint32_t size;    /**< Time window size                              */
    } window;   /**< Window alignment */

    struct {
        bool en;          /**< Enable/disable files generation based on
                            * profiles. When it is enabled, files.path is
                            * ignored                                        */
    } profiles; /**< Profiles configuration                                  */
};

/**
 * \brief Parse the plugin configuration
 *
 * \warning The configuration MUST be free by configuration_free() function.
 * \param[in] params XML configuration
 * \return On success returns a pointer to the configuration. Otherwise returns
 *   NULL.
 */
struct conf_params *
configuration_parse(ipx_ctx_t *ctx, const char *params);

/**
 * \brief Destroy the plugin configuration
 * \param[in,out] config Configuration
 */
void
configuration_free(struct conf_params *config);

#endif // CONFIGURATION_H

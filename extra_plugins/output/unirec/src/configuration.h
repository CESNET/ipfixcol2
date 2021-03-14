/**
 * \file configuration.h
 * \author Tomas Cejka <cejkat@cesnet.cz>
 * \author Jaroslav Hlavac <hlavaj20@fit.cvut.cz>
 * \brief Configuration parser (header file)
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

#include <ipfixcol2.h>

/**
 * \brief Structure for a configuration parsed from XML
 */
struct conf_params {
    /** Prepared TRAP interface specification string                                             */
    char *trap_ifc_spec;
    /**
     * TRAP interface UniRec template
     *
     * Elements marked with '?' are optional and might not be filled (e.g. TCP_FLAGS)
     * For example, "DST_IP,SRC_IP,BYTES,DST_PORT,?TCP_FLAGS,SRC_PORT,PROTOCOL".
     * All fields must be contained in unirec-elements.txt
     * \note All whitespaces have been removed
     */
    char *unirec_spec;
    /** The same as \ref conf_params.unirec_spec, however, question marks have been removed  */
    char *unirec_fmt;
    /** Split biflow record to 2 unidirectional flows                                        */
    bool biflow_split;
};

/**
 * \brief Parse the plugin configuration
 *
 * \warning The configuration MUST be free by configuration_free() function.
 * \param[in] ctx    Instance context
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
configuration_free(struct conf_params *cfg);

#endif // CONFIGURATION_H


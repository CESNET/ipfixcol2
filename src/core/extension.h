/**
 * \file src/core/extension.h
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Plugin instance extensions (internal header file)
 * \date 2020
 */

/* Copyright (C) 2020 CESNET, z.s.p.o.
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

#ifndef IPX_CTX_EXTENSION_H
#define IPX_CTX_EXTENSION_H

#include <stddef.h>
#include <ipfixcol2/plugins.h>

/// Type of extension record
enum ipx_extension {
    IPX_EXTENSION_PRODUCER,  ///< Extension producer
    IPX_EXTENSION_CONSUMER   ///< Extension consumer
};

/// Extension record of an instance context
struct ipx_ctx_ext {
    /// Extension type
    enum ipx_extension etype;
    /// Identification of extension type
    char *data_type;
    /// Identification of extension name
    char *data_name;
    /// Size of the extension
    size_t size;

    // -- Following fields are later filled by the configurator --
    /// Offset of the extension data (\ref ipx_ipfix_record.ext)
    size_t offset;
    /// Extension bitset mask (signalizes if the value is set)
    uint64_t mask;
};

/**
 * @brief Initialize internal extension record
 *
 * Check parameters and initialize all structure members
 * @param[in] ext       Structure to be initialized
 * @param[in] etype     Extension type (producer/consumer registration)
 * @param[in] data_type Extension data type
 * @param[in] data_name Extension name
 * @param[in] size      Producer: Size of the extension in bytes / Consumer: ignored
 * @return #IPX_OK on success
 * @return #IPX_ERR_NOMEM if a memory allocation failure has occurred
 * @return #IPX_ERR_ARG if parameters are not valid
 */
int
ipx_ctx_ext_init(struct ipx_ctx_ext *ext, enum ipx_extension etype, const char *data_type,
    const char *data_name, size_t size);

/**
 * @brief Destroy internal extension record
 *
 * Cleanup memory allocation for extension identificators
 * @param[in] ext Structure to destroy
 */
void
ipx_ctx_ext_destroy(struct ipx_ctx_ext *ext);


#endif // IPX_CTX_EXTENSION_H
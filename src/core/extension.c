/**
 * \file src/core/extension.c
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Plugin instance extensions (internal source file)
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

#include "extension.h"
#include <ipfixcol2/plugins.h>
#include <stdlib.h>
#include <string.h>

int
ipx_ctx_ext_get(ipx_ctx_ext_t *ext, struct ipx_ipfix_record *drec, void **data, size_t *size)
{
    if (ext->etype == IPX_EXTENSION_CONSUMER && (drec->ext_mask & ext->mask) == 0) {
        // The extension hasn't been filled by the producer
        return IPX_ERR_NOTFOUND;
    }

    *data = &drec->ext[ext->offset];
    *size = ext->size;
    return IPX_OK;
}

void
ipx_ctx_ext_set_filled(ipx_ctx_ext_t *ext, struct ipx_ipfix_record *drec)
{
    if (ext->etype != IPX_EXTENSION_PRODUCER) {
        return; // Not allowed!
    }

    drec->ext_mask |= ext->mask;
}

int
ipx_ctx_ext_init(struct ipx_ctx_ext *ext, enum ipx_extension etype, const char *data_type,
    const char *data_name, size_t size)
{
    // Check parameters
    if (!data_name || !data_type || strlen(data_type) == 0 || strlen(data_name) == 0) {
        return IPX_ERR_ARG;
    }
    if (etype != IPX_EXTENSION_CONSUMER && etype != IPX_EXTENSION_PRODUCER) {
        return IPX_ERR_ARG;
    }
    if (etype == IPX_EXTENSION_PRODUCER && size == 0) {
        return IPX_ERR_ARG;
    }

    ext->etype = etype;
    ext->size = (etype == IPX_EXTENSION_PRODUCER) ? size : 0;
    ext->data_type = strdup(data_type);
    ext->data_name = strdup(data_name);
    if (!ext->data_type || !ext->data_name) {
        free(ext->data_type);
        free(ext->data_name);
        return IPX_ERR_NOMEM;
    }

    ext->offset = 0;
    ext->mask = 0;
    return IPX_OK;
}

void
ipx_ctx_ext_destroy(struct ipx_ctx_ext *ext)
{
    free(ext->data_name);
    free(ext->data_type);
}


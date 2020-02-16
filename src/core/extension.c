
#include "extension.h"
#include <ipfixcol2/plugins.h>
#include <stdlib.h>
#include <string.h>

int
ipx_ctx_ext_size(ipx_ctx_ext_t *ext, size_t *size)
{
    if (!ext->mask) {
        // Mask is not filled -> initialization hasn't been completed yet
        return IPX_ERR_DENIED;
    }

    *size = ext->size;
    return IPX_OK;
}

void *
ipx_ctx_ext_get(ipx_ctx_ext_t *ext, struct ipx_ipfix_record *ipx_drec)
{
    if (ext->etype == IPX_EXTENSION_CONSUMER) {
        // TODO: check if the memory has been filled (only for consumers)
        return NULL;
    }

    return (ipx_drec->ext + ext->offset);
}

void
ipx_ctx_ext_set_filled(ipx_ctx_ext_t *ext, struct ipx_ipfix_record *ipx_drec)
{
    if (ext->etype != IPX_EXTENSION_PRODUCER) {
        return; // Not allowed!
    }

    // TODO: label the extension as filled!
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


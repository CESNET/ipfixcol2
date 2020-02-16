
#ifndef IPX_CTX_EXTENSION_H
#define IPX_CTX_EXTENSION_H

#include <stddef.h>
#include <ipfixcol2/plugins.h>

/// Type of extension record
enum ipx_extension {
    IPX_EXTENSION_PRODUCER,  ///< Extension producer
    IPX_EXTENSION_CONSUMER   ///< Extension consumer
};

/// Extension record
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
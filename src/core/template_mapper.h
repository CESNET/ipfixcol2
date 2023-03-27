/**
 * \file template_mapper.h
 * \author Adam Zvara <xzvara01@stud.fit.vutbr.cz>
 * \brief Template mapper (internal header file)
 * \date 2023
 */

#ifndef IPX_TEMPLATE_MAPPER_INTERNAL_H
#define IPX_TEMPLATE_MAPPER_INTERNAL_H

#include <ipfixcol2.h>
#include <inttypes.h>

/** External definitions of items in mapper         */
typedef struct template_mapper ipx_template_mapper_t;

/**
 * \brief Create new instance of template mapper
 *
 * \return Pointer to allocated mapper or NULL on memory allocation error
 */
IPX_API struct template_mapper *
ipx_mapper_create();

/**
 * \brief Destroy template mapper
 *
 * \param[in] map Pointer to template mapper
 */
IPX_API void
ipx_mapper_destroy(struct template_mapper *map);

/**
 * \brief Add new mapping to template mapper
 *
 * \param[in] map            Pointer to template mapper
 * \param[in] original_id    Original template ID
 * \param[in] modified_tmplt Pointer to modified template
 * \return #IPX_OK on success, #IPX_ERR_NOMEM on memory allocation error
 */
IPX_API int
ipx_mapper_add(ipx_template_mapper_t *map, const struct fds_template *modified_tmplt, uint16_t original_id);

/**
 * \brief Look for modified template in template mapper
 *
 * \param[in] map         Pointer to template mapper
 * \param[in] original_id Template ID
 * \param[in] tmplt       Template to look for
 * \return Pointer to mapping or NULL if not found
 */
IPX_API const struct fds_template *
ipx_mapper_lookup(ipx_template_mapper_t *map, const struct fds_template *tmplt, uint16_t original_id);

#endif
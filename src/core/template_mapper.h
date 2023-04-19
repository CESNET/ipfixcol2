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

#define APPENDED_FIELDS_DEF_CNT 32

/** External definitions of items in mapper                 */
typedef struct template_mapper ipx_template_mapper_t;

struct modified_tmplt_id {
    /** Original template info                              */
    uint8_t *data;     /**< Original template data          */
    uint16_t length;   /**< Original template size          */

    /** Array of new fields to append to original template  */
    int8_t appended_fields[APPENDED_FIELDS_DEF_CNT];
};

/**
 * \brief Create new instance of template mapper
 *
 * \return Pointer to allocated mapper or NULL on memory allocation error
 */
IPX_API ipx_template_mapper_t *
ipx_mapper_create();

/**
 * \brief Clear template mapper
 *
 * \param[in] map Pointer to template mapper
 */
IPX_API void
ipx_mapper_clear(ipx_template_mapper_t *map);

/**
 * \brief Get number of templates in template mapper
 *
 * \param[in] map Pointer to template mapper
 * \return Number of templates in template mapper
 */
IPX_API size_t
ipx_mapper_get_tmplt_count(ipx_template_mapper_t *map);

/**
 * \brief Destroy template mapper
 *
 * \param[in] map Pointer to template mapper
 */
IPX_API void
ipx_mapper_destroy(ipx_template_mapper_t *map);

/**
 * \brief Add new mapping to template mapper
 *
 * \param[in] map            Pointer to template mapper
 * \param[in] original_id    Original template ID
 * \param[in] modified_tmplt Pointer to modified template
 * \return #IPX_OK on success, #IPX_ERR_NOMEM on memory allocation error
 */
IPX_API int
ipx_mapper_add(ipx_template_mapper_t *map, const struct fds_template *modified_tmplt, struct modified_tmplt_id *item, uint16_t original_id);

/**
 * \brief Look for modified template in template mapper
 *
 * \param[in] map         Pointer to template mapper
 * \param[in] original_id Template ID
 * \param[in] tmplt       Template to look for
 * \return Pointer to mapping or NULL if not found
 */
IPX_API const struct fds_template *
ipx_mapper_lookup(ipx_template_mapper_t *map, const struct modified_tmplt_id *item, uint16_t original_id);

#endif
/**
 * \file   src/templater/tmpl_algorithms.h
 * \author Michal Režňák
 * \brief  Basic vectors for templater
 * \date   8/23/17
 */

/* Copyright (C) 2016 CESNET, z.s.p.o.
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

#ifndef TEMPLATER_VECTORS_H
#define TEMPLATER_VECTORS_H

#include <stdlib.h>
#include <stdint.h>
#include <ipfixcol2/templater.h>

/** Fields with parsed templates                                                          */
struct vectm_fields {
    struct ipx_tmpl_template *templates; /**< Field of parsed templates                   */
    uint64_t                  die_time;  /**< Time after which template shouldn't be used */
};

/** Vector of templates                                                           */
typedef struct vec_tm_s {
    uint64_t             global_die_time; /**< Smallest die_time of the templates */
    size_t               end;             /**< Index one after the last element   */
    size_t               used;            /**< Number of used fields of elements  */
    struct vectm_fields *fields;          /**< Field of vector elements           */
} vectm_t;

/**
 * \brief Create template vector
 * \return Template vector
 */
vectm_t *
vectm_create();

/**
 * \brief Number of templates in a vector
 * \param[in] vec Template vector
 * \return Count
 */
size_t
vectm_get_count(const vectm_t *vec);

/**
 * \brief Deep copy of the vector
 * \param[in] vec Vector of templates
 * \return Copied vector
 */
vectm_t *
vectm_copy(vectm_t *vec);

/**
 * \brief Find template on a index
 * \param[in] vec   Template vector
 * \param[in] index Index of the template
 * \return Template
 *
 * \warning Function doesn't control array boundaries
 */
ipx_tmpl_template_t *
vectm_get_template(const vectm_t* vec, size_t index);

/**
 * \brief Set template on the index to the different value.
 * \param[in]     tmpl  Templater
 * \param[in,out] vec   Vector of templates
 * \param[in]     index Index of the template
 * \param[in]     src   Source template
 *
 * \warning Previous template is NOT destroyed
 * \warning Function doesn't control array boundaries
 */
void
vectm_set_index(ipx_tmpl_t *tmpl, vectm_t *vec, size_t index, ipx_tmpl_template_t *src);

/**
 * \brief Set die time of the field
 * \param[in,out] vec   Vector of templates
 * \param[in]     index Index of the field
 * \param[in]     time  Die time
 */
void
vectm_set_die_time(vectm_t *vec, size_t index, uint64_t time);

/**
 * \brief Get die_time of the template
 * \param vec   Vector of templates
 * \param index Index of the template in a vector
 * \return die_time
 *
 * \note Because of performance, function doesn't check boundaries
 */
uint64_t
vectm_get_die_time(vectm_t *vec, size_t index);

/**
 * \brief Get global_die_time of the vector
 * \param vec Vector of templates
 * \return Global die time
 */
uint64_t
vectm_get_global_die_time(vectm_t *vec);

/**
 * \brief Find index of the template with defined ID
 * \param[in] vec Template vector
 * \param[in] id  ID of the template
 * \return Index of the template on success, otherwise -1
 */
ssize_t
vectm_find_index(const vectm_t *vec, uint16_t id);

/**
 * \brief Find template with defined ID
 * \param[in] vec Template vector
 * \param[in] id  ID of teh template
 * \return Template if founded, otherwise NULL
 *
 * \note This use binary find, thus vector must be sorted
 */
struct ipx_tmpl_template *
vectm_find(const vectm_t *vec, uint16_t id);

//*
// * \brief Create new empty template in a vector
// * \param[in] vec   Template vector
// * \param[in] count Number of fields in a template
// * \return Template in a vector
// */
//ipx_tmpl_template_t *
//vectm_make(ipx_tmpl_t *tmpl, vectm_t *vec, size_t count);

/**
 * \brief Add template to the end of the vector
 * \param[in] tmpl Templater
 * \param[in] vec  Vector of templates
 * \param[in] res  Template
 */
void
vectm_add(ipx_tmpl_t *tmpl, vectm_t *vec, ipx_tmpl_template_t *res);

/**
 * \brief Sort vector
 * \param vec Template vector
 */
void
vectm_sort(vectm_t *vec);

/**
 * \brief Remove all templates from a vector
 * \param vec Template vector
 */
void
vectm_clear(vectm_t *vec);

/**
 * \brief Remove vector
 * \param vec Template vector
 */
void
vectm_destroy(vectm_t *vec);

// GARBAGE
/** Garbage */
typedef struct garbage garbage_t;

/**
 * \brief Create garbage
 * \param[in] tmpl Templater
 * \return Garbage on success, otherwise NULL
 */
garbage_t *
tmpl_garbage_create(ipx_tmpl_t *tmpl);

/**
 * \brief Add template to the end of the vector of templates
 * \param[in,out] gar Garbage
 * \param[in]     tmp Template
 * \return True on success, otherwise False
 *
 * \note Function doesn't copy template
 */
bool
tmpl_garbage_template_add(garbage_t *gar, ipx_tmpl_template_t *tmp);

/**
 * \brief Add index of the template to the end of the vector of indexes
 * \param[in,out] gar   Garbage
 * \param[in]     index Index
 * \return True on success, otherwise False
 */
bool
tmpl_garbage_template_index_add(garbage_t *gar, uint16_t index);

/**
 * \brief Add snapshot to the end of the vector of snapshots
 * \param[in,out] gar Garbage
 * \param[in] tmp Snapshot
 * \return True on success, otherwise False
 *
 * \note Function doesn't copy snapshot
 */
void
tmpl_garbage_snapshot_add(garbage_t *gar, ipx_tmpl_t *tmp);

/**
 * \brief Destroy garbage with all context
 * \param gar Garbage
 */
void
tmpl_garbage_destroy(garbage_t *gar);

//// BINARY TREE
//typedef struct node node_t;
//
///**
// * \brief Search binary tree.
// * \param[in,out] leaf Binary tree
// * \param[in]     key  Key value
// * \return Founded node on success, otherwise NULL and key is added
// */
//const node_t *
//tmpl_tree_get(node_t **leaf, ipx_tmpl_template_t *key);
//
///**
// * \brief Destroy binary tree
// * \param[in] leaf Binary tree
// */
//void
//tmpl_tree_destroy(node_t *leaf);

#endif // TEMPLATER_VECTORS_H

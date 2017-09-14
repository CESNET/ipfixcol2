/**
 * \file   src/templater/tmpl_template.h
 * \author Michal Režňák
 * \date   8/28/17
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

#ifndef TEMPLATE_H
#define TEMPLATE_H

#include "tmpl_common.h"
#include <ipfixcol2/templater.h>

/**
 * \brief Create a template with defined number of elements
 * \param count Number of elements
 * \return Template on success, otherwise NULL
 */
ipx_tmpl_template_t *
template_create(size_t count);

/**
 * \brief Create copy of the template and set end time to current time
 * \param[in] tmpl Templater
 * \param[in] src  Source template
 * \return Copied template on success, otherwise NULL
 */
ipx_tmpl_template_t *
template_copy_end(ipx_tmpl_t *tmpl, ipx_tmpl_template_t *src);

/**
 * \brief Convert template record and save to the templater
 * \param[in,out] tmpl    Templater
 * \param[in]     rec     Template record
 * \param[in]     max_len Maximal length of the template
 * \return Real length of the record on success, otherwise IPX_ERR or IPX_NOMEM
 */
int
template_add(ipx_tmpl_t *tmpl, const struct ipfix_template_record *rec, uint16_t max_len);

/**
 * \brief Convert record to the template
 * \param[in,out] tmpl    Templater
 * \param[in]     rec     Record
 * \param[in]     max_len Maximal length of the record
 * \param[out]    res     Converted template
 * \return Length of the record on success, otherwise IPX_ERR or IPX_NOMEM
 *
 * \note Record is copied
 */
int
template_convert(ipx_tmpl_t *tmpl, const struct ipfix_template_record *rec, uint16_t max_len,
                 struct ipx_tmpl_template *res);

/**
 * \brief Replace all previous definitions of elements in all templates with new definitions
 * from a new iemgr
 * \param[in,out] tmpl Templater
 * \param[in]     mgr  Iemgr
 */
int
templates_reset_iemgr(ipx_tmpl_t *tmpl, fds_iemgr_t *mgr);

/**
 * \brief Change all definitions of elements to NULL in all templates
 * \param[in,out] tmpl Templater
 */
int
templates_null_iemgr(ipx_tmpl_t* tmpl);

/**
 * \brief If can (depends on templater's flags), create new template and save to the templater
 * \param[in,out] tmpl     Templater
 * \param[in]     template Previous template
 * \param[in]     rec      Template record that will be parsed
 * \param[in]     max_len  Maximal length of the template record
 * \param[in]     index    Index on which previous template was in templater's field of templates
 * \return IPX_OK on success, otherwise IPX_ERR or IPX_NOMEM
 *
 * \note New template will points to the previous
 */
int
template_overwrite(ipx_tmpl_t *tmpl, struct ipx_tmpl_template *template,
                   const struct ipfix_template_record *rec, uint16_t max_len, ssize_t index);

/**
 * \brief Save properties of the template from a record
 * \param[in]  tmpl Templater
 * \param[out] res  Template
 * \param[in]  rec  Record
 * \param[in]  len  Length of the raw data in a record
 * \return True on success, otherwise False
 */
bool
template_save_properties(ipx_tmpl_t* tmpl, struct ipx_tmpl_template* res,
                         const struct ipfix_template_record* rec, uint16_t len);
/**
 * \brief Save properties of the options template from a record
 * \param[in]  tmpl Templater
 * \param[out] res  Options template
 * \param[in]  rec  Record
 * \param[in]  len  Length of the raw data in a record
 * \return True on success, otherwise False
 */
void
opts_template_save_properties(ipx_tmpl_t *tmpl, ipx_tmpl_template_t *res,
                              const struct ipfix_options_template_record *rec, uint16_t len);

/**
 * \brief Add options template to the templater
 * \param[in,out] tmpl    Templater
 * \param[in]     rec     Options template record
 * \param[in]     max_len Maximal length of the options template record
 * \return Real length of the template record on success, otherwise IPX_ERR or IPX_NOMEM
 */
int
opts_template_add(ipx_tmpl_t *tmpl, const struct ipfix_options_template_record *rec, uint16_t max_len);

/**
 * \brief If can (depends on templater's flags), create new options template and save to the templater
 * \param[in,out] tmpl     Templater
 * \param[in]     template Previous options template
 * \param[in]     rec      Template record that will be parsed
 * \param[in]     max_len  Maximal length of the template record
 * \param[in]     index    Index on which previous options template was in templater's field of templates
 * \return IPX_OK on success, otherwise IPX_ERR or IPX_NOMEM
 *
 * \note New options template will points to the previous
 */
int
opts_template_overwrite(ipx_tmpl_t *tmpl, ipx_tmpl_template_t *template,
                        const struct ipfix_options_template_record *rec, uint16_t max_len, ssize_t index);

/**
 * \brief Convert record to the options template
 * \param[in,out] tmpl    Templater
 * \param[in]     rec     Record
 * \param[in]     max_len Maximal length of the record
 * \param[out]    res     Converted options template
 * \return Length of the record on success, otherwise IPX_ERR or IPX_NOMEM
 *
 * \note Record is copied
 */
int
opts_template_convert(ipx_tmpl_t *tmpl, const struct ipfix_options_template_record *rec,
                      uint16_t max_len, ipx_tmpl_template_t *res);

/**
 * \brief Go throw all options templates in a record and parse them to the templater
 * \param[in,out] tmpl    Templater
 * \param[in]     recs    Options template records
 * \param[in]     max_len Maximal length of the records together
 * \return IPX_OK if parsed successfully, otherwise IPX_ERR or IPX_NOMEM
 */
int
opts_templates_parse(ipx_tmpl_t* tmpl, struct ipfix_options_template_record* recs,
                     uint16_t max_len);

/**
 * \brief Parse template from a template record to the templater
 * \param[in,out] tmpl    Templater
 * \param[in]     rec     Template record
 * \param[in]     max_len Maximal length
 * \return Real length of the record on success, otherwise IPX_ERR or IPX_NOMEM
 *
 * \note Templater is not sorted
 */
int
template_parse(ipx_tmpl_t *tmpl, const struct ipfix_template_record *rec, uint16_t max_len);

/**
 * \brief Go throw all templates in a record and parse them to the templater
 * \param[in,out] tmpl    Templater
 * \param[in]     recs    Template records
 * \param[in]     max_len Maximal length of the records together with template head size
 * \return IPX_OK if parsed successfully, otherwise IPX_ERR or IPX_NOMEM
 */
int
templates_parse(ipx_tmpl_t* tmpl, struct ipfix_template_record* recs, uint16_t max_len);

/**
 * \brief Find template with 'current time'
 *
 * Find template with defined ID and then go throw all previous template
 * to find one with 'current time'
 * \param[in] tmpl Templater
 * \param[in] id   ID of the template
 * \return Template if founded, otherwise NULL
 * \note All snapshots have modified flag set to false so they will never create another snapshot
 */
ipx_tmpl_template_t *
template_find_with_time(const ipx_tmpl_t* tmpl, uint16_t id);

/**
 * \brief Create new template with same definitions as 'template record' and save to the templater
 * \param[in] tmpl    Templater
 * \param[in] next    Next template
 * \param[in] rec     Template record
 * \param[in] max_len Maximal length
 * \param[in] index   Index of the template in a templater
 * \return Real length when template is saved, otherwise IPX_ERR or IPX_NOMEM
 *
 * \note Template next will be used as a 'next' pointer in a new template.
 */
int
template_replace_index(ipx_tmpl_t *tmpl, ipx_tmpl_template_t *next,
                       const struct ipfix_template_record *rec, uint16_t max_len, ssize_t index);

/**
 * \brief Check if templates are identical
 * \param[in] template Template
 * \param[in] rec      Template record
 * \param[in] max_len  Maximal length
 * \return
 */
int
templates_identical(const ipx_tmpl_template_t *template, const struct ipfix_template_record *rec,
                    uint16_t max_len);

/**
 * \brief Move template withe defined ID to the garbage
 * \param[in,out] tmpl  Templater
 * \param[in]     index Index of the template in a vector of templates
 * \return IPX_OK when removed, otherwise IPX_ERR or IPX_NOMEM
 *
 * \warning Function doesn't check array boundaries
 */
int
template_remove(ipx_tmpl_t *tmpl, size_t index);

/**
 * \brief Remove template
 * \param[in] src Template
 */
void
template_destroy(ipx_tmpl_template_t *src);

/**
 * \brief Remove all templates except \p src template
 * \param[in] src Template
 */
void
template_remove_previous(ipx_tmpl_template_t* src);

/**
 * \brief Remove all templates with previous definitions of the same ID
 * \param[in] src Template
 */
void
template_remove_all(ipx_tmpl_template_t* src);

/**
 * \brief Remove all snapshots
 * \param[in] snap Snapshot
 */
void
snapshots_remove(const ipx_tmpl_t *snap);

/**
 * \brief Move all templates with defined ID to the garbage
 * \param[in,out] tmpl  Templater
 * \param[in]     index Index of the template in a vector
 * \return IPX_OK when removed, otherwise IPX_ERR or IPX_NOMEM
 *
 * \note Move all templates, those with older time also too
 * \note Set die time for template to current time, so on the next get_garbage all templates with
 * defined ID will be removed
 * \warning Function doesn't check array boundaries
 */
int
template_remove_all_with_id(ipx_tmpl_t *tmpl, size_t index);

/**
 * \brief Create new snapshot from a templater
 * \param[in,out] tmpl Templater
 *
 * \note Templater's 'current time' is time when snapshot was first used, template_get was call.
 */
void
snapshot_create(ipx_tmpl_t* tmpl);

/**
 * \brief Find first template in a link which has end time smaller than \p time
 * \param[in] src  First template of a link
 * \param[in] time Time
 * \return Founded template if exists, otherwise NULL
 */
ipx_tmpl_t *
tmpl_time_find(const ipx_tmpl_t* src, uint64_t time);

/**
 * \brief Create copy of the template
 * \param[in] tmpl Templater
 * \return Copied templater on success, otherwise NULL
 *
 * \note Fields in templater's pointers to templates are not copied
 */
const ipx_tmpl_t *
tmpl_copy(ipx_tmpl_t *tmpl);

#endif //TEMPLATE_H

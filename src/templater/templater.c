/**
 * \file //TODO
 * \author Michal Režňák
 * \brief  Implementation of the template manager
 * \date   8/21/17
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

#include <stdlib.h>
#include <string.h>
#include <ipfixcol2/templater.h>
#include <ipfixcol2.h>
#include "tmpl_common.h"
#include "tmpl_template.h"

ipx_tmpl_t *
ipx_tmpl_create(uint64_t life_time, uint64_t life_packet, enum IPX_SESSION_TYPE type)
{
    ipx_tmpl_t *tmpl = malloc(sizeof(ipx_tmpl_t));
    if (tmpl == NULL) {
        return NULL;
    }

    tmpl->iemgr         = NULL;
    tmpl->snapshot      = NULL;
    tmpl->life.count    = life_packet;
    tmpl->life.time     = life_time;
    tmpl->templates     = vectm_create();
    tmpl->type          = type;
    tmpl->flag.modified = true;
    tmpl->current.time  = 0;
    tmpl->current.count = 0;

    if (tmpl->type == IPX_SESSION_TYPE_UDP) {
        tmpl->flag.can_overwrite    = true;
        tmpl->flag.can_truly_remove = true;
        tmpl->flag.care_count       = true;
        tmpl->flag.care_time        = true;
    }
    else if (tmpl->type == IPX_SESSION_TYPE_SCTP) {
        tmpl->flag.can_overwrite    = false;
        tmpl->flag.can_truly_remove = false;
        tmpl->flag.care_count       = false;
        tmpl->flag.care_time        = false;
    }
    else if (tmpl->type == IPX_SESSION_TYPE_TCP) {
        tmpl->flag.can_overwrite    = false;
        tmpl->flag.can_truly_remove = false;
        tmpl->flag.care_count       = false;
        tmpl->flag.care_time        = false;
    } else {
        ipx_tmpl_destroy(tmpl);
        return NULL;
    }

    return tmpl;
}

int
ipx_tmpl_iemgr_load(ipx_tmpl_t *tmpl, fds_iemgr_t *mgr)
{
    assert(tmpl != NULL);

    tmpl->iemgr = mgr;
    tmpl->flag.modified = true;
    if (mgr == NULL) {
        return templates_null_iemgr(tmpl);
    }
    return templates_reset_iemgr(tmpl, mgr);
}

void
tmpl_destroy(ipx_tmpl_t *tmpl, node_t **deleted)
{
    if (tmpl->snapshot != NULL) {
        tmpl_destroy((ipx_tmpl_t*) tmpl->snapshot, deleted);
    }

    ipx_tmpl_template_t *tmp;
    for (size_t i = 0; i < vectm_get_count(tmpl->templates); ++i) {
        tmp = vectm_get_template(tmpl->templates, i);
        if (tmpl_tree_get(deleted, tmp) == NULL) {
            template_destroy(tmp);
        }
    }

    vectm_destroy(tmpl->templates);
    free(tmpl);
}

void
ipx_tmpl_destroy(ipx_tmpl_t *tmpl)
{
    assert(tmpl != NULL);

    node_t* root = NULL;
    tmpl_destroy(tmpl, &root);
    tmpl_tree_destroy(root);
}

void
ipx_tmpl_set(ipx_tmpl_t *tmpl, uint64_t current_time, uint64_t current_packet)
{
    assert(tmpl != NULL);

    tmpl->current.time = current_time;
    tmpl->current.count = current_packet;
}

int
template_remove_at_index(ipx_tmpl_t *tmpl, size_t index)
{
    tmpl->flag.modified = true;
    if (tmpl->flag.can_truly_remove) {
        return template_remove_all_with_id(tmpl, (size_t) index);
    }
    return template_remove(tmpl, (size_t) index);
}

int
ipx_tmpl_clear(ipx_tmpl_t *tmpl)
{
    assert(tmpl != NULL);

    int res;
    for (size_t i = 0; i < vectm_get_count(tmpl->templates); ++i) {
        res = template_remove_at_index(tmpl, i);
        if (res != IPX_OK) {
            return res;
        }
    }
    return IPX_OK;
}

int
ipx_tmpl_template_remove(ipx_tmpl_t *tmpl, uint16_t id)
{
    assert(tmpl != NULL);

    ssize_t index = vectm_find_index(tmpl->templates, id);
    if (index < 0) {
        return IPX_NOT_FOUND;
    }
    return template_remove_at_index(tmpl, (size_t) index);
}

int
ipx_tmpl_template_set_parse(ipx_tmpl_t *tmpl, struct ipfix_set_header *head)
{
    assert(tmpl != NULL);
    assert(head != NULL);
    uint64_t id;
    uint64_t len;
    ipx_get_uint_be(&head->flowset_id, INT16_SIZE, &id);
    ipx_get_uint_be(&head->length, INT16_SIZE, &len);

    tmpl->flag.modified = true;
    if (id == IPFIX_SET_TEMPLATE) {
        return templates_parse(tmpl, (struct ipfix_template_record*) ++head, (uint16_t) len);
    }

    if (id == IPFIX_SET_OPTIONS_TEMPLATE) {
        return ops_templates_parse(tmpl, (struct ipfix_options_template_record*)
                ++head, (uint16_t) len);
    }

    return IPX_ERR;
}

int
ipx_tmpl_template_parse(ipx_tmpl_t *tmpl, const struct ipfix_template_record *rec, uint16_t max_len)
{
    assert(tmpl != NULL);
    assert(rec != NULL);
    if (max_len < TEMPL_HEAD_SIZE) {
        return IPX_ERR;
    }
    if (tmpl->snapshot != NULL
        && (tmpl->current.time < tmpl->snapshot->current.time)) {
        return IPX_ERR;
    }

    const int res = template_parse(tmpl, rec, max_len);
    vectm_sort(tmpl->templates);
    tmpl->flag.modified = true;
    return res;
}

int
ipx_tmpl_options_template_parse(ipx_tmpl_t *tmpl, const struct ipfix_options_template_record *rec, uint16_t max_len)
{
    assert(tmpl != NULL);
    assert(rec != NULL);

    if (rec->count == 0) {
        ipx_tmpl_template_remove(tmpl, rec->template_id);
        return OPTS_TEMPL_HEAD_SIZE;
    }

    const int res = opts_template_add(tmpl, rec, max_len);
    vectm_sort(tmpl->templates);
    tmpl->flag.modified = true;
    return res;
}

int
ipx_tmpl_template_get(const ipx_tmpl_t *tmpl, uint16_t id, ipx_tmpl_template_t **template)
{
    assert(tmpl != NULL);
    assert(template != NULL);

    ipx_tmpl_template_t *res = template_find_with_time(tmpl, id);
    if (res == NULL) {
        return IPX_NOT_FOUND;
    }
    *template = res;

    if (tmpl->flag.care_count
        && (tmpl->current.count - res->number_packet >= tmpl->life.count)) {
        return IPX_OK_OLD;
    }
    return IPX_OK;
}

const ipx_tmpl_t *
ipx_tmpl_snapshot_get(ipx_tmpl_t *tmpl)
{
    assert(tmpl != NULL);

    const ipx_tmpl_t *res = tmpl->snapshot;
    while (res != NULL) {
        if (res->current.time <= tmpl->current.time) {
            return res;
        }
        res = res->snapshot;
    }
    return NULL;
}

/**
 * \brief Find first before the snapshot that has 'current time' plus 'lif time' smaller
 * than templater's 'current time'.
 * \param[in] tmpl Templater
 * \return Snapshot if exist, otherwise NULL
 */
ipx_tmpl_t *
snapshot_before_die_time(const ipx_tmpl_t* tmpl)
{
    const ipx_tmpl_t *res = tmpl;
    const ipx_tmpl_t *prev = tmpl;
    while (res != NULL) {
        if (res->current.time + tmpl->life.time < tmpl->current.time) {
            return (ipx_tmpl_t*) prev;
        }
        prev = res;
        res = res->snapshot;
    }
    return NULL;
}

/**
 * \brief Add template to the garbage, if it's first template in a row,
 * add index of the template instead
 * \param[in,out] gar   Garbage
 * \param[in]     tmpl  Templater
 * \param[in]     index Index of the template
 * \return True if added, otherwise False
 */
bool
garbage_add(garbage_t *gar, const ipx_tmpl_t *tmpl, uint16_t index)
{
    ipx_tmpl_template_t *res = vectm_get_template(tmpl->templates, index);
    if (res->time.end + tmpl->life.time < tmpl->current.time && res->time.end != 0) {
        return tmpl_garbage_template_index_add(gar, index);
    }

    ipx_tmpl_template_t *prev = res;
    res = res->next;
    while (res != NULL) {
        if (res->time.end + tmpl->life.time < tmpl->current.time && res->time.end != 0) {
            return tmpl_garbage_template_add(gar, prev);
        }
        prev = res;
        res = res->next;
    }
    return true;
}

/**
 * \brief Get all garbage from a templater
 * \param[in] tmpl Templater
 * \return Garbage on success, otherwise NULL
 *
 * \note When die_time is 0, than template is automatically added to the garbage
 */
struct garbage *
garbage_get(const ipx_tmpl_t *tmpl)
{
    if (vectm_get_global_die_time(tmpl->templates) > tmpl->current.time) {
        return NULL;
    }

    uint64_t die_time = 0;
    garbage_t *gar = tmpl_garbage_create((ipx_tmpl_t*) tmpl);
    for (uint16_t i = 0; i < vectm_get_count(tmpl->templates); ++i) {
        die_time = vectm_get_die_time(tmpl->templates, i);
        if (die_time > tmpl->current.time || die_time == 0) {
            continue;
        }

        if (!garbage_add(gar, tmpl, i)) {
            return NULL;
        }
    }

    tmpl_garbage_snapshot_add(gar, snapshot_before_die_time(tmpl));
    return gar;
}

ipx_garbage_msg_t *
ipx_tmpl_garbage_get(ipx_tmpl_t *tmpl)
{
    assert(tmpl != NULL);

    garbage_t *gar = garbage_get(tmpl);
    if (gar == NULL) {
        return NULL;
    }

//    return ipx_garbage_msg_create(garbage_get(tmpl), (ipx_garbage_msg_cb) tmpl_garbage_destroy);
    tmpl_garbage_destroy(gar);
    return NULL;
}

const struct ipx_tmpl_template_field*
ipx_tmpl_template_field_get(ipx_tmpl_template_t* template, size_t index)
{
    if (index > template->fields_cnt_total) {
        return NULL;
    }

    return &template->fields[index];
}

enum IPX_TEMPLATE_TYPE
ipx_tmpl_template_type_get(const ipx_tmpl_template_t* template)
{
    return template->template_type;
}

enum IPX_OPTS_TEMPLATE_TYPE
ipx_tmpl_template_opts_type_get(const ipx_tmpl_template_t* template)
{
    return template->options_type;
}

uint16_t
ipx_tmpl_template_id_get(const ipx_tmpl_template_t* template)
{
    return template->id;
}

/**
 * @file src/core/netflow2ipfix/netflow9_templates.c
 * @author Lukas Hutak <lukas.hutak@cesnet.cz>
 * @brief Auxiliary template management for NetFlow v9 to IPFIX conversion (source file)
 * @date 2018-2019
 *
 * Copyright(c) 2019 CESNET z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stddef.h> // offsetof
#include <stdlib.h>

#include <ipfixcol2.h>
#include "netflow9_templates.h"

/// Default number of pre-allocated instructions
#define NF9_TREC_DEF_INSTR 8U


void
nf9_tmplts_init(struct tmplts_l1_table *tbl)
{
    // Set all pointers to NULL
    memset(tbl, 0, sizeof(*tbl));
}

void
nf9_tmplts_destroy(struct tmplts_l1_table *tbl)
{
    // Destroy templates and lookup tables
    for (size_t l1_idx = 0; l1_idx < TMPLTS_TABLE_SIZE; ++l1_idx) {
        struct tmplts_l2_table *l2_table = tbl->l2_tables[l1_idx];
        if (!l2_table) {
            continue;
        }

        for (size_t l2_idx = 0; l2_idx < TMPLTS_TABLE_SIZE; ++l2_idx) {
            struct nf9_trec *rec = l2_table->recs[l2_idx];
            if (!rec) {
                continue;
            }

            // Destroy the record
            nf9_trec_destroy(rec);
        }

        // Destroy the L2 table
        free(l2_table);
    }
}

struct nf9_trec *
nf9_tmplts_find(struct tmplts_l1_table *tbl, uint16_t id)
{
    const uint16_t l1_idx = id / TMPLTS_TABLE_SIZE;
    struct tmplts_l2_table *l2_table = tbl->l2_tables[l1_idx];
    if (!l2_table) {
        return NULL;
    }

    const uint16_t l2_idx = id % TMPLTS_TABLE_SIZE;
    struct nf9_trec *rec = l2_table->recs[l2_idx];
    return rec;
}

int
nf9_tmplts_insert(struct tmplts_l1_table *tbl, uint16_t id, struct nf9_trec *rec)
{
    const uint16_t l1_idx = id / TMPLTS_TABLE_SIZE;
    struct tmplts_l2_table *l2_table = tbl->l2_tables[l1_idx];
    if (!l2_table) {
        // Create a new L2 table
        l2_table = calloc(1, sizeof(*l2_table));
        if (!l2_table) {
            return IPX_ERR_NOMEM;
        }

        tbl->l2_tables[l1_idx] = l2_table;
    }

    // Remove the old record, if exists
    const uint16_t l2_idx = id % TMPLTS_TABLE_SIZE;
    struct nf9_trec *old_rec = l2_table->recs[l2_idx];
    if (old_rec != NULL && old_rec != rec) {
        nf9_trec_destroy(old_rec);
    }

    l2_table->recs[l2_idx] = rec;
    return IPX_OK;
}

/**
 * @brief Calculate size of Template structure with user defined number of instructions
 * @param[in] instr_cnt Instruction count
 * @return Size of the Template structure
 */
static inline size_t
nf9_trec_size(size_t instr_cnt)
{
    return offsetof(struct nf9_trec, instr_data) + (instr_cnt * sizeof(struct nf2ipx_instr));
}

struct nf9_trec *
nf9_trec_new(size_t nf_size, size_t ipx_size)
{
    struct nf9_trec *res = calloc(1, nf9_trec_size(NF9_TREC_DEF_INSTR));
    if (!res) {
        // Memory allocation error
        return NULL;
    }

    res->nf9_data = malloc(nf_size * sizeof(uint8_t));
    res->ipx_data = malloc(ipx_size * sizeof(uint8_t));
    if (res->nf9_data == NULL || res->ipx_data == NULL) {
        // Memory allocation error
        free(res->ipx_data);
        free(res->nf9_data);
        free(res);
        return NULL;
    }

    res->instr_alloc = NF9_TREC_DEF_INSTR;
    res->instr_size = 0;
    return res;
}

void
nf9_trec_destroy(struct nf9_trec *rec)
{
    free(rec->nf9_data);
    free(rec->ipx_data);
    free(rec);
}

int
nf9_trec_instr_add(struct nf9_trec **ptr, struct nf2ipx_instr instr)
{
    struct nf9_trec *rec = *ptr;
    if (rec->instr_size == rec->instr_alloc) {
        // Reallocate the structure
        size_t new_cnt = 2 * rec->instr_size;
        struct nf9_trec *new_rec = realloc(rec, nf9_trec_size(new_cnt));
        if (!new_rec) {
            return IPX_ERR_NOMEM;
        }

        new_rec->instr_alloc = new_cnt;
        rec = new_rec;
        *ptr = new_rec;
    }

    rec->instr_data[rec->instr_size++] = instr;
    return IPX_OK;
}
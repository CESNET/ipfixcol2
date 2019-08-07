/**
 * @file src/core/netflow2ipfix/netflow9_templates.h
 * @author Lukas Hutak <lukas.hutak@cesnet.cz>
 * @brief Auxiliary template management for NetFlow v9 to IPFIX conversion (header file)
 * @date 2018-2019
 *
 * Copyright(c) 2019 CESNET z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef IPFIXCOL2_NETFLOW9_TEMPLATES_H
#define IPFIXCOL2_NETFLOW9_TEMPLATES_H

#include <stdint.h>
#include <stddef.h>

/// Data conversion instructions
enum NF2IPX_ITYPE {
    /**
     * Copy memory
     * @note Copy specified @p size from the original NetFlow record to the new IPFIX record.
     */
    NF2IPX_ITYPE_CPY,
    /**
     * Convert relative timestamp to absolute timestamp
     * @note Value of @p size is ignored, the converted timestamp is always 8 bytes long.
     */
    NF2IPX_ITYPE_TS
};

/// Data conversion instructions with parameters
struct nf2ipx_instr {
    /// Instruction type
    enum NF2IPX_ITYPE itype;
    /// Size of used memory after conversion (in bytes)
    size_t size;
};

/// Template record action
enum REC_ACTION {
    /**
     * @brief Perform conversion of the record
     */
    REC_ACT_CONVERT,
    /**
     * @brief Drop the Data record
     *
     * Typical usage is in case of Options Template records, where the Scope is not valid
     * (for example, failed to convert Scope ID to IPFIX IE, missing scope, etc.)
     */
    REC_ACT_DROP
};

/**
 * @brief Template record
 *
 * This structure represents NetFlow-to-IPFIX template conversion. It consists of the original
 * NetFlow Template, a new converted IPFIX (Options) Template and Data Record conversion
 * instructions.
 */
struct nf9_trec {
    /// Conversion action
    enum REC_ACTION action;
    /// Template type (::IPX_NF9_SET_TMPLT or ::IPX_NF9_SET_OPTS_TMPLT)
    uint16_t type;

    /// Copy of the original NetFlow (Options) Template
    uint8_t *nf9_data;
    /// Size of the original NetFlow (Options) Template
    uint16_t nf9_size;
    /// Data record length of an original (a.k.a NetFlow v9) record described by the NF Template
    uint16_t nf9_drec_len;

    /**
     * Copy of the new IPFIX (Options) Template
     * @note If the action is ::REC_ACT_DROP, the pointer is always NULL!
     */
    uint8_t *ipx_data;
    /**
     * Size of the new IPFIX (Options) Template
     * @note If the action is ::REC_ACT_DROP, the size is always 0!
     */
    uint16_t ipx_size;
    /**
     * Data record length of a converter (a.k.a. IPFIX) record described by the IPFIX Template
     * @note If the action is ::REC_ACT_DROP, the size is always 0!
     */
    uint16_t ipx_drec_len;

    // --- Note: following fields are filled automatically  ---
    /// Number of pre-allocated instructions
    size_t instr_alloc;
    /// Number of valid instructions
    size_t instr_size;
    /**
     * Conversion instruction
     * @warning This field MUST be the last in the structure!
     */
    struct nf2ipx_instr instr_data[1];
};

/// Size of L1 and L2 template lookup table
#define TMPLTS_TABLE_SIZE 256

/// L1 template table
struct tmplts_l1_table {
    /**
     * Array of L2 tables (sparse array)
     * Keep in mind that if there are no templates in a L2 table, the L2 table doesn't exist
     * (i.e. its value is NULL)
     */
    struct tmplts_l2_table *l2_tables[TMPLTS_TABLE_SIZE];
};

/// L2 template table
struct tmplts_l2_table {
    /**
     * Array of template records (sparse array)
     * Keep in mind that if a template is missing in the array, the pointer is NULL.
     */
    struct nf9_trec *recs[TMPLTS_TABLE_SIZE];
};


/**
 * @brief Initialize template lookup structure
 * @param[in] tbl L1 template table
 */
void
nf9_tmplts_init(struct tmplts_l1_table *tbl);

/**
 * @brief Destroy template lookup structure and destroy all templates
 * @param[in] tbl L1 template table
 */
void
nf9_tmplts_destroy(struct tmplts_l1_table *tbl);

/**
 * @brief Find a template definition in the template lookup table
 * @param[in] tbl L1 template table
 * @param[in] id  Template ID
 * @return Pointer to the template or NULL (not found)
 */
struct nf9_trec *
nf9_tmplts_find(struct tmplts_l1_table *tbl, uint16_t id);

/**
 * @brief Insert a template record into Template lookup table
 *
 * @note If the template already exists, it will be automatically freed
 * @param[in] tbl L1 template table
 * @param[in] id  Template ID
 * @param[in] rec Template record to add
 * @return #IPX_OK on success
 * @return #IPX_ERR_NOMEM in case of memory allocation error
 */
int
nf9_tmplts_insert(struct tmplts_l1_table *tbl, uint16_t id, struct nf9_trec *rec);


/**
 * @brief Create a new Template structure
 *
 * All fields in the structure are initialized by calloc() and the structure is prepared for
 * up to ::NF9_TREC_DEF_INSTR instructions.
 * @param[in] nf_size  Memory allocated for raw NetFlow (Options) Template (in bytes)
 * @param[in] ipx_size Memory allocated for raw IPFIX (Options) Template (in bytes)
 * @return Pointer to the structure or NULL (memory allocation error)
 */
struct nf9_trec *
nf9_trec_new(size_t nf_size, size_t ipx_size);

/**
 * @brief Destroy a Template structure
 * @note NetFlow and IPFIX Templates will be destroyed, if defined.
 * @param[in] rec Template structure
 */
void
nf9_trec_destroy(struct nf9_trec *rec);

/**
 * @brief Add a conversion instruction to a Template record
 *
 * @warning Due to reallocation, pointer to the template can be changed!
 * @param[in] ptr   Pointer to a pointer to the Template record
 * @param[in] instr Instruction to add
 * @return #IPX_OK on success
 * @return #IPX_ERR_NOMEM in case of memory allocation error
 */
int
nf9_trec_instr_add(struct nf9_trec **ptr, struct nf2ipx_instr instr);

#endif //IPFIXCOL2_NETFLOW9_TEMPLATES_H

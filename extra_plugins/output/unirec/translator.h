/**
 * \file translator.h
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Conversion of IPFIX to LNF format (header file)
 */
/* Copyright (C) 2015-2018 CESNET, z.s.p.o.
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
 * This software is provided ``as is, and any express or implied
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

#ifndef LS_TRANSLATOR_H
#define LS_TRANSLATOR_H

#include <ipfixcol2.h>
#include <unirec/unirec.h>

// Size of conversion buffer
#define REC_BUFF_SIZE (65535)

struct translator_table_rec;
struct conf_unirec;

/**
 * \typedef translator_func
 * \brief Definition of conversion function
 * \param[in]  field      Pointer to an IPFIX field to convert
 * \param[in]  def        Definition of data conversion
 * \param[out] buffer_ptr Pointer to the conversion buffer where LNF value
 *   will be stored
 * \return On success return 0. Otherwise returns a non-zero value.
 */
typedef int (*translator_func)(const struct fds_drec_field *field,
    struct conf_unirec *conf, const struct translator_table_rec *def);

/**
 * \brief Conversion record
 */
typedef struct translator_table_rec
{
    /** Identification of the IPFIX Information Element              */
    struct ipfix_s {
        /** Private Enterprise Number of the Information Element     */
        uint32_t pen;
        /** ID of the Information Element within the PEN             */
        uint16_t ie;
    } ipfix;
    uint8_t ipfix_priority;

    /** Identification of the corresponding LNF Field */
    ur_field_id_t ur_field_id;

    /** Conversion function */
    translator_func func;
} translator_table_rec_t;

typedef struct translator_s {
    /** Instance context (only for log!) */
    ipx_ctx_t *ctx;

    /** Created and initialized UniRec template */
    const ur_template_t *urtmpl;

    /** Required UniRec fields to be filled */
    uint8_t *req_fields;

    /** UniRec fields that still must be filled by translate, it must be reinitialized for every IPFIX record */
    uint8_t *todo_fields;

    /**
     * Map of UniRec field id and index into UniRec template/req_fields/todo_fields.
     *
     * field_idx[UniRec_FieldID] is the index of the field in req_fields
     */
    ur_field_id_t *field_idx;

    /** Allocated size of the Private conversion table         */
    size_t table_capacity;

    /** Number of entries in the Private conversion table         */
    size_t table_count;

    /** Private conversion table         */
    translator_table_rec_t *table;
} translator_t;

/** struct for ipfix elements' 2 ids */
typedef struct ipfixElement {
   uint16_t id;
   uint32_t en;
} ipfixElement_t;

/** linked list of UnirecFields */
typedef struct unirecField {
   /**
    * Name of UniRec field
    */
   char *name;

   /**
    * Type of UniRec field as string, used to generate data format string
    */
   char *unirec_type_str;

   /**
    * Type of UniRec field
    */
   ur_field_type_t unirec_type;

   /**
    * Pointer to the next UniRec field
    */
   struct unirecField *next;

   /**< Number of ipfix elements */
   uint32_t ipfixCount;

   /**
    * IPFIX elements that are mapped to this UniRec field
    */
   ipfixElement_t ipfix[1];
} unirecField_t;

unirecField_t *load_IPFIX2UR_mapping(ipx_ctx_t *ctx, uint32_t *urcount, uint32_t *ipfixcount);

void free_IPFIX2UR_map(unirecField_t *map);

/**
 * \brief Create a new instance of a translator
 * \return Pointer to the instance or NULL.
 */
translator_t *translator_init(ipx_ctx_t *ctx, unirecField_t *map, uint32_t ipfixfieldcount);

/**
 * \brief Initialize arrays related to the UniRec template
 * \param[in,out] tr Context of translator
 * \param[in] urtmpl Initialized UniRec template
 * \param[in] urspec String containing a comma-separated list of UniRec field names, optionally with '?' to mark optional fields
 *
 * \return 0 on success, 1 on error
 */
int translator_init_urtemplate(translator_t *tr, ur_template_t *urtmpl, char *urspec);

/**
 * \brief Destroy instance of a translator
 * \param[in] trans Instance
 */
void
translator_destroy(translator_t *trans);

/**
 * \brief Convert a IPFIX record to a LNF record
 * \warning LNF record is always automatically cleared before conversion start.
 * \param[in]     trans     Translator instance
 * \param[in]     ipfix_rec IPFIX record (read only!)
 * \param[in,out] lnf_rec   Filled LNF record
 * \param[in]     flags     Flags for iterator over the IPFIX record
 * \return Number of converted fields
 */
int
translator_translate(translator_t *trans, struct conf_unirec *conf, struct fds_drec *ipfix_rec, uint16_t flags);

#endif //LS_TRANSLATOR_H

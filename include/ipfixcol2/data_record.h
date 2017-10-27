/**
 * \file ipfixcol2/data_record.h
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Data record in the IPFIX message (header file)
 * \date 2016
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

#ifndef IPFIXCOL_DATA_RECORD_H
#define IPFIXCOL_DATA_RECORD_H

#include <stdint.h>
#include <ipfixcol2/template.h>

/**
 * \brief Structure for parsed IPFIX Data record
 *
 * Represents a one parsed data record in the IPFIX packet.
 */
struct ipx_drec {
	uint8_t                   *rec;         /**< Start of the record              */
	unsigned int               size;        /**< Size of the record (in bytes)    */
	const struct ipx_template *template;    /**< Template (always must be defined)*/
	uint8_t                    ext_data[1]; /**< Start of extension records       */
};

/**
 * \brief Structure for a data field in a data record in a IPFIX message
 *
 * This structure is used by a lookup function or an iterator.
 */
struct ipx_drec_field {
	/**
	 * Pointer to the field in a data record. It always points to the beginning
	 * of Information Element data (i.e. for variable-length elements behind
	 * octet prefix with real-length).
	 */
	uint8_t     *data;
	/** Real length of the field                                             */
	unsigned int real_length;
	/** Pointer to the field description (IDs, data types, etc.)             */
	struct ipx_tfield *info;
};

/**
 * \brief Get a field in a data record
 *
 * Try to find the first occurrence of the field defined by an Enterprise Number
 * and an Information Element ID in a data record.
 * \param[in] rec     Pointer to the data set
 * \param[in] en      Enterprise Number
 * \param[in] id      Information Element ID
 * \param[out] field  Pointer to a variable where the result will be stored
 * \return If the field is present in the record, this function will fill
 *   \p field and return 0. Otherwise (usually the field is not present in
 *   the record) returns nonzero value and \p field is not filled
 */
IPX_API int
ipx_drec_get_field(struct ipx_drec *rec, uint32_t en, uint16_t id,
	struct ipx_drec_field *field);

/** \brief Iterator over all data fields in a data record                    */
struct ipx_drec_iter {
	/** Current field of an iterator                                         */
	struct ipx_drec_field field;

	/** FOR INTERNAL USE ONLY. DO NOT USE DIRECTLY!                          */
	struct {
		struct ipx_drec *record;       /**< Pointer to the data record       */
		unsigned int     next_offset;  /**< Offset of the next field         */
		unsigned int     next_idx;     /**< Index of the next field          */
	} _internal_; /**< Implementation can change!                            */
};

/**
 * \brief Initialize an iterator to data fields in a data record
 * \warning
 *   After initialization the iterator only has initialized internal structures,
 *   but public part is still undefined i.e. does NOT point to the first field
 *   in the record. To get the first field and for code example,
 *   see ipx_drec_iter_next().
 * \param[out] iter    Pointer to the iterator to initialize
 * \param[in]  record  Pointer to data record
 */
IPX_API void
ipx_drec_iter_init(struct ipx_drec_iter *iter, struct ipx_drec *record);

/**
 * \brief Get next record in the field
 *
 * Move the iterator to the next field in the order. If this function was NOT
 * previously called after initialization by ipx_drec_iter_init(), then the
 * iterator will point to the first field in a data record.
 *
 * \code{.c}
 *  // How to use iterators...
 *  struct ipx_drec_iter it;
 *	ipx_drec_iter_init(it, record);
 *
 *	while(ipx_drec_iter_next(it) >= 0) {
 *		// Add your code here, for example:
 *		const struct ipx_template_field *field = it.field.info;
 *		printf("en: %" PRIu32 " & id: %" PRIu16 "\n", field->en, field->id);
 *	}
 *	ipx_drec_iter_destroy(it);
 * \endcode
 *
 * \param[in,out] iter Pointer to the iterator
 * \return When the next field is prepared, returns an index from the field in
 *   the file (starts from 0). Otherwise (no more field in the record) returns
 *   negative value.
 */
IPX_API int
ipx_drec_iter_next(struct ipx_drec_iter *iter);

/**
 * \brief Destroy an iterator to a data field
 * \param[in] iter Pointer to the iterator
 */
IPX_API void
ipx_drec_iter_destroy(struct ipx_drec_iter *iter);

#endif //IPFIXCOL_DATA_RECORD_H

/**
 * \file src/template.c
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Parsed IPFIX Template (source file)
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

#include <stdint.h>
#include <ipfixcol2.h>

/** Size of the Top-level table with pointers to the second-level table      */
#define IPX_TEMPLATE_IDX_1LEVEL (128)
/** Size of the second-level table with indexes                              */
#define IPX_TEMPLATE_IDX_2LEVEL (256)

/** Index validity flag                                                      */
#define IPX_TEMPLATE_IDX_VALID      (1U << 15)
/**
 * ID Collision flag (same Information Elements IDs in the fields).
 *
 * Means that stored index represents the first element with the IE ID, but
 * there is one or more elements with the same IE ID (and the same or different
 * Enterprise Number)
 */
#define IPX_TEMPLATE_IDX_COLLISION  (1U << 14)
/** Mask for hiding flags in an index value                                  */
#define IPX_TEMPLATE_IDX_MASK       (0x3FFFU)

/** \brief Timestamps relating to a template record                          */
struct ipx_template_time {
	/** Timestamp of the first received (seconds since UNIX epoch)           */
	uint64_t first;
	/** Timestamp of the last received (seconds since UNIX epoch)            */
	uint64_t last;
	/**
	 * Timestamp of the template withdrawal (in seconds ).
	 * If equals to "0", then the template is still valid.
	 */
	uint64_t end;
};

/**
 * \brief Structure for a parsed IPFIX template
 *
 * This dynamically sized structure wraps a parsed copy of IPFIX template.
 */
struct ipx_template {
	/** Type of the template                                                 */
	enum IPX_TEMPLATE_TYPE template_type;
	/** Type of the Option Template.                                         */
	enum IPX_OPTS_TEMPLATE_TYPE options_type;

	/** Template ID                                                          */
	uint16_t id;
	/**
	 * Length of a data record using this template.
	 * \warning If the template has at least one element with variable-length
	 *   (property has_dynamic), this value represents the smallest possible
	 *   length of corresponding data record. Otherwise represents real-length.
	 */
	uint16_t data_length;

	/** Raw template record (starts with a header)                           */
	struct raw_s {
		/** Pointer to the copy of template record (starts with a header)    */
		void     *data;
		/** Length of the record (in bytes)                                  */
		uint16_t  length;
	} raw;

	/** Template properties */
	struct properties_s {
		/**
		 * Has multiple defitions of the same element (i.e. combination of
		 * Information Element ID and Enterprise Number)
		 * */
		bool has_multiple_defs;
		/** Has at least one variable-length element                         */
		bool has_dynamic;
	} properties;

	/**
	 * Time information related to exporting process.
	 * All timestamp (seconds since UNIX epoch) are based on "Export time"
	 * from the IPFIX message header.
	 */
	struct ipx_template_time time_export;

	/**
	 * Time information related to collecting process.
	 * All timestamp represents monotonic time since some unspecified starting
	 * point i.e. not real "wall-clock" time and not affected by discontinuous
	 * jump in the system time.
	 */
	struct ipx_template_time time_collect;

	/**
	 * First index (i.e. occurence) of IPFIX element in the template's fields
	 * for fast lookup.
	 *
	 * Idea: Table with first index of each Information Element ID (ignores
	 * Enterprise Number specification -> safe memory usage but can cause
	 * index mapping collisions).
	 *
	 * The Information Element ID of a template (see typedef ::template_ie) is
	 * 16bit value (with reserver 1 MSB bit) and can have values in the range
	 * <0;32768>. Therefor we can create a sparse 2-level table where
	 * the top-level table has 128 references (#IPX_TEMPLATE_IDX_1LEVEL) to
	 * the seconds-level tables where each table has 256 index records.
	 * (#IPX_TEMPLATE_IDX_2LEVEL). The second-level hierarchy tables can
	 * miss (i.e. NULL pointer) when not used.
	 *
	 * Each index record is 16bits long and because IPFIX template can have at
	 * most (65535 - 16 - 8) / 4 = 16'377 fields
	 * (Max. msg. size - Msg. header - Set and Template header / Field size)
	 * therefor 2 MSB bits of uint16_t can be used as flags:
	 * #IPX_TEMPLATE_IDX_VALID and #IPX_TEMPLATE_IDX_COLLISION.
	 */
	uint16_t *indexes[IPX_TEMPLATE_IDX_1LEVEL];

	/** Number of scope fields (first N records of the template)             */
	uint16_t fields_cnt_scope;
	/** Total number of fields                                               */
	uint16_t fields_cnt_total;

	/**
	 * Array of parsed fields.
	 * This element MUST be the last element in this structure.
	 */
	struct ipx_template_field fields_data[1];
};




uint16_t
ipx_template_get_fields_count(const ipx_template_t *template)
{
	return template->fields_cnt_total;
}

/*
 * Get the number of fields with given Enterprise Number and Information
 * Element ID in a template
 */
uint16_t
ipx_template_field_present(const ipx_template_t *template, uint32_t en,
		uint16_t id)
{
	const uint16_t *level = template->indexes[id / IPX_TEMPLATE_IDX_2LEVEL];
	if (!level) {
		return 0;
	}

	const uint16_t record = level[id % IPX_TEMPLATE_IDX_2LEVEL];
	if (!(record & IPX_TEMPLATE_IDX_VALID)) {
		return 0;
	}

	unsigned int idx = record & IPX_TEMPLATE_IDX_MASK;
	const struct ipx_template_field *field = &template->fields_data[idx];
	if (!(record & IPX_TEMPLATE_IDX_COLLISION)) {
		// No collisions
		return (field->en == en) ? 1 : 0;
	}

	// One or more collision -> go through the rest of the fields
	const unsigned int fields_cnt = ipx_template_get_fields_count(template);
	uint16_t match_cnt = 0;

	while (idx < fields_cnt) {
		if (field->en == en) {
			++match_cnt;
		}

		++idx;
		++field;
	}

	return match_cnt;
}
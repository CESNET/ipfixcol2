/**
 * \file include/ipfixcol2/template.h
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Parsed IPFIX Template (header file)
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

#ifndef IPFIXCOL_TEMPLATE_H
#define IPFIXCOL_TEMPLATE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <ipfixcol2/api.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \defgroup ipxTemplatesAPI Templates
 * \ingroup publicAPIs
 * \brief Public functions for parsed IPFIX templates
 *
 * @{
 */

/** Types of IPFIX (Options) Templates                                       */
enum IPX_TEMPLATE_TYPE {
	IPX_TEMPLATE_DEF,          /**< Definition of Template                   */
	IPX_TEMPLATE_WD,           /**< Withdrawal of Template                   */
	IPX_TEMPLATE_OPTIONS_DEF,  /**< Definition of Options Template           */
	IPX_TEMPLATE_OPTIONS_WD    /**< Withdrawal of Options Template           */
};

/**
 * \brief Standard types of Options Templates
 * \remark Based on RFC 7011, Section 4.
 */
enum IPX_OPTS_TEMPLATE_TYPE {
	/** Not an Options Template i.e. "Normal" Template                       */
	IPX_OPTS_NO_OPTIONS,
	/** The Metering Process Statistics                                      */
	IPX_OPTS_METER_PROC_STAT,
	/** The Metering Process Reliability Statistics                          */
	IPX_OPTS_METER_PROC_RELIABILITY_STAT,
	/** The Exporting Process Reliability Statistics                         */
	IPX_OPTS_EXPORT_PROC_RELIABILITY_STAT,
	/** The Flow Keys                                                        */
	IPX_OPTS_FLOW_KEYS,
	/** Unknown type of Options Template                                     */
	IPX_OPTS_UNKNOWN
};

/**
 * \brief Structure for parsed IPFIX elemenent in an IPFIX template
 */
struct ipx_template_field {
	/** Enterpirse Number      */
	uint32_t en;
	/** Information Element ID */
	uint16_t id;

	/**
	 * The real length of the Information Element.
	 * The value #IPFIX_VAR_IE_LENGTH (i.e. 65535) is reserved for
	 * variable-length information elements.
	 */
	uint16_t length;
	/**
	 * The offset from the start of a data record in octets.
	 * The value #IPFIX_VAR_IE_LENGTH (i.e. 65535) is reserved for unknown
	 * offset when there is at least one variable-length element among
	 * preceding elements in the same template.
	 */
	uint16_t offset;
	/**
	 * The last of identical Elements with the same combination of an
	 * Information Element ID and an Enterpise Number, i.e. if equals to
	 * "false", then there is at least one more element with the same
	 * combination and a higher index in the template.
	 */
	bool last_identical;

	/**
	 * Detailed definition of the element (data/sementic/unit type).
	 * Can be NULL, when the definition is missing in the configuration.
	 */
	const struct ipx_element *definition;
};

/** IPFIX template type */
typedef struct ipx_template ipx_template_t;


/**
 * \brief Create a new template from an IPFIX template definition
 * \param[in] rec      Pointer to the IPFIX template header
 * \param[in] max_size  Maximum length of the raw template (in bytes).
 *   Typically lenght to the end of a (Options) Template Set.
 * \param[in] type     Type of IPFIX Set (#IPFIX_SET_TEMPLATE for Template
 *   or #IPFIX_SET_OPTIONS_TEMPLATE for Options Template).
 * \return On success returns pointer to the template. Otherwise (invalid
 *   template definition or memory allocation error) returns NULL.
 */
API ipx_template_t *
ipx_template_create(const void *rec, uint16_t max_size, enum IPFIX_SET_ID type);

/**
 * \brief Destroy a template
 * \param[in] tmplt Pointer to the template to be destroyed
 */
API void
ipx_template_destroy(ipx_template_t *tmplt);

/**
 * \brief Update detailed descriptions of parsed fields
 *
 * For each field of the template try to find its definition i.e. name, data
 * type/semantics/unit etc. and store it (just pointers). If some fields were
 * already defined, for example, by previous call of this function,
 * the description of that fields is NOT updated, but fields with missing
 * description will be updated.
 *
 * \param[in,out] tmplt Pointer to the template
 */
API void
ipx_template_update_descriptions(ipx_template_t *tmplt /* TODO */);

/**
 * \brief Get the type of a template
 * \param[in] tmplt Pointer to the template
 * \return The type
 */
API enum IPX_TEMPLATE_TYPE
ipx_template_get_type(const ipx_template_t *tmplt);

/**
 * \brief Get the options type (only for Options Template) of a template
 *
 * If the template is not Options Template returns #IPX_OPTS_NO_OPTIONS.
 * Otherwise, if known, returns the type.
 * \param[in] tmplt Pointer to the template
 * \return The options type
 */
API enum IPX_OPTS_TEMPLATE_TYPE
ipx_template_get_opts_type(const ipx_template_t *tmplt);

/**
 * \brief Get the Template ID of a template
 * \param[in] tmplt Pointer to the template
 * \return The Template ID
 */
API uint16_t
ipx_template_get_id(const ipx_template_t *tmplt);

/**
 * \brief Get the total number of all fields in a template
 *
 * Returns count of scope and non-scope fields in the template
 * \param[in] tmplt Pointer to the template
 * \return Count
 */
API uint16_t
ipx_template_get_fields_count(const ipx_template_t *tmplt);

/**
 * \brief Get the number of scope fields only in a template
 * \param[in] tmplt Pointer to the template
 * \return Count
 */
API uint16_t
ipx_template_get_scope_fields_count(const ipx_template_t *tmplt);

/**
 * \brief Get the pointer to the array of all template fields
 * \param[in] tmplt  Pointer to the template
 * \param[out] size  Number of the fields in the array (can be NULL)
 * \return Pointer to the array of templates
 */
const struct ipx_template_field *
ipx_template_get_all_fields(const ipx_template_t *tmplt, size_t *size);

/**
 * \brief Get the description of a field with a given index in a template
 * \param[in] tmplt  Pointer to the template
 * \param[in] idx    Index of the field (indexed from 0)
 * \return
 */
const struct ipx_template_field *
ipx_template_get_field(const ipx_template_t *tmplt, uint16_t idx);

/**
 * \brief Get the number of fields with given Enterprise Number and Information
 *   Element ID in a template
 * \param[in] tmplt Pointer to the template
 * \param[in] en    Enterprise Number
 * \param[in] id    Information Element ID
 * \return Number of fields
 */
API uint16_t
ipx_template_field_present(const ipx_template_t *tmplt, uint32_t en,
		uint16_t id);

/**@}*/
#ifdef __cplusplus
}
#endif

#endif //IPFIXCOL_TEMPLATE_H

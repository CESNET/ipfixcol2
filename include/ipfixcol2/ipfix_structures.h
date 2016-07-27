/**
 * \file ipfixcol2/ipfix_structures.h
 * \author Radek Krejci <rkrejci@cesnet.cz>
 * \author Lukas Hutak <xhutak01@cesnet.cz>
 * \brief Structures and macros definition for IPFIX processing
 */

/* Copyright (C) 2009-2016 CESNET, z.s.p.o.
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

#ifndef IPFIX_H_
#define IPFIX_H_

/**
 * \defgroup ipfixStructures IPFIX Structures
 * \ingroup publicAPIs
 * \brief IPFIX structures allowing access to the data
 *
 * \warning
 * All fields of all structures are stored in Network Byte Order
 * (i.e. Big Endian Order). To read the content of the fields, you MUST use
 * conversion functions from "Network Byte Order" to "Host Byte Order",
 * such as ntohl, be64toh, etc. To write the content to the fields, you MUST use
 * conversion functions from "Host Byte Order" to "Network Byte Order",
 * such as htonl, htobe64, etc. See Linux manual pages of the functions.
 * Note: "Host Byte Order" represents byte order of a machine, where this
 * source code is build.
 *
 * \remark Based on RFC 7011
 *
 * @{
 */

#include <stdint.h>
#include <ipfixcol2/api.h>

/**
 * \struct ipfix_header
 * \brief IPFIX header structure
 *
 * This structure represents always present header of each IPFIX message.
 * \remark Based on RFC 7011, Section 3.1.
 */
struct __attribute__((__packed__)) ipfix_header {
	/**
	 * \brief
	 * Version of Flow Record format exported in this message.
	 *
	 * The value of this field is 0x000a for the current version,
	 * incrementing by one the version used in the NetFlow services export
	 * version 9.
	 */
	uint16_t version;

	/**
	 * \brief
	 * Total length of the IPFIX Message, measured in octets, including Message
	 * Header and Set(s).
	 */
	uint16_t length;

	/**
	 * \brief
	 * Time at which the IPFIX Message Header leaves the Exporter.
	 *
     * Expressed in seconds since the UNIX epoch of 1 January 1970 at
     * 00:00 UTC, encoded as an unsigned 32-bit integer.
	 */
	uint32_t export_time;

	/**
	 * \brief
	 * Incremental sequence counter modulo 2^32 of all IPFIX Data Records
     * sent in the current stream from the current Observation Domain by
     * the Exporting Process.
     *
     * \warning
     * Each SCTP Stream counts sequence numbers separately, while all messages
     * in a TCP connection or UDP session are considered to be part of the same
     * stream.
     *
     * \remark
     * This value can be used by the Collecting Process to identify whether
     * any IPFIX Data Records have been missed. Template and Options Template
     * Records do not increase the Sequence Number.
	 */
	uint32_t sequence_number;

	/**
	 * \brief
	 * A 32-bit identifier of the Observation Domain that is locally
     * unique to the Exporting Process.
     *
     * The Exporting Process uses the Observation Domain ID to uniquely
     * identify to the Collecting Process the Observation Domain that metered
     * the Flows. It is RECOMMENDED that this identifier also be unique
     * per IPFIX Device.
     *
     * \remark
     * Collecting Processes SHOULD use the Transport Session and the
     * Observation Domain ID field to separate different export streams
     * originating from the same Exporter.
     *
     * \remark
     * The Observation Domain ID SHOULD be 0 when no specific
     * Observation Domain ID is relevant for the entire IPFIX Message,
     * for example, when exporting the Exporting Process Statistics,
     * or in the case of a hierarchy of Collectors when aggregated Data Records
     * are exported.
	 */
	uint32_t observation_domain_id;
};

/** IPFIX identification (NetFlow version 10)  */
#define IPFIX_VERSION               0x000a
/** Length of the IPFIX header (in bytes).     */
#define IPFIX_HEADER_LENGTH 		16


/**
 * \struct ipfix_set_header
 * \brief Common IPFIX Set (header) structure
 * \remark Based on RFC 7011, Section 3.3.2.
 */
struct __attribute__((__packed__)) ipfix_set_header {
	/**
	 * \brief Identifies the Set.
	 *
	 * A value of 2 is reserved for Template Sets (see structure
	 * ipfix_template_set). A value of 3 is reserved for Options Template Sets
	 * (see structure ipfix_options_template_set). Values from 4 to 255 are
	 * reserved for future use. Values 256 and above are used for Data Sets
	 * (see structure ipfix_data_set).
	 * The Set ID values of 0 and 1 are not used, for historical reasons
	 * \see For options enumeration see #IPFIX_SET_ID
	 */
	uint16_t flowset_id;

	/**
	 * \brief
	 * Total length of the Set, in octets, including the Set Header, all
     * records, and the optional padding.
     *
     * Because an individual Set MAY contain multiple records, the Length
     * value MUST be used to determine the position of the next Set.
	 */
	uint16_t length;
};

/** Flowset type identifiers */
enum IPFIX_SET_ID {
	IPFIX_SET_TEMPLATE         = 2,  /**< Template Set ID              */
	IPFIX_SET_OPTIONS_TEMPLATE = 3,  /**< Options Template Set ID      */
	IPFIX_SET_MIN_DATA_SET_ID  = 256 /**< Minimum ID for any Data Set  */
};


/**
 * \typedef template_ie
 * \brief Template's definition of an IPFIX Information Element.
 *
 * The type is defined as 32b value containing one of (union) Enterprise Number
 * or standard element definition containing IE ID and its length. There are
 * two #template_ie in the following scheme.
 * <pre>
 *   0                   1                   2                   3
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |E|  Information Element ident. |        Field Length           |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |                      Enterprise Number                        |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * </pre>
 *
 * \remark Based on RFC 7011, Section 3.2.
 */
typedef union __attribute__((__packed__)) template_ie_u {
	/**
	 * \brief
	 * Information Element identifier and the length of the corresponding
	 * encoded Information Element
	 */
	struct {
		/**
		 * \brief
		 * A numeric value that represents the Information Element.
		 *
		 * \warning
		 * The first (highest) bit is Enterprise bit. This is the first bit of
		 * the Field Specifier. If this bit is zero, the Information Element
		 * identifier identifies an Information Element in
		 * [<a href="http://www.iana.org/assignments/ipfix/ipfix.xhtml">
		 * IANA-IPFIX</a>], and the four-octet Enterprise Number field MUST NOT
		 * be present. If this bit is one, the Information Element identifier
		 * identifies an enterprise-specific Information Element, and
		 * the Enterprise Number filed MUST be present.
		 */
		uint16_t id;

		/**
		 * \brief
		 * The length of the corresponding encoded Information Element, in
		 * octets.
		 *
		 * The value #IPFIX_VAR_IE_LENGTH (65535) is reserved for
		 * variable-length Information Elements.
		 */
		uint16_t length;
	} ie;

	/**
	 * \brief
	 * IANA enterprise number of the authority defining the Information Element
	 * identifier in this Template Record.
	 *
     * Refer to [<a href="http://www.iana.org/assignments/enterprise-numbers/">
	 * IANA Private Enterprise Numbers</a>].
	 */
	uint32_t enterprise_number;
} template_ie;

/** Length value signaling variable-length IE  */
#define IPFIX_VAR_IE_LENGTH 65535


/**
 * \struct ipfix_template_record
 * \brief Structure of the IPFIX Template Record Header
 *
 * This record MUST be inside the IPFIX Template Set
 * (see struct ipfix_template_set).
 */
struct __attribute__((__packed__)) ipfix_template_record {
	/**
	 * \brief Template Identification Number
	 *
	 * Each Template Record is given a unique Template ID in the range
     * 256 (#IPFIX_SET_MIN_DATA_SET_ID) to 65535.  This uniqueness is local to
     * the Transport Session and Observation Domain that generated
     * the Template ID. Since Template IDs are used as Set IDs in the Sets
     * they describe, values 0-255 are reserved for special Set types
     * (e.g., Template Sets themselves).
     *
     * \remark
     * Templates and Options Templates cannot share Template IDs within
     * a Transport Session and Observation Domain.
     *
     * \remark
     * There are no constraints regarding the order of the Template ID
     * allocation.  As Exporting Processes are free to allocate Template IDs
     * as they see fit, Collecting Processes MUST NOT assume incremental
     * Template IDs, or anything about the contents of a Template based on its
     * Template ID alone.
	 */
	uint16_t template_id;

	/**
	 * \brief Number of fields in this Template Record.
	 *
	 * If the number is 0, then this template is called Template Withdrawal,
	 * revokes a previously defined template and consists only from members
	 * template_id and count. For more details see RFC 7011, Section 8.1.
	 */
	uint16_t count;

	/**
	 * \brief
	 * Field Specifier(s)
	 */
	template_ie fields[1];
};

/** Size of template withdrawal record */
#define IPFIX_TMPLT_WITHDRAWAL_REC_SIZE    4

/**
 * \struct ipfix_template_set
 * \brief IPFIX Template Set structure
 *
 * Consists of the common Set header and the first Template record.
 */
struct __attribute__((__packed__)) ipfix_template_set {
	/**
	 * Common IPFIX Set header.
	 * Identification of the flowset MUST be 2 (#IPFIX_SET_TEMPLATE)
	 */
	struct ipfix_set_header header;

	/**
	 * The first of templates records in this Template Set. Real size of the
	 * record is unknown here due to a variable count of fields in each record.
	 */
	struct ipfix_template_record first_record;
};


/**
 * \struct ipfix_options_template_record
 * \brief Structure of the IPFIX Options Template record
 *
 * This record MUST be inside the IPFIX Options Template Set
 * (see struct ipfix_options_template_set).
 */
struct __attribute__((__packed__)) ipfix_options_template_record {
	/**
	 * \brief Template Identification Number
	 *
	 * Each Template Record is given a unique Template ID in the range
     * 256 (#IPFIX_SET_MIN_DATA_SET_ID) to 65535.  This uniqueness is local to
     * the Transport Session and Observation Domain that generated
     * the Template ID. Since Template IDs are used as Set IDs in the Sets
     * they describe, values 0-255 are reserved for special Set types
     * (e.g., Template Sets themselves).
     *
     * \remark
     * Templates and Options Templates cannot share Template IDs within
     * a Transport Session and Observation Domain.
     *
     * \remark
     * There are no constraints regarding the order of the Template ID
     * allocation.  As Exporting Processes are free to allocate Template IDs
     * as they see fit, Collecting Processes MUST NOT assume incremental
     * Template IDs, or anything about the contents of a Template based on its
     * Template ID alone.
	 */
	uint16_t template_id;

	/**
	 * \brief Number of all fields in this Options Template Record, including
	 *   the Scope Fields.
	 *
	 * If the number is 0, then this template is called Options Template
	 * Withdrawal, revokes a previously defined template and consists only
	 * from members template_id and count. For more details see RFC 7011,
	 * Section 8.1.
	 */
	uint16_t count;

	/**
	 * \brief Number of scope fields in this Options Template Record.
	 *
	 * The Scope Fields are normal Fields except that they are interpreted
	 * as scope at the Collector. A scope field count of N specifies that
	 * the first N Field Specifiers in the Template Record are Scope Fields
	 * \warning The Scope Field Count MUST NOT be zero.
	 */
	uint16_t scope_field_count;

	/**
	 * Field Specifier(s)
	 */
	template_ie fields[1];
};

/**
 * \struct ipfix_options_template_set
 * \brief IPFIX Options Template Set structure
 *
 * Consists of the common Set header and the first Options Template record.
 */
struct __attribute__((__packed__)) ipfix_options_template_set {
	/**
	 * Common IPFIX Set header.
	 * Identification of the flowset MUST be 3 (#IPFIX_SET_OPTIONS_TEMPLATE)
	 */
	struct ipfix_set_header header;

	/**
	 * The first of templates records in this Options Template Set. Real size of
	 * the record is unknown here due to a variable count of fields in each
	 * record.
	 */
	struct ipfix_options_template_record first_record;
};

/**
 * \struct ipfix_data_set
 * \brief IPFIX Data records Set structure
 *
 * The Data Records are sent in Data Sets. It consists only of one or more Field
 * Values. The Template ID to which the Field Values belong is encoded in the
 * Set Header field "Set ID", i.e., "Set ID" = "Template ID".
 */
struct __attribute__((__packed__)) ipfix_data_set {
	/**
	 * Common IPFIX Set header.
	 * Identification of the flowset MUST be at least 256
	 * (#IPFIX_SET_MIN_DATA_SET_ID) and at most 65535.
	 */
	struct ipfix_set_header header;

	/** Start of the first Data record */
	uint8_t records[1];
};

/**@}*/

#endif /* IPFIX_H_ */

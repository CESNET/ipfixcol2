/**
 * \file include/ipfixcol2/ipfix_element.h
 * \author Lukas Hutak <xhutak01@stud.fit.vutbr.cz>
 * \brief Description of IPFIX Information Elements
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

#ifndef _IPFIX_ELEMENTS_
#define _IPFIX_ELEMENTS_

#include <stdint.h>

/**
 * \defgroup ipxElementAPI IPFIX Information Model
 * \ingroup publicAPIs
 * \brief Description of IPFIX Information Elements (i.e. data types, data
 *   semantics, units, etc.)
 * \remark Based on RFC 5610, RFC 7012 and RFC 6313
 * @{
 */

/**
 * \enum IPX_ELEMENT_DATA
 * \brief IPFIX Element data type
 *
 * This enumeration describes the set of valid abstract data types of the
 * IPFIX information model, independent of encoding. Note that further
 * abstract data types may be specified by future updates to this enumeration.
 *
 * \warning
 * The abstract data type definitions in this section are intended only to
 * define the values which can be taken by Information Elements of each type.
 * For example, #IPX_ET_UNSIGNED_64 does NOT mean that an element with this
 * type occupies 8 bytes, because it can be stored on 1,2,3,4,5,6,7 or 8 bytes.
 * The encodings of these data types for use with the IPFIX protocol are
 * defined in RFC 7011, Section 6.1.
 *
 * \remark Based on RFC 7012, Section 3.1 and RFC 6313, Section 11.1
 */
enum IPX_ELEMENT_TYPE {
	/**
	 * The type represents a finite-length string of octets.
	 */
	IPX_ET_OCTET_ARRAY = 0,

	/**
	 * The type represents a non-negative integer value in the range
	 * of 0 to 255.
	 */
	IPX_ET_UNSIGNED_8,

	/**
	 * The type represents a non-negative integer value in the range
	 * of 0 to 65,535.
	 */
	IPX_ET_UNSIGNED_16,

	/**
	 * The type represents a non-negative integer value in the range
	 * of 0 to 4,294,967,295.
	 */
	IPX_ET_UNSIGNED_32,

	/**
	 * The type represents a non-negative integer value in the range
	 * of 0 to 18,446,744,073,709,551,615.
	 */
	IPX_ET_UNSIGNED_64,

	/**
	 * The type represents an integer value in the range of -128 to 127.
	 */
	IPX_ET_SIGNED_8,

	/**
	 * The type represents an integer value in the range of -32,768 to 32,767.
	 */
	IPX_ET_SIGNED_16,

	/**
	 * The type represents an integer value in the range
	 * of -2,147,483,648 to 2,147,483,647.
	 */
	IPX_ET_SIGNED_32,

	/**
	 * The type represents an integer value in the range
	 * of -9,223,372,036,854,775,808 to 9,223,372,036,854,775,807.
	 */
	IPX_ET_SIGNED_64,

	/**
	 * The type corresponds to an IEEE single-precision 32-bit floating-point
	 * type.
	 */
	IPX_ET_FLOAT_32,

	/**
	 * The type corresponds to an IEEE double-precision 64-bit floating-point
	 * type.
	 */
	IPX_ET_FLOAT_64,

	/**
	 * The type "boolean" represents a binary value. The only allowed values
	 * are "true" and "false".
	 */
	IPX_ET_BOOLEAN,

	/**
	 * The type represents a MAC-48 address as define in IEEE 802.3, 2012
	 */
	IPX_ET_MAC_ADDRESS,

	/**
	 * The type represents a finite-length string of valid characters from
	 * the Unicode coded character set.
	 */
	IPX_ET_STRING,

	/**
	 * The type represents a time value expressed with second-level precision.
	 */
	IPX_ET_DATE_TIME_SECONDS,

	/**
	 * The type represents a time value expressed with millisecond-level
	 * precision.
	 */
	IPX_ET_DATE_TIME_MILLISECONDS,

	/**
	 * The type represents a time value expressed with microsecond-level
	 * precision.
	 */
	IPX_ET_DATE_TIME_MICROSECONDS,

	/**
	 * The type represents a time value expressed with nanosecond-level
	 * precision.
	 */
	IPX_ET_DATE_TIME_NANOSECONDS,

	/**
	 * The type represents an IPv4 address.
	 */
	IPX_ET_IPV4_ADDRESS,

	/**
	 * The type represents an IPv6 address.
	 */
	IPX_ET_IPV6_ADDRESS,

	/**
	 * The type represents a list of any Information Element used
	 * for single-valued data types.
	 */
	IPX_ET_BASIC_LIST,

	/**
	 * The type represents a list of a structured data type, where the data
	 * type of each list element is the same and corresponds with a single
	 * Template Record.
	 */
	IPX_ET_SUB_TEMPLATE_LIST,

	/**
	 * The type "subTemplateMultiList" represents a list of structured data
	 * types, where the data types of the list elements can be different and
	 * correspond with different Template definitions.
	 */
	IPX_ET_SUB_TEMPLATE_MULTILIST,

	/**
	 * An unassigned type (invalid value).
	 */
	IPX_ET_UNASSIGNED = 255
};

/**
 * \enum IPX_ELEMENT_SEMANTIC
 * \brief IPFIX Element semantic type
 *
 * This enumeration describes the set of valid data type semantics of the
 * IPFIX information model. Further data type semantics may be specified by
 * future updates to this enumeration.
 * \remark Based on RFC 7012, Section 3.2 and RFC 6313, Section 11.2
 */
enum IPX_ELEMENT_SEMANTIC {
	/**
	 * No semantics apply to the field. It cannot be manipulated by
	 * a Collecting Process or File Reader that does not understand it a priori.
	 */
	IPX_ES_DEFAULT = 0,

	/**
	 * A numeric (integral or floating point) value representing a measured
	 * value pertaining to the record. This is the default semantic type of all
	 * numeric data types.
	 */
	IPX_ES_QUANTITY,

	/**
	 * An integral value reporting the value of a counter. Counters are
	 * unsigned and wrap back to zero after reaching the limit of the type.
	 * A total counter counts independently of the export of its value.
	 */
	IPX_ES_TOTAL_COUNTER,

	/**
	 * An integral value reporting the value of a counter. Counters are
	 * unsigned and wrap back to zero after reaching the limit of the type.
	 * A delta counter is reset to 0 each time it is exported and/or expires
	 * without export.
	 */
	IPX_ES_DELTA_COUNTER,

	/**
	 * An integral value that serves as an identifier. Identifiers MUST be
	 * one of the signed or unsigned data types.
	 */
	IPX_ES_IDENTIFIER,

	/**
	 * An integral value that represents a set of bit fields. Flags MUST
	 * always be of an unsigned data type.
	 */
	IPX_ES_FLAGS,

	/**
	 * A list is a structured data type, being composed of a sequence of
	 * elements, e.g., Information Element, Template Record.
	 */
	IPX_ES_LIST,

	/**
	 * An unassigned sematic type (invalid value).
	 */
	IPX_ES_UNASSIGNED = 255
};

/**
 * \enum IPX_ELEMENT_UNIT
 * \brief IPFIX data unit
 *
 * A description of the units of an IPFIX Information Element. Further data
 * units may be specified by future updates to this enumeration.
 */
enum IPX_ELEMENT_UNIT {
	/** The type represents a unitless field.                                 */
	IPX_EU_NONE = 0,
	/** THe type represents a number of bits                                  */
	IPX_EU_BITS,
	/** The type represents a number of octets (bytes)                        */
	IPX_EU_OCTETS,
	/** The type represents a number of packets                               */
	IPX_EU_PACKETS,
	/** The type represents a number of flows                                 */
	IPX_EU_FLOWS,
	/** The type represents a time value in seconds.                          */
	IPX_EU_SECONDS,
	/** The type represents a time value in milliseconds.                     */
	IPX_EU_MILLISECONDS,
	/** The type represents a time value in microseconds.                     */
	IPX_EU_MICROSECONDS,
	/** The type represents a time value in nanoseconds.                      */
	IPX_EU_NANOSECONDS,
	/** The type represents a length in units of 4 octets (e.g. IPv4 header). */
	IPX_EU_4_OCTET_WORDS,
	/** The type represents a number of IPFIX messages (e.g. for reporing).   */
	IPX_EU_MESSAGES,
	/** The type represents a TTL (Time to Live) value                        */
	IPX_EU_HOPS,
	/** The type represents a number of labels in the MPLS stack              */
	IPX_EU_ENTRIES,
	/** The type represents a number of L2 frames                             */
	IPX_EU_FRAMES,
	/** An unassigned unit type (invalid value)                               */
	IPX_EU_UNASSIGNED = 65535
};

/**
 * \brief IPFIX Element definition
 */
struct ipx_element {
	/** Element ID                                                         */
	uint16_t id;
	/** Enterprise ID                                                      */
	uint32_t en;
	/** Name of the element                                                */
	char    *name;
	/**
	 * Abstract data type. Intend only to define the values which can be
	 * taken by the element.
	 * \warning Do NOT represent a size of the record!
	 */
	enum IPX_ELEMENT_TYPE     data_type;
	/** Data semantic                                                      */
	enum IPX_ELEMENT_SEMANTIC data_semantic;
	/** Data unit                                                          */
	enum IPX_ELEMENT_UNIT     data_unit;
};

/**@}*/

#endif /* _IPFIX_ELEMENTS_ */


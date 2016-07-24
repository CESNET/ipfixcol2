/**
 * \file convertors.h
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Conversion functions for IPFIX data types
 * \date 2016
 */
/*
 * Copyright (C) 2016 CESNET, z.s.p.o.
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
 * This software is provided ``as is``, and any express or implied
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

#ifndef _IPX_CONVERTORS_H_
#define _IPX_CONVERTORS_H_

#include <stddef.h>    // size_t
#include <stdint.h>    // uintXX_t, intXX_t
#include <stdbool.h>   // bool
#include <time.h>      // struct timespec
#include <string.h>    // memcpy
#include <float.h>     // float min/max
#include <assert.h>    // static_assert
#include <arpa/inet.h> // inet_ntop
#include <endian.h>    // htobe64, be64toh

#include <ipfixcol2/ipfix_element.h>

/**
 * \defgroup ipx_convertors Data conversion
 * \brief Conversion "from" and "to" data types used in IPFIX messages
 * \remark Based on RFC 7011, Section 6.
 * (see https://tools.ietf.org/html/rfc7011#section-6)
 *
 * @{
 */

// Check byte order
#if (__BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__ \
		&& __BYTE_ORDER__ != __ORDER_BIG_ENDIAN__)
#error Unsupported endianness type of the machine.
#endif

// Check the size of double and float
static_assert(sizeof(double) == sizeof(uint64_t), "Double is not 8 bytes long");
static_assert(sizeof(float)  == sizeof(uint32_t), "Float is not 4 bytes long");

/**
 * \def IPX_CONVERT_ERR_ARG
 * \brief Status code for invalid argument(s) of a conversion function
 */
#define IPX_CONVERT_ERR_ARG     (-1)

/**
 * \def IPX_CONVERT_ERR_TRUNC
 * \brief Status code for a truncation of a value of an argument of a conversion
 *   function.
 */
#define IPX_CONVERT_ERR_TRUNC   (-2)

/**
 * \def IPX_CONVERT_ERR_BUFFER
 * \brief Status code for insufficient buffer size for a result of a conversion
 *   function
 */
#define IPX_CONVERT_ERR_BUFFER  (-3)


/**
 * \def IPX_CONVERT_EPOCHS_DIFF
 * \brief Time difference between NTP and UNIX epoch in seconds
 *
 * NTP epoch (1 January 1900, 00:00h) vs. UNIX epoch (1 January 1970 00:00h) 
 * i.e. ((70 years * 365 days) + 17 leap-years) * 86400 seconds per day
 */
#define IPX_CONVERT_EPOCHS_DIFF (2208988800ULL)


/**
 * \defgroup ipx_setters Value setters
 * \brief Convert and write a value to a field of an IPFIX record
 * @{
 */

/**
 * \brief Set a value of an unsigned integer
 *
 * The \p value is converted from "host byte order" to "network byte order" and
 * stored to a data \p field.
 * \param[out] field  Pointer to the data field
 * \param[in]  size   Size of the data field (min: 1 byte, max: 8 bytes)
 * \param[in]  value  New value
 * \return On success returns 0. When the \p value cannot fit in the \p field of
 *   the defined \p size, stores a saturated value and returns the value
 *   #IPX_CONVERT_ERR_TRUNC. When the \p size is out of range, a value of
 *   the \p field is unchanged and returns #IPX_CONVERT_ERR_ARG.
 */
static inline int ipx_set_uint(void *field, size_t size, uint64_t value)
{
	switch (size) {
	case 8:
		*((uint64_t *) field) = htobe64(value);
		return 0;

	case 4:
		if (value > UINT32_MAX) {
			*((uint32_t *) field) = UINT32_MAX; // byte conversion not required
			return IPX_CONVERT_ERR_TRUNC;
		}

		*((uint32_t *) field) = htonl(value);
		return 0;

	case 2:
		if (value > UINT16_MAX) {
			*((uint16_t *) field) = UINT16_MAX; // byte conversion not required
			return IPX_CONVERT_ERR_TRUNC;
		}

		*((uint16_t *) field) = htons(value);
		return 0;

	case 1:
		if (value > UINT8_MAX) {
			*((uint8_t *) field) = UINT8_MAX;
			return IPX_CONVERT_ERR_TRUNC;
		}

		*((uint8_t *) field) = value;
		return 0;

	default:
		// Other sizes (3,5,6,7)
		break;
	}

	if (size == 0 || size > 8) {
		return IPX_CONVERT_ERR_ARG;
	}

	const uint64_t over_limit = 1ULL << (size * 8);
	if (value >= over_limit) {
		value = UINT64_MAX; // byte conversion not required (all bits set)
		memcpy(field, &value, size);
		return IPX_CONVERT_ERR_TRUNC;
	}

	value = htobe64(value);	
	memcpy(field, &(((uint8_t *) &value)[8U - size]), size);
	return 0;
}

/**
 * \brief Set a value of a signed integer
 *
 * The \p value is converted from "host byte order" to "network byte order" and
 * stored to a data \p field.
 * \param[out] field  Pointer to the data field
 * \param[in]  size   Size of the data field (min: 1 byte, max: 8 bytes)
 * \param[in]  value  New value
 * \return On success returns 0. When the \p value cannot fit in the \p field of
 *   the defined \p size, stores a saturated value and returns the value
 *   #IPX_CONVERT_ERR_TRUNC. When the \p size is out of range, a value of
 *   the \p field is unchanged and returns #IPX_CONVERT_ERR_ARG.
 */
static inline int ipx_set_int(void *field, size_t size, int64_t value)
{
	switch (size) {
	case 8:
		*((int64_t *) field) = (int64_t) htobe64(value);
		return 0;

	case 4:
		if (value > INT32_MAX) {
			*((int32_t *) field) = (int32_t) htonl(INT32_MAX);
			return IPX_CONVERT_ERR_TRUNC;
		}
		
		if (value < INT32_MIN) {
			*((int32_t *) field) = (int32_t) htonl(INT32_MIN);
			return IPX_CONVERT_ERR_TRUNC;
		}

		*((int32_t *) field) = (int32_t) htonl(value);
		return 0;

	case 2:
		if (value > INT16_MAX) {
			*((int16_t *) field) = (int16_t) htons(INT16_MAX);
			return IPX_CONVERT_ERR_TRUNC;
		}

		if (value < INT16_MIN) {
			*((int16_t *) field) = (int16_t) htons(INT16_MIN);
			return IPX_CONVERT_ERR_TRUNC;
		}

		*((uint16_t *) field) = (int16_t) htons(value);
		return 0;

	case 1:
		if (value > INT8_MAX) {
			*((int8_t *) field) = INT8_MAX;
			return IPX_CONVERT_ERR_TRUNC;
		}
	
		if (value < INT8_MIN) {
			*((int8_t *) field) = INT8_MIN;
			return IPX_CONVERT_ERR_TRUNC;
		}

		*((int8_t *) field) = (int8_t) value;
		return 0;

	default:
		// Other sizes (3,5,6,7)
		break;
	}

	if (size == 0 || size > 8) {
		return IPX_CONVERT_ERR_ARG;
	}

	const int64_t over_limit = (INT64_MAX >> ((8 - size) * 8));
	bool over = false;

	if (value > over_limit) {
		value = htobe64(over_limit);
		over = true;
	} else if (value < ~over_limit) {
		value = htobe64(~over_limit);
		over = true;
	} else {
		value = htobe64(value);
	}

	memcpy(field, &(((int8_t *) &value)[8U - size]), size);
	return over ? IPX_CONVERT_ERR_TRUNC : 0;
}

/**
 * \brief Set a value of a boolean
 * \param[out] field  A pointer to a data field
 * \param[in]  size   Size of the data field (MUST be always 1 byte)
 * \param[in]  value  New value
 * \remark A size of the field is always consider as 1 byte.
 * \return On success returns 0. Otherwise (usually incorrect \p size of the
 *   field) returns #IPX_CONVERT_ERR_ARG and an original value of the \p field
 *   is unchanged.
 */
static inline int ipx_set_bool(void *field, size_t size, bool value)
{
	if (size != 1) {
		return IPX_CONVERT_ERR_ARG;
	}
	
	if (value) {
		// True
		*(uint8_t *) field = 1;
	} else {
		// False (according to the RFC 7011, section 6.1.5. is "false" == 2)
		*(uint8_t *) field = 2;
	}
	
	return 0;
}

/**
 * \brief Set a value of a float/double
 *
 * The \p value is converted from "host byte order" to "network by order" and
 * stored to a data \p field.
 * \param[out] field  Pointer to the data field
 * \param[in]  size   Size of tha data field (4 or 8 bytes)
 * \param[in]  value  New value
 * \return On success returns 0. When the \p value cannot fit in the \p field of
 *   the defined \p size, stores a saturated value and returns the value
 *   #IPX_CONVERT_ERR_TRUNC. When the \p size of the field is not valid,
 *   returns #IPX_CONVERT_ERR_ARG and an original value of the \p field is
 *   unchanged.
 */
static inline int ipx_set_float(void *field, size_t size, double value)
{
	if (size == sizeof(uint64_t)) {
		// 64 bits
		*(uint64_t *) field = htobe64(*((uint64_t *) &value));
		return 0;
		
	} else if (size == sizeof(uint32_t)) {
		// 32 bits
		float new_value;
		bool over = false;
		
		if (value < FLT_MIN) {
			new_value = FLT_MIN;
			over = true;
		} else if (value > FLT_MAX) {
			new_value = FLT_MAX;
			over = true;
		} else {
			new_value = value;
		}
		
		*(uint32_t *) field = htonl(*((uint32_t *) &new_value));
		return over ? IPX_CONVERT_ERR_TRUNC : 0;
		
	} else {
		return IPX_CONVERT_ERR_ARG;
	}
}

/**
 * \brief Set a value of a timestamp (low precision)
 *
 * The result stored to a \p field will be converted to "network by order".
 * \param[out] field  Pointer to a data field
 * \param[in]  size   Size of the data field
 * \param[in]  type   Type of the timestamp (see the remark)
 * \param[in]  value  Number of milliseconds since the UNIX epoch
 * \remark The parameter \p type can be only one of the following types:
 *   #IPX_ET_DATE_TIME_SECONDS, #IPX_ET_DATE_TIME_MILLISECONDS,
 *   #IPX_ET_DATE_TIME_MICROSECONDS, #IPX_ET_DATE_TIME_NANOSECONDS
 * \warning The \p size of the \p field MUST be 4 bytes
 *   (#IPX_ET_DATE_TIME_SECONDS) or 8 bytes (#IPX_ET_DATE_TIME_MILLISECONDS,
 *   #IPX_ET_DATE_TIME_MICROSECONDS, #IPX_ET_DATE_TIME_NANOSECONDS)
 * \warning Wraparound for dates after 8 February 2036 is not implemented.
 * \return On success returns 0. Otherwise (usually incorrect \p size of the
 *   field) returns #IPX_CONVERT_ERR_ARG and an original value of the \p field
 *   is unchanged.
 */
static inline int ipx_set_date_lp(void *field, size_t size, 
	enum IPX_ELEMENT_TYPE type, uint64_t value)
{
	// One second to milliseconds
	const uint64_t S1E3 = 1000ULL;
	
	if (size != sizeof(uint64_t) 
			&& (size != sizeof(uint32_t) || type != IPX_ET_DATE_TIME_SECONDS)) {
		return IPX_CONVERT_ERR_ARG;
	}

	switch (type) {
	case IPX_ET_DATE_TIME_SECONDS:
		*(uint32_t *) field = htonl(value / S1E3); // To seconds
		return 0;
		
	case IPX_ET_DATE_TIME_MILLISECONDS:
		*(uint64_t *) field = htobe64(value);
		return 0;
	
	case IPX_ET_DATE_TIME_MICROSECONDS:
	case IPX_ET_DATE_TIME_NANOSECONDS: {
		// Conversion from UNIX timestamp to NTP 64bit timestamp
		uint32_t (*part)[2] = field;
		
		// Seconds
		(*part)[0] = htonl((value / S1E3) + IPX_CONVERT_EPOCHS_DIFF);
		
		/*
		 * Fraction of second (1 / 2^32)
		 * The "value" uses 1/1000 sec as unit of subsecond fractions and NTP
		 * uses 1/(2^32) sec as its unit of fractional time.
		 * Conversion is easy: First, multiply by 2^32, then divide by 1e3.
		 * Warning: Calculation must use 64bit variable!!!
		 */
		uint32_t fraction = ((value % S1E3) << 32) / S1E3;
		if (type == IPX_ET_DATE_TIME_MICROSECONDS) {
			fraction &= 0xFFFFF800; // Make sure that last 11 bits are zeros
		}
		
		(*part)[1] = htonl(fraction);
		}

		return 0;
	default:
		return IPX_CONVERT_ERR_ARG;
	}
}

/**
 * \brief Set a value of a timestamp (high precision)
 *
 * The result stored to a \p field will be converted to "network by order".
 * \param[out] field  Pointer to a data field
 * \param[in]  size   Size of the data field
 * \param[in]  type   Type of the timestamp (see ipx_set_date_lr())
 * \param[in]  ts     Number of seconds and nanoseconds (see time.h)
 * \warning Wraparound for dates after 8 February 2036 is not implemented.
 * \warning The value of the nanoseconds field MUST be in the range 0 
 *   to 999999999.
 * \return On success returns 0. Otherwise (usually incorrect \p size of the
 *   field) returns #IPX_CONVERT_ERR_ARG and an original value of the \p field
 *   is unchanged.
 */
static inline int ipx_set_date_hp(void *field, size_t size,
	enum IPX_ELEMENT_TYPE type, struct timespec ts)
{
	const uint64_t S1E3 = 1000ULL;       // One second to milliseconds
	const uint64_t S1E6 = 1000000ULL;    // One second to microseconds
	const uint64_t S1E9 = 1000000000ULL; // One second to nanoseconds

	if ((uint64_t) ts.tv_nsec >= S1E9) {
		return IPX_CONVERT_ERR_ARG;
	}
	
	if (size != sizeof(uint64_t)
			&& (size != sizeof(uint32_t) || type != IPX_ET_DATE_TIME_SECONDS)) {
		return IPX_CONVERT_ERR_ARG;
	}

	switch (type) {
	case IPX_ET_DATE_TIME_SECONDS:
		*(uint32_t *) field = htonl(ts.tv_sec); // To seconds
		return 0;
		
	case IPX_ET_DATE_TIME_MILLISECONDS:
		*(uint64_t *) field = htobe64((ts.tv_sec * S1E3) + (ts.tv_nsec / S1E6));
		return 0;
	
	case IPX_ET_DATE_TIME_MICROSECONDS:
	case IPX_ET_DATE_TIME_NANOSECONDS: {
		// Conversion from UNIX timestamp to NTP 64bit timestamp
		uint32_t (*parts)[2] = field;
		
		// Seconds
		(*parts)[0] = htonl((uint32_t) ts.tv_sec + IPX_CONVERT_EPOCHS_DIFF);
		
		/*
		 * Fraction of second (1 / 2^32)
		 * The "ts" uses 1/1e9 sec as unit of subsecond fractions and NTP
		 * uses 1/(2^32) sec as its unit of fractional time.
		 * Conversion is easy: First, multiply by 2^32, then divide by 1e9.
		 * Warning: Calculation must use 64bit variable!!!
		 */
		uint32_t fraction = (((uint64_t) ts.tv_nsec) << 32) / S1E9;
		if (type == IPX_ET_DATE_TIME_MICROSECONDS) {
			fraction &= 0xFFFFF800; // Make sure that last 11 bits are zeros
		}
		
		(*parts)[1] = htonl(fraction);
		}

		return 0;
	default:
		return IPX_CONVERT_ERR_ARG;
	}
}

/**
 * \brief Set a value of an IP address (IPv4/IPv6)
 *
 *
 * \param[out] field  Pointer to a data field
 * \param[in]  size   Size of the data field (MUST be 4 or 16 bytes!)
 * \param[in]  value  Pointer to a new value of the field
 * \warning The \p value MUST be at least \p size bytes long.
 * \return On sucess returns 0. Otherwise (usually incorrect \p size of the
 *   field) returns #IPX_CONVERT_ERR_ARG and an original value of the \p field
 *   is unchanged.
 */
inline int ipx_set_ip(void *field, size_t size, const void *value)
{
	// todo
}

/**
 * \brief Set a value of a MAC address
 * \param[out] field  Pointer to a data field
 * \param[in]  size   Size of the data field (MUST be always 6 bytes!)
 * \param[in]  value  Pointer to a new value of the field
 * \warning The \p value is left in the original byte order
 * \warning The \p value MUST be at least \p size bytes long.
 * \return On sucess returns 0. Otherwise (usually incorrect \p size of the
 *   field) returns #IPX_CONVERT_ERR_ARG and an original value of the \p field
 *   is unchanged.
 */
inline int ipx_set_mac(void *field, size_t size, const void *value)
{
	// todo
}

/**
 * \brief Set a value of an octet array
 *
 * \param[out] field  Pointer to a data field
 * \param[in]  size   Size of the data field
 * \param[in]  value  Pointer to a new value of the field
 * \remark Can be implemented as a wrapper over memcpy.
 * \warning The \p value is left in the original byte order
 * \warning The \p value MUST be at least \p size bytes long.
 * \return On sucess returns 0. Otherwise returns #IPX_CONVERT_ERR_ARG.
 */
inline int ipx_set_octet_array(void *field, size_t size, const void *value)
{
	// todo
}

/**
 * \brief Set a value of a string
 *
 * \param[out] field  Pointer to a data field
 * \param[in]  size   Size of the data field
 * \param[in]  value  Pointer to a new value of the field
 * \remark Can be implemented as a wrapper over memcpy.
 * \warning The \p value MUST be at least \p size bytes long.
 * \return On sucess returns 0. Otherwise returns #IPX_CONVERT_ERR_ARG.
 */
inline int ipx_set_string(void *field, size_t size, const char *value)
{
	// todo
}


/**
 * @}
 *
 * \defgroup ipx_getters Value getters
 * \brief Read and convert a value from a field of an IPFIX record
 * @{
 */


/**
 * \brief Get a value of an unsigned integer
 *
 * The \p value is read from a data \p field and converted from
 * "network byte order" to "host byte order".
 * \param[in]  field  Pointer to the data field (in "network byte order")
 * \param[in]  size   Size of the data field (min: 1 byte, max: 8 bytes)
 * \param[out] value  Pointer to a variable for the result
 * \return On success returns 0 and fills the \p value. Otherwise (usually
 *   the incorrect \p size of the field) returns #IPX_CONVERT_ERR_ARG
 *   and the \p value is not filled.
 */
static inline int ipx_get_uint(const void *field, size_t size, uint64_t *value)
{
	switch (size) {
	case 8:
		*value = be64toh(*(const uint64_t *) field);
		return 0;

	case 4:
		*value = ntohl(*(const uint32_t *) field);
		return 0;

	case 2:
		*value = ntohs(*(const uint16_t *) field);
		return 0;

	case 1:
		*value = *(const uint8_t *) field;
		return 0;

	default:
		// Other sizes (3,5,6,7)
		break;
	}

	if (size == 0 || size > 8) {
		return IPX_CONVERT_ERR_ARG;
	}
	
	uint64_t new_value = 0;
	memcpy(&(((uint8_t *) &new_value)[8U - size]), field, size);
	
	*value = be64toh(new_value);
	return 0;
}

/**
 * \brief Get a value of a signed integer
 *
 * The \p value is read from a data \p field and converted from
 * "network byte order" to "host byte order".
 * \param[in]  field  Pointer to the data field (in "network byte order")
 * \param[in]  size   Size of the data field (min: 1 byte, max: 8 bytes)
 * \param[out] value  Pointer to a variable for the result
 * \return On success returns 0 and fills the \p value. Otherwise (usually
 *   the incorrect \p size of the field) returns #IPX_CONVERT_ERR_ARG
 *   and the \p value is not filled.
 */
static inline int ipx_get_int(const void *field, size_t size, int64_t *value)
{
	switch (size) {
	case 8:
		*value = (int64_t) be64toh(*(const uint64_t *) field);
		return 0;

	case 4:
		*value = (int32_t) ntohl(*(const uint32_t *) field);
		return 0;

	case 2:
		*value = (int16_t) ntohs(*(const uint16_t *) field);
		return 0;

	case 1:
		*value = *(const int8_t *) field;
		return 0;

	default:
		// Other sizes (3,5,6,7)
		break;
	}

	if (size == 0 || size > 8) {
		return IPX_CONVERT_ERR_ARG;
	}
	
	/*
	 * Sign extension
	 * The value is in network-byte-order therefore first bit determines
	 * the sign of the number. If first bit is set, prepare a new value with
	 * all bits set (-1). Otherwise with all bits unset (0).
	 */
	int64_t new_value = ((*(const uint8_t *) field) & 0x80) ? (-1) : 0;
	memcpy(&(((int8_t *) &new_value)[8U - size]), field, size);

	*value = be64toh(new_value);
	return 0;
}

/**
 * \brief Get a value of a boolean
 * \param[in]  field  Pointer to a data field
 * \param[in]  size   Size of the data field (MUST be always 1 byte)
 * \param[out] value  Pointer to a variable for the result
 * \remark A size of the field is always consider as 1 byte.
 * \return On success returns 0 and fills the \p value. Otherwise (usually
 *   invalid boolean value - see the definition of the boolean for IPFIX,
 *   RFC 7011, Section 6.1.5.) returns #IPX_CONVERT_ERR_ARG and 
 *   the \p value is not filled.
 */
static inline int ipx_get_bool(const void *field, size_t size, bool *value)
{
	if (size != 1) {
		return IPX_CONVERT_ERR_ARG;
	}
	
	switch (*(const uint8_t *) field) {
	case 1: // True
		*value = true;
		return 0;
	case 2: // False (according to the RFC 7011, section 6.1.5. is "false" == 2)
		*value = false;
		return 0;
	default:
		return IPX_CONVERT_ERR_ARG;
	}	
}

/**
 * \brief Get a value of a float/double
 *
 * The \p value is read from a data \p field and converted from
 * "network byte order" to "host byte order".
 * \param[in]  field  Pointer to the data field (in "network byte order")
 * \param[in]  size   Size of the data field (min: 1 byte, max: 8 bytes)
 * \param[out] value  Pointer to a variable for the result
 * \return On success returns 0 and fills the \p value. Otherwise (usually
 *   the incorrect \p size of the field) returns #IPX_CONVERT_ERR_ARG and the
 *   \p value is not filled.
 */
static inline int ipx_get_float(const void *field, size_t size, double *value)
{
	if (size == sizeof(uint64_t)) {
		// 64bit
		*(uint64_t *) value = be64toh(*(const uint64_t *) field);
		return 0;

	} else if (size == sizeof(uint32_t)) {
		// 32bit
		float new_value;
		*((uint32_t *) &new_value) = ntohl(*(const uint32_t *) field);
		*value = new_value;
		return 0;
		
	} else {
		return IPX_CONVERT_ERR_ARG;
	}
}

/**
 * \brief Get a value of a timestamp (low precision)
 *
 * The \p value is read from a data \p field, converted from
 * "network byte order" to "host byte order" and transformed to a corresponding
 * data type.
 * \param[in]  field  Pointer to the data field (in "network byte order")
 * \param[in]  size   Size of the data field (in bytes)
 * \param[in]  type   Type of the timestamp (see the remark)
 * \param[out] value  Pointer to a variable for the result (Number of
 *   milliseconds since the UNIX epoch)
 * \remark The parameter \p type can be only one of the following types:
 *   #IPX_ET_DATE_TIME_SECONDS, #IPX_ET_DATE_TIME_MILLISECONDS,
 *   #IPX_ET_DATE_TIME_MICROSECONDS, #IPX_ET_DATE_TIME_NANOSECONDS
 * \warning The \p size of the \p field MUST be 4 bytes
 *   (#IPX_ET_DATE_TIME_SECONDS) or 8 bytes (#IPX_ET_DATE_TIME_MILLISECONDS,
 *   #IPX_ET_DATE_TIME_MICROSECONDS, #IPX_ET_DATE_TIME_NANOSECONDS)
 * \warning Wraparound for dates after 8 February 2036 is not implemented.
 * \return On success returns 0 and fills the \p value. Otherwise (usually
 *   the incorrect \p size of the field) returns #IPX_CONVERT_ERR_ARG and the
 *   \p value is not filled.
 */
inline int ipx_get_date_lp(const void *field, size_t size,
	enum IPX_ELEMENT_TYPE type, uint64_t *value)
{
	// One second to milliseconds
	const uint64_t S1E3 = 1000ULL;	

	if (size != sizeof(uint64_t) 
			&& (size != sizeof(uint32_t) || type != IPX_ET_DATE_TIME_SECONDS)) {
		return IPX_CONVERT_ERR_ARG;
	}
	
	switch (type) {
	case IPX_ET_DATE_TIME_SECONDS:
		*value = ntohl(*(const uint32_t *) field) * S1E3;
		return 0;
		
	case IPX_ET_DATE_TIME_MILLISECONDS:
		*value = be64toh(*(const uint64_t *) field);
		return 0;

	case IPX_ET_DATE_TIME_MICROSECONDS:
	case IPX_ET_DATE_TIME_NANOSECONDS: {
		// Conversion from NTP 64bit timestamp to UNIX timestamp
		const uint32_t (*parts)[2] = (void *) field; // Ugly :(
		uint64_t result;
		
		// Seconds
		result = (ntohl((*parts)[0]) - IPX_CONVERT_EPOCHS_DIFF) * S1E3;
		
		/*
		 * Fraction of second (1 / 2^32)
		 * The "value" uses 1/1000 sec as unit of subsecond fractions and NTP
		 * uses 1/(2^32) sec as its unit of fractional time.
		 * Conversion is easy: First, multiply by 1e3, then divide by 2^32.
		 * Warning: Calculation must use 64bit variable!!!
		 */
		uint64_t fraction = ntohl((*parts)[1]);
		if (type == IPX_ET_DATE_TIME_MICROSECONDS) {
			fraction &= 0xFFFFF800; // Make sure that last 11 bits are zeros
		}
		
		result += (fraction * S1E3) >> 32;
		*value = result;
		}
		
		return 0;
	default:
		return IPX_CONVERT_ERR_ARG;
	}
}

/**
 * \brief Get a value of a timestamp (high precision)
 *
 * The \p value is read from a data \p field, converted from
 * "network byte order" to "host byte order" and transformed to a corresponding
 * data type.
 * \param[in]  field  Pointer to the data field (in "network byte order")
 * \param[in]  size   Size of the data field (in bytes)
 * \param[in]  type   Type of the timestamp (see ipx_get_date_lp())
 * \param[out] ts     Pointer to a variable for the result (see time.h)
 * \warning Wraparound for dates after 8 February 2036 is not implemented.
 * \return On success returns 0 and fills the \p value. Otherwise (usually
 *   the incorrect \p size of the field) returns #IPX_CONVERT_ERR_ARG and the
 *   \p value is not filled.
 */
inline int ipx_get_date_hp(const void *field, size_t size,
	enum IPX_ELEMENT_TYPE type, struct timespec *ts)
{
	const uint64_t S1E3 = 1000ULL;       // One second to milliseconds
	const uint64_t S1E6 = 1000000ULL;    // One second to microseconds
	const uint64_t S1E9 = 1000000000ULL; // One second to nanoseconds

	if (size != sizeof(uint64_t) 
			&& (size != sizeof(uint32_t) || type != IPX_ET_DATE_TIME_SECONDS)) {
		return IPX_CONVERT_ERR_ARG;
	}

	switch (type) {
	case IPX_ET_DATE_TIME_SECONDS:
		ts->tv_sec = ntohl(*(const uint32_t *) field);
		ts->tv_nsec = 0;
		return 0;
	
	case IPX_ET_DATE_TIME_MILLISECONDS: {
		const uint64_t new_value = be64toh(*(const uint64_t *) field);
		ts->tv_sec =  new_value / S1E3;
		ts->tv_nsec = (new_value % S1E3) * S1E6;
		}
		
		return 0;
		
	case IPX_ET_DATE_TIME_MICROSECONDS:
	case IPX_ET_DATE_TIME_NANOSECONDS: {
		// Conversion from NTP 64bit timestamp to UNIX timestamp
		const uint32_t (*parts)[2] = (void *) field; // Ugly :(
		
		// Seconds
		ts->tv_sec = ntohl((*parts)[0]) - IPX_CONVERT_EPOCHS_DIFF;
		
		/*
		 * Fraction of second (1 / 2^32)
		 * The "ts" uses 1/1e9 sec as unit of subsecond fractions and NTP
		 * uses 1/(2^32) sec as its unit of fractional time.
		 * Conversion is easy: First, multiply by 1e9, then divide by 2^32.
		 * Warning: Calculation must use 64bit variable!!!
		 */
		uint64_t fraction = ntohl((*parts)[1]);
		if (type == IPX_ET_DATE_TIME_MICROSECONDS) {
			fraction &= 0xFFFFF800; // Make sure that last 11 bits are zeros
		}
		
		ts->tv_nsec = (fraction * S1E9) >> 32;
		}
	
		return 0;
	default:
		return IPX_CONVERT_ERR_ARG;
	}
}

/**
 * \brief Get a value of an IP address (IPv4 or IPv6)
 *
 * The \p value is left in the original byte order (i.e. network byte order)
 * \param[in]  field  Pointer to a data field
 * \param[in]  size   Size of the data field (MUST be 4 or 16 bytes!)
 * \param[out] value  Pointer to a variable for the result
 * \warning The \p value MUST be at least \p size bytes long.
 * \return On success returns 0 and fills the \p value. Otherwise (usually
 *   the incorrect \p size of the field) returns #IPX_CONVERT_ERR_ARG and the
 *   \p value is not filled.
 */
inline int ipx_get_ip(const void *field, size_t size, void *value)
{
	// todo
}

/**
 * \brief Get a value of a MAC address (IPv4 or IPv6)
 *
 * The \p value is left in the original byte order (i.e. network byte order)
 * \param[in]  field  Pointer to a data field
 * \param[in]  size   Size of the data field (MUST always 6 bytes!)
 * \param[out] value  Pointer to a variable for the result
 * \warning The \p value MUST be at least \p size bytes long.
 * \return On success returns 0 and fills the \p value. Otherwise (usually
 *   the incorrect \p size of the field) returns #IPX_CONVERT_ERR_ARG and the
 *   \p value is not filled.
 */
inline int ipx_get_mac(const void *field, size_t size, void *value)
{
	// todo
}

/**
 * \brief Get a value of an octet array
 *
 * \param[in]  field  Pointer to a data field
 * \param[in]  size   Size of the data field
 * \param[out] value  Pointer to the output buffer
 * \remark Can be implemented as a wrapper over memcpy.
 * \warning The \p value MUST be at least \p size bytes long.
 * \return On success returns 0. Otherwise returns #IPX_CONVERT_ERR_ARG.
 */
inline int ipx_get_octet_array(const void *field, size_t size, void *value)
{
	// todo
}

/**
 * \brief Get a value of a string
 *
 * \param[in]  field  Pointer to a data field
 * \param[in]  size   Size of the data field
 * \param[out] value  Pointer to the output buffer
 * \remark Can be implemented as a wrapper over memcpy.
 * \warning The \p value MUST be at least \p size bytes long.
 * \return On success returns 0. Otherwise returns #IPX_CONVERT_ERR_ARG.
 */
inline int ipx_get_string(const void *field, size_t size, char *value)
{
	// todo
}


/**
 * @}
 *
 * \defgroup ipx_to_string To string
 * \brief Read and convert a value from a field of an IPFIX record to
 * a character string
 * @{
 */

/**
 * \def IPX_CONVERT_STRLEN_MAC
 * \brief Minimal size of an output buffer for any MAC address conversion
 */
#define IPX_CONVERT_STRLEN_MAC (18) // 2 * 6 groups + 5 colons + 1 * '\0'

/**
 * \def IPX_CONVERT_STRLEN_IP
 * \brief Minimal size of an output buffer for any IP address conversion
 */
#define IPX_CONVERT_STRLEN_IP (INET6_ADDRSTRLEN) // Usually 48 bytes

/**
 * \def IPX_CONVERT_STRLEN_DATELP
 * \brief Minimum size of an output buffer for low precision time conversion
 */
#define IPX_CONVERT_STRLEN_DATELP (24)

/**
 * \def IPX_CONVERT_STRLEN_DATEHP
 * \brief Minimum size of an output buffer for high precision time conversion
 */
#define IPX_CONVERT_STRLEN_DATEHP (30)

/**
 * \brief Convert a value of an unsigned integer to a character string
 *
 * The \p value is read from a data \p field and converted from
 * "network byte order" to "host byte order" and converted to string.
 * \param[in]  field     Pointer to a data field (in "network byte order")
 * \param[in]  size      Size of the data field (in bytes)
 * \param[out] str       Pointer to a character buffer
 * \param[in]  str_size  Size of the buffer (in bytes)
 * \return On success returns a number of characters (excluding the termination
 *   null byte) placed into the buffer \p str. If the length of the result
 *   string (including the termination null byte) would exceed \p str_size,
 *   then returns #IPX_CONVERT_ERR_BUFFER and the content written to
 *   the buffer \p str is undefined. Otherwise returns #IPX_CONVERT_ERR_ARG.
 */
inline int ipx_uint2str(const void *field, size_t size, char *str,
	size_t str_size)
{
	// todo
}

/**
 * \brief Convert a value of a signed integer to a character string
 *
 * The \p value is read from a data \p field, converted from
 * "network byte order" to "host byte order" and converted to string.
 * \param[in]  field     Pointer to a data field (in "network byte order")
 * \param[in]  size      Size of the data field (in bytes)
 * \param[out] str       Pointer to a character buffer
 * \param[in]  str_size  Size of the buffer (in bytes)
 * \return Same as a return value of ipx_uint2str().
 */
inline int ipx_int2str(const void *field, size_t size, char *str,
	size_t str_size)
{
	// todo
}

/**
 * \brief Convert a value of a float/double to a character string
 *
 * The \p value is read from a data \p field, converted from
 * "network byte order" to "host byte order" and converted to string.
 * \param[in]  field     Pointer to the data field (in "network byte order")
 * \param[in]  size      Size of the data field (min: 1 byte, max: 8 bytes)
 * \param[out] str       Pointer to a character buffer
 * \param[in]  str_size  Size of the buffer (in bytes)
 * \remark The value is rounded to 3 decimal places.
 * \return Same as a return value of ipx_uint2str().
 */
inline int ipx_float2str(const void *field, size_t size, char *str,
	size_t str_size)
{
	// todo
}

/**
 * \brief Convert a boolean value to a character string
 * \param[in]  field      Pointer to a data field
 * \param[out] str        Pointer to a character buffer
 * \param[in]  str_size   Size of the buffer (in bytes)
 * \remark Output strings are "true" and "false".
 * \remark A size of the \p field is always consider as 1 byte.
 * \remark If the content of the \p field is invalid value, the function
 *   will also return #IPX_CONVERT_ERR_ARG.
 * \return Same as a return value of ipx_uint2str().
 */
inline int ipx_bool2str(const void *field, char *str, size_t str_size)
{
	// todo
}

/**
 * \brief Convert a value of a timestamp to a character string (low precision)
 *
 * The \p value is read from a data \p field, converted from
 * "network byte order" to "host byte order" and transformed to string.
 * \param[in]  field     Pointer to the data field (in "network byte order")
 * \param[in]  size      Size of the data field (in bytes)
 * \param[in]  type      Type of the timestamp (see the remark)
 * \param[out] str       Pointer to a character buffer
 * \param[in]  str_size  Size of the bufffer (in bytes)
 * \remark For more details about the parameter \p type see the documentation
 *    of ipx_get_date_lp().
 * \remark Output format is "%Y-%m-%dT%H:%M:%S.mmm", for example:
 *   "2016-06-22T08:15:00.000" (without quotation marks)
 * \remark The size of the output buffer (\p str_size) must be at least
 *   #IPX_CONVERT_STRLEN_DATELP bytes to guarantee enought size of conversion.
 * \warning Wraparound for dates after 8 February 2036 is not implemented.
 * \return Same as a return value of ipx_uint2str().
 */
inline int ipx_date_lp2str(const void *field, size_t size,
	enum IPX_ELEMENT_TYPE type, char *str, size_t str_size)
{
	// todo
}

/**
 * \brief Convert a value of a timestamp to a character string (high precision)
 *
 * The \p value is read from a data \p field, converted from
 * "network byte order" to "host byte order" and transformed to string.
 * \param[in]  field     Pointer to the data field (in "network byte order")
 * \param[in]  size      Size of the data field (in bytes)
 * \param[in]  type      Type of the timestamp (see the remark)
 * \param[out] str       Pointer to a character buffer
 * \param[in]  str_size  Size of the buffer (in bytes)
 * \remark For more details about the parameter \p type see the documentation
 *   of ipx_get_date_lp().
 * \remark Output format is "%Y-%m-%dT%H:%M:%S.nnnnnnnnn", for example:
 *   "2016-06-22T08:15:00.000000000" (without quotation marks).
 * \remark The size of the output buffer (\p str_size) must be at least
 *   #IPX_CONVERT_STRLEN_DATEHP bytes to guarantee enought size for conversion.
 * \warning Wraparound for dates after 8 February 2036 is not implemented.
 * \return Same as a return value of ipx_uint2str().
 */
inline int ipx_date_hp2str(const void *field, size_t size,
	enum IPX_ELEMENT_TYPE type, char *str, size_t str_size)
{
	// todo
}

/**
 * \brief Convert a value of a MAC address to a character string
 * \param[in]  field     Pointer to a data field
 * \param[out] str       Pointer to a character buffer
 * \param[in]  str_size  Size of the buffer (in bytes)
 * \remark A size of the \p field is always consider as 6 byte.
 * \remark Output format is six groups of hexadecimal digits separated by colon
 *   symbols (:), for example: "00:0a:bc:e0:12:34" (without quotation marks)
 * \remark The size of the output buffer (\p str_size) must be at least
 *   #IPX_CONVERT_STRLEN_MAC bytes to guarantee enought size for conversion.
 * \return Same as a return value of ipx_uint2str().
 */
inline int ipx_mac2str(const void *field, char *str, size_t str_size)
{
	// todo
}

/**
 * \brief Convert a value of an IP address (IPv4/IPv6) to a character string
 * \param[in]  field     Pointer to a data field
 * \param[in]  size      Size of the data field (4 or 16 bytes)
 * \param[out] str       Pointer to a character buffer
 * \param[in]  str_size  Size of the buffer (in bytes)
 * \remark The size of the output buffer (\p str_size) should be at least
 *   #IPX_CONVERT_STRLEN_IP bytes to guarantee enought size for conversion.
 * \return Same as a return value of ipx_uint2str().
 */
inline int ipx_ip2str(const void *field, size_t size, char *str,
	size_t str_size)
{
	// todo
}

/**
 * \brief Convert a value of an octet array to a character string
 * \param[in]  field     Pointer to a data field
 * \param[in]  size      Size of the data field
 * \param[out] str       Pointer to a character buffer
 * \param[in]  str_size  Size of the buffer (in bytes)
 * \remark Output format is "0xhh...", where "hh" is hexadecimal representation
 *   of each byte.
 * \remark Minimum size of the output buffer (\p str_size) must be at least
 *   (2 * \p size) + 3 ('0' + 'x' + '\0') bytes.
 * \return Same as a return value of ipx_uint2str().
 */
inline int ipx_octet_array2str(const void *field, size_t size, char *str,
	size_t str_size)
{
	// todo
}

/**
 * \brief Convert a value of a IPFIX string to a character string
 * \param[in]  field     Pointer to a data field
 * \param[in]  size      Size of the data field
 * \param[out] str       Pointer to a character buffer
 * \param[in]  str_size  Size of the buffer
 * \remark Characters representable by escape sequences are represented as
 *   particular escape sequences i.e. alert (\\a), backspace (\\b),
 *   formfeed (\\f), newline (\\n), carriage return (\\r), horizontal tab (\\t),
 *   vertical tab (\\v), backslash (\\\), single and double quotation mark
 *   (\\', \\"). Other non-printable charactes are replaces with \\xhh (where
 *   "hh" is a hexadecimal value).
 * \return Same as a return value of ipx_uint2str().
 */
inline int ipx_string2str(const void *field, size_t size, char *str,
	size_t str_size)
{
	// todo
}

#endif /* _IPX_CONVERTORS_H_ */

/**
 * @}
 * @}
 */


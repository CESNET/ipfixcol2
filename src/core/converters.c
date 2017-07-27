/**
 * \file src/core/converters.c
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Conversion functions for IPFIX data types (source code)
 * \date 2016-2017
 */

/*
 * Copyright (C) 2016-2017 CESNET, z.s.p.o.
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

#include <ipfixcol2.h>
#include <stdio.h>    // snprintf
#include <inttypes.h> // PRIi64, PRIu32,...

int
ipx_uint2str_be(const void *field, size_t size, char *str, size_t str_size)
{
	// Get a value
	uint64_t result;
	if (ipx_get_uint_be(field, size, &result) != IPX_CONVERT_OK) {
		return IPX_CONVERT_ERR_ARG;
	}

	// TODO: use something faster than snprintf
	int str_real_len = snprintf(str, str_size, "%" PRIu64, result);
	if (str_real_len < 0 || (size_t) str_real_len >= str_size) {
		return IPX_CONVERT_ERR_BUFFER;
	}

	return str_real_len;
}

int
ipx_int2str_be(const void *field, size_t size, char *str, size_t str_size)
{
	// Get a value
	int64_t result;
	if (ipx_get_int_be(field, size, &result) != IPX_CONVERT_OK) {
		return IPX_CONVERT_ERR_ARG;
	}

	// TODO: use something faster than snprintf
	int str_real_len = snprintf(str, str_size, "%" PRIi64, result);
	if (str_real_len < 0 || (size_t) str_real_len >= str_size) {
		return IPX_CONVERT_ERR_BUFFER;
	}

	return str_real_len;
}

int
ipx_float2str_be(const void *field, size_t size, char *str, size_t str_size)
{
	// Get a value
	double result;
	if (ipx_get_float_be(field, size, &result) != IPX_CONVERT_OK) {
		return IPX_CONVERT_ERR_ARG;
	}

	const char *fmt = (size == sizeof(float))
		? ("%." IPX_CONVERT_STRX(FLT_DIG) "g")  // float precision
		: ("%." IPX_CONVERT_STRX(DBL_DIG) "g"); // double precision
	int str_real_len = snprintf(str, str_size, fmt, result);
	if (str_real_len < 0 || (size_t) str_real_len >= str_size) {
		return IPX_CONVERT_ERR_BUFFER;
	}

	return str_real_len;
}

int
ipx_bool2str(const void *field, char *str, size_t str_size)
{
	bool result;
	if (ipx_get_bool(field, 1, &result) != 0) {
		return IPX_CONVERT_ERR_ARG;
	}

	const char *bool_str;
	size_t bool_len;

	if (result) {
		// True
		bool_str = IPX_CONVERT_STR_TRUE;
		bool_len = IPX_CONVERT_STRLEN_TRUE;
	} else {
		// False
		bool_str = IPX_CONVERT_STR_FALSE;
		bool_len = IPX_CONVERT_STRLEN_FALSE;
	}

	if (str_size < bool_len) {
		return IPX_CONVERT_ERR_BUFFER;
	}

	memcpy(str, bool_str, bool_len);
	return (int) (bool_len - 1); // Cast is OK. Max. size is length of "false".
}

int
ipx_datetime2str_be(const void *field, size_t size, enum ipx_element_type type,
	char *str, size_t str_size, enum ipx_convert_time_fmt fmt)
{
	struct timespec ts;
	if (ipx_get_datetime_hp_be(field, size, type, &ts) != IPX_CONVERT_OK) {
		return IPX_CONVERT_ERR_ARG;
	}

	// Convert common part
	struct tm utc_time;
	if (!gmtime_r(&ts.tv_sec, &utc_time)) {
		return IPX_CONVERT_ERR_ARG;
	}

	size_t size_used = strftime(str, str_size, "%FT%T", &utc_time);
	if (size_used == 0) {
		return IPX_CONVERT_ERR_BUFFER;
	}

	// Add the faction part
	uint32_t frac;
	int frac_width;

	switch (fmt) {
	case IPX_CONVERT_TF_SEC:
		return (int) size_used;
	case IPX_CONVERT_TF_MSEC:
		frac = (uint32_t) (ts.tv_nsec / 1000000U);
		frac_width = 3;
		break;
	case IPX_CONVERT_TF_USEC:
		frac = (uint32_t) (ts.tv_nsec / 1000U);
		frac_width = 6;
		break;
	case IPX_CONVERT_TF_NSEC:
		frac = (uint32_t) (ts.tv_nsec);
		frac_width = 9;
		break;
	default:
		return IPX_CONVERT_ERR_ARG;
	}

	size_t size_remain = size - size_used;
	char *str_remain = str + size_used;

	int ret = snprintf(str_remain, size_remain, ".%0*" PRIu32, frac_width, frac);
	if (ret >= (int) size_remain || ret < 0) {
		return IPX_CONVERT_ERR_BUFFER;
	}

	return (int) size_used + ret;
}

int
ipx_mac2str(const void *field, char *str, size_t str_size)
{
	// Get a copy of the MAC address
	uint8_t mac[6];
	if (ipx_get_mac(field, sizeof(mac), &mac) != IPX_CONVERT_OK) {
		return IPX_CONVERT_ERR_ARG;
	}

	// Convert to string
	int ret = snprintf(str, str_size, "%02X:%02X:%02X:%02X:%02X:%02X",
			mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	if (ret != IPX_CONVERT_STRLEN_MAC) {
		return IPX_CONVERT_ERR_ARG;
	}

	return IPX_CONVERT_STRLEN_MAC;
}

int
ipx_ip2str(const void *field, size_t size, char *str, size_t str_size)
{
	if (size == 4U) {
		if (!inet_ntop(AF_INET, field, str, str_size)) {
			return IPX_CONVERT_ERR_BUFFER;
		}

		return (int) strlen(str);
	}

	if (size == 16U) {
		if (!inet_ntop(AF_INET6, field, str, str_size)) {
			return IPX_CONVERT_ERR_BUFFER;
		}

		return (int) strlen(str);
	}

	return IPX_CONVERT_ERR_ARG;
}

int
ipx_octet_array2str(const void *field, size_t size, char *str, size_t str_size)
{
	size_t real_len = (2 * size) + 1; // 2 characters per byte + '\0'
	if (real_len > str_size) {
		return IPX_CONVERT_ERR_BUFFER;
	}

	const uint8_t *field_ptr = field;

	while (size > 0) {
		// Convert byte to a pair of hexadecimal digits
		uint8_t byte;
		byte = ((*field_ptr) & 0xF0) >> 4;
		str[0] = (byte < 10) ? ('0' + byte) : ('A' - 10 + byte);
		byte = (*field_ptr) & 0x0F;
		str[1] = (byte < 10) ? ('0' + byte) : ('A' - 10 + byte);

		str += 2;
		field_ptr++;
		size--;
	}

	*str = '\0';
	return (int) (real_len - 1); // Cast is OK (max. value is 2*(2^16))
}

/**
 * \brief Is a UTF-8 character valid
 * \param[in] str Pointer to the character beginning
 * \param[in] len Maximum length of the character (in bytes)
 * \note Parameter \p len is used to avoid access outside of the array's bounds.
 * \warning Value of the parameter \p len MUST be at least 1.
 * \return If the character is NOT valid, the function will return 0.
 *   Otherwise (only valid characters) returns length of the character i.e.
 *   number 1-4 (in bytes).
 */
static inline int
utf8char_is_valid(const uint8_t *str, size_t len)
{
	if ((str[0] & 0x80) == 0) {                // 0xxx xxxx
		// Do nothing...
		return 1;
	}

	if ((str[0] & 0xE0) == 0xC0 && len >= 2) { // 110x xxxx + 1 more byte
		// Check the second byte (must be 10xx xxxx)
		return ((str[1] & 0xC0) == 0x80) ? 2 : 0;
	}

	if ((str[0] & 0xF0) == 0xE0 && len >= 3) { // 1110 xxxx + 2 more bytes
		// Check 2 tailing bytes (each must be 10xx xxxx)
		uint16_t tail = *((const uint16_t *) &str[1]);
		return ((tail & 0xC0C0) == 0x8080) ? 3 : 0;
	}

	if ((str[0] & 0xF8) == 0xF0 && len >= 4) { // 1111 0xxx + 3 more bytes
		// Check 3 tailing bytes (each must be 10xx xxxx)
		uint32_t tail = *((const uint32_t *) &str[0]);
		// Change the first byte for easier comparision
		*(uint8_t *) &tail = 0x80; // Little/big endian compatible solution
		return ((tail & 0xC0C0C0C0) == 80808080) ? 4 : 0;
	}

	// Invalid character
	return 0;
}

/**
 * \brief Is it a UTF-8 control character
 * \param[in] str Pointer to the character beginning
 * \param[in] len Maximum length of the character (in bytes)
 * \note Parameter \p len is used to avoid access outside of the array's bounds.
 * \return True or false.
 */
static inline bool
utf8char_is_control(const uint8_t *str, size_t len)
{
	(void) len;
	// Check C0 control characters
	if (str[0] <= 0x1F || str[0] == 0x7F) {
		return true;
	}

	// Check C1 control characters
	if (str[0] >= 0x80 && str[0] <= 0x9F) {
		return true;
	}

	return false;
}

/**
 * \brief Is a UTF-8 character escapable
 * \param[in]  str  Pointer to the character beginning
 * \param[in]  len  Maximum length of the character (in bytes)
 * \param[out] repl Replacement character (can be NULL). The variable is filled
 *   only when the input character is escapable. The size of the character is
 *   always only one byte.
 * \note Parameter \p len is used to avoid access outside of the array's bounds.
 * \return True or false.
 */
static inline bool
utf8char_is_escapable(const uint8_t *str, size_t len, uint8_t *repl)
{
	(void) len;
	if ((str[0] & 0x80) != 0) {
		// Only 1 byte characters can be escapable (for now)
		return false;
	}

	uint8_t new_char;
	const char old_char = (char) *str;
	switch (old_char) {
	case '\n': new_char = 'n'; break;
	case '\r': new_char = 'r'; break;
	case '\t': new_char = 't'; break;
	case '\b': new_char = 'b'; break;
	case '\f': new_char = 'f'; break;
	case '\v': new_char = 'v'; break;
	default: return false;
	}

	if (repl != NULL) {
		*repl = new_char;
	}

	return true;
}

int
ipx_string2str(const void *field, size_t size, char *str, size_t str_size)
{
	if (size + 1 > str_size) { // "+1" for '\0'
		// The output buffer is definitely small
		return IPX_CONVERT_ERR_BUFFER;
	}

	unsigned int step;
	size_t pos_copy = 0; // Start of "copy" region
	uint8_t subst; // Replacement character for escapable characters

	const uint8_t *ptr_in = (const uint8_t *) field;
	uint8_t *ptr_out = (uint8_t *) str;
	size_t pos_out = 0; // Position in the output buffer

	for (size_t pos_in = 0; pos_in < size; pos_in += step) {
		const uint8_t *char_ptr = ptr_in + pos_in;
		const size_t char_max = size - pos_in; // Maximum character length

		int  is_valid =     utf8char_is_valid(char_ptr, char_max);
		bool is_escapable = utf8char_is_escapable(char_ptr, char_max, &subst);
		bool is_control =   utf8char_is_control(char_ptr, char_max);

		// Size of the current character
		step = (is_valid > 0) ? (unsigned int) is_valid : 1;

		if (is_valid && !is_escapable && !is_control) {
			// This character will be just copied
			continue;
		}

		// Interpretation of the character must be changed
		const size_t copy_len = pos_in - pos_copy;
		size_t out_remaining = str_size - pos_out;

		// Copy unchanged characters
		if (copy_len > out_remaining) {
			return IPX_CONVERT_ERR_BUFFER;
		}
		memcpy(&ptr_out[pos_out], &ptr_in[pos_copy], copy_len);
		out_remaining -= copy_len;
		pos_out += copy_len;
		pos_copy = pos_in + 1; // Next time start from the next character

		// Is it an escapable character?
		if (is_escapable) {
			const size_t subst_len = 2U;
			if (out_remaining < subst_len) {
				return IPX_CONVERT_ERR_BUFFER;
			}

			ptr_out[pos_out] = '\\';
			ptr_out[pos_out + 1] = subst;
			pos_out += subst_len;
			continue;
		}

		// Is it a control character?
		if (is_control) {
			const size_t subst_len = 4U;
			if (out_remaining < subst_len) {
				return IPX_CONVERT_ERR_BUFFER;
			}

			uint8_t hex;
			ptr_out[pos_out] = '\\';
			ptr_out[pos_out + 1] = 'x';

			hex = ((*char_ptr) & 0xF0) >> 4;
			ptr_out[pos_out + 2] = (hex < 10) ? ('0' + hex) : ('A' - 10 + hex);
			hex = (*char_ptr) & 0x0F;
			ptr_out[pos_out + 3] = (hex < 10) ? ('0' + hex) : ('A' - 10 + hex);
			pos_out += subst_len;
			continue;
		}

		// Invalid character -> replace with "REPLACEMENT CHARACTER"
		const size_t subst_len = 3U;
		if (out_remaining < subst_len) {
			return IPX_CONVERT_ERR_BUFFER;
		}

		// Character U+FFFD in UTF8 encoding
		ptr_out[pos_out] = 0xEF;
		ptr_out[pos_out + 1] = 0xBF;
		ptr_out[pos_out + 2] = 0xBD;
		pos_out += subst_len;
	}

	// Copy the rest and add termination symbol
	const size_t copy_len = size - pos_copy;
	const size_t out_remaining = str_size - pos_out;

	if (copy_len + 1 > out_remaining) { // "+1" for '\0'
		return IPX_CONVERT_ERR_BUFFER;
	}
	memcpy(&ptr_out[pos_out], &ptr_in[pos_copy], copy_len);
	pos_out += copy_len;
	ptr_out[pos_out] = '\0';
	return (int) pos_out;
}

int
ipx_string_utf8check(const void *field, size_t size)
{
	int step;

	for (size_t idx = 0; idx < size; idx += step) {
		const uint8_t *ptr = ((const uint8_t *) field) + idx;
		const size_t remaining = size - idx;

		step = utf8char_is_valid(ptr, remaining);
		if (step == 0) {
			return IPX_CONVERT_ERR_ARG;
		}
	}

	return IPX_CONVERT_OK;
}




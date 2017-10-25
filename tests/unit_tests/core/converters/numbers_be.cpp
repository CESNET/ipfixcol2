/**
 * \file tests/unit_tests/core/converters/numbers_be.cpp
 * \author Lukas Hutak <xhutak01@stud.fit.vutbr.cz>
 * \brief Converters tests
 */
/* Copyright (C) 2016-2017 CESNET, z.s.p.o.
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

/**
 * \defgroup ipx_converters_number_be_test Data conversion tests of big endian
 *   numeric functions
 *
 * \note In many cases, test functions use dynamically allocated variables,
 *   because it is useful for valgrind memory check (accessing an array out
 *   of bounds, etc.)
 * @{
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <endian.h>

extern "C" {
	#include <ipfixcol2/converters.h>
}

#define BYTES_1 (1U)
#define BYTES_2 (2U)
#define BYTES_3 (3U)
#define BYTES_4 (4U)
#define BYTES_5 (5U)
#define BYTES_6 (6U)
#define BYTES_7 (7U)
#define BYTES_8 (8U)

// Auxiliary definitions of maximal values for UINT_XX for 3, 5, 6 and 7 bytes
const uint32_t IPX_UINT24_MAX = 0xFFFFFFUL;
const uint64_t IPX_UINT40_MAX = 0x000000FFFFFFFFFFULL;
const uint64_t IPX_UINT48_MAX = 0x0000FFFFFFFFFFFFULL;
const uint64_t IPX_UINT56_MAX = 0x00FFFFFFFFFFFFFFULL;

const int32_t IPX_INT24_MAX = 0x007FFFFFL;
const int64_t IPX_INT40_MAX = 0x0000007FFFFFFFFFLL;
const int64_t IPX_INT48_MAX = 0x00007FFFFFFFFFFFLL;
const int64_t IPX_INT56_MAX = 0x007FFFFFFFFFFFFFLL;

const int32_t IPX_INT24_MIN = 0xFF800000;
const int64_t IPX_INT40_MIN = 0xFFFFFF8000000000LL;
const int64_t IPX_INT48_MIN = 0xFFFF800000000000LL;
const int64_t IPX_INT56_MIN = 0xFF80000000000000LL;

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

/**
 * \brief Test fixture for Unsigned Integer tests
 */
class ConverterUint : public ::testing::Test {
protected:
	/*
	 * We want to have all variables dynamically allocated so Valgrind can check
	 * access out of bounds, etc.
     */
	uint8_t  *u8;
	uint16_t *u16;
	uint32_t *u32;
	uint64_t *u64;

	uint8_t *u24;
	uint8_t *u40;
	uint8_t *u48;
	uint8_t *u56;

public:
	/** Create variables for tests */
	virtual void SetUp() {
		u8  = new uint8_t;
		u16 = new uint16_t;
		u32 = new uint32_t;
		u64 = new uint64_t;

		u24 = new uint8_t[3];
		u40 = new uint8_t[5];
		u48 = new uint8_t[6];
		u56 = new uint8_t[7];
	}

	/** Destroy variables for the tests */
	virtual void TearDown() {
		delete u8;
		delete u16;
		delete u32;
		delete u64;

		delete[] u24;
		delete[] u40;
		delete[] u48;
		delete[] u56;
	}
};

/*
 * Insert the maximum possible value i.e. "UINT64_MAX" and the minimum possible
 * value i.e. "0" into 1 - 8 byte variables.
 */
TEST_F(ConverterUint, SetUintMaxMin)
{
	// SetUp
	const uint64_t max_val = UINT64_MAX;
	const uint64_t min_val = 0U;

	// Execute
	// 1 byte
	EXPECT_EQ(ipx_set_uint_be(u8, BYTES_1,  max_val), IPX_ERR_TRUNC);
	EXPECT_EQ(*u8,  UINT8_MAX);
	EXPECT_EQ(ipx_set_uint_be(u8, BYTES_1,  min_val), IPX_OK);
	EXPECT_EQ(*u8,  0U);

	// 2 bytes
	EXPECT_EQ(ipx_set_uint_be(u16, BYTES_2, max_val), IPX_ERR_TRUNC);
	EXPECT_EQ(*u16, UINT16_MAX);
	EXPECT_EQ(ipx_set_uint_be(u16, BYTES_2, min_val), IPX_OK);
	EXPECT_EQ(*u16, 0U);

	// 4 bytes
	EXPECT_EQ(ipx_set_uint_be(u32, BYTES_4, max_val), IPX_ERR_TRUNC);
	EXPECT_EQ(*u32, UINT32_MAX);
	EXPECT_EQ(ipx_set_uint_be(u32, BYTES_4, min_val), IPX_OK);
	EXPECT_EQ(*u32, 0U);

	// 8 bytes
	EXPECT_EQ(ipx_set_uint_be(u64, BYTES_8, max_val), IPX_OK);
	EXPECT_EQ(*u64, UINT64_MAX);
	EXPECT_EQ(ipx_set_uint_be(u64, BYTES_8, min_val), IPX_OK);
	EXPECT_EQ(*u64, 0U);

	// Other (unusual situations i.e. 3, 5, 6 and 7 bytes)
	// 3 bytes
	EXPECT_EQ(ipx_set_uint_be(u24, BYTES_3, max_val), IPX_ERR_TRUNC);
	EXPECT_EQ(memcmp(u24, &max_val, BYTES_3), 0);
	EXPECT_EQ(ipx_set_uint_be(u24, BYTES_3, min_val), IPX_OK);
	EXPECT_EQ(memcmp(u24, &min_val, BYTES_3), 0);

	// 5 bytes
	EXPECT_EQ(ipx_set_uint_be(u40, BYTES_5, max_val), IPX_ERR_TRUNC);
	EXPECT_EQ(memcmp(u40, &max_val, BYTES_5), 0);
	EXPECT_EQ(ipx_set_uint_be(u40, BYTES_5, min_val), IPX_OK);
	EXPECT_EQ(memcmp(u40, &min_val, BYTES_5), 0);

	// 6 bytes
	EXPECT_EQ(ipx_set_uint_be(u48, BYTES_6, max_val), IPX_ERR_TRUNC);
	EXPECT_EQ(memcmp(u48, &max_val, BYTES_6), 0);
	EXPECT_EQ(ipx_set_uint_be(u48, BYTES_6, min_val), IPX_OK);
	EXPECT_EQ(memcmp(u48, &min_val, BYTES_6), 0);

	// 7 bytes
	EXPECT_EQ(ipx_set_uint_be(u56, BYTES_7, max_val), IPX_ERR_TRUNC);
	EXPECT_EQ(memcmp(u56, &max_val, BYTES_7), 0);
	EXPECT_EQ(ipx_set_uint_be(u56, BYTES_7, min_val), IPX_OK);
	EXPECT_EQ(memcmp(u56, &min_val, BYTES_7), 0);
}

/*
 * Insert max + 1/max/max - 1 values into 1 - 8 byte variables.
 */
TEST_F(ConverterUint, SetUintAboveBelow)
{
	// SetUp
	const uint16_t u8_above =  ((uint16_t) UINT8_MAX) + 1;
	const uint8_t  u8_below =  UINT8_MAX - 1;
	const uint32_t u16_above = ((uint32_t) UINT16_MAX) + 1;
	const uint16_t u16_below = UINT16_MAX - 1;
	const uint64_t u32_above = ((uint64_t) UINT32_MAX) + 1;
	const uint32_t u32_below = UINT32_MAX - 1;
	const uint64_t u64_below = UINT64_MAX - 1;

	const uint32_t u24_above = IPX_UINT24_MAX + 1;
	const uint32_t u24_below = IPX_UINT24_MAX - 1;
	const uint64_t u40_above = IPX_UINT40_MAX + 1;
	const uint64_t u40_below = IPX_UINT40_MAX - 1;
	const uint64_t u48_above = IPX_UINT48_MAX + 1;
	const uint64_t u48_below = IPX_UINT48_MAX - 1;
	const uint64_t u56_above = IPX_UINT56_MAX + 1;
	const uint64_t u56_below = IPX_UINT56_MAX - 1;

	// Execute
	// 1 byte
	EXPECT_EQ(ipx_set_uint_be(u8, BYTES_1, u8_above), IPX_ERR_TRUNC);
	EXPECT_EQ(*u8, UINT8_MAX);  // No endian conversion needed
	EXPECT_EQ(ipx_set_uint_be(u8, BYTES_1, UINT8_MAX), IPX_OK);
	EXPECT_EQ(*u8, UINT8_MAX);  // No endian conversion needed
	EXPECT_EQ(ipx_set_uint_be(u8, BYTES_1, u8_below), IPX_OK);
	EXPECT_EQ(*u8, u8_below);   // No endian conversion needed (only 1 byte)

	// 2 bytes
	EXPECT_EQ(ipx_set_uint_be(u16, BYTES_2, u16_above), IPX_ERR_TRUNC);
	EXPECT_EQ(*u16, UINT16_MAX); // No endian conversion needed
	EXPECT_EQ(ipx_set_uint_be(u16, BYTES_2, UINT16_MAX), IPX_OK);
	EXPECT_EQ(*u16, UINT16_MAX);  // No endian conversion needed
	EXPECT_EQ(ipx_set_uint_be(u16, BYTES_2, u16_below), IPX_OK);
	EXPECT_EQ(*u16, htons(u16_below));

	// 4 bytes
	EXPECT_EQ(ipx_set_uint_be(u32, BYTES_4, u32_above), IPX_ERR_TRUNC);
	EXPECT_EQ(*u32, UINT32_MAX); // No endian conversion needed
	EXPECT_EQ(ipx_set_uint_be(u32, BYTES_4, UINT32_MAX), IPX_OK);
	EXPECT_EQ(*u32, UINT32_MAX);  // No endian conversion needed
	EXPECT_EQ(ipx_set_uint_be(u32, BYTES_4, u32_below), IPX_OK);
	EXPECT_EQ(*u32, htonl(u32_below));

	// 8 bytes (only the value below MAX and MAX)
	EXPECT_EQ(ipx_set_uint_be(u64, BYTES_8, UINT64_MAX), IPX_OK);
	EXPECT_EQ(*u64, UINT64_MAX);  // No endian conversion needed
	EXPECT_EQ(ipx_set_uint_be(u64, BYTES_8, u64_below), IPX_OK);
	EXPECT_EQ(*u64, htobe64(u64_below));

	// Other (unusual situations i.e. 3, 5, 6 and 7 bytes)
	const uint64_t temp_max = UINT64_MAX;
	uint8_t temp32[4];
	uint8_t temp64[8];

	// 3 bytes
	EXPECT_EQ(ipx_set_uint_be(u24, BYTES_3, u24_above), IPX_ERR_TRUNC);
	EXPECT_EQ(memcmp(u24, &temp_max, BYTES_3), 0);
	EXPECT_EQ(ipx_set_uint_be(u24, BYTES_3, IPX_UINT24_MAX), IPX_OK);
	EXPECT_EQ(memcmp(u24, &temp_max, BYTES_3), 0);
	EXPECT_EQ(ipx_set_uint_be(u24, BYTES_3, u24_below), IPX_OK);
	*((uint32_t *) temp32) = htonl(u24_below);
	EXPECT_EQ(memcmp(u24, &temp32[1], BYTES_3), 0);

	// 5 bytes
	EXPECT_EQ(ipx_set_uint_be(u40, BYTES_5, u40_above), IPX_ERR_TRUNC);
	EXPECT_EQ(memcmp(u40, &temp_max, BYTES_5), 0);
	EXPECT_EQ(ipx_set_uint_be(u40, BYTES_5, IPX_UINT40_MAX), IPX_OK);
	EXPECT_EQ(memcmp(u40, &temp_max, BYTES_5), 0);
	EXPECT_EQ(ipx_set_uint_be(u40, BYTES_5, u40_below), IPX_OK);
	*((uint64_t *) temp64) = htobe64(u40_below);
	EXPECT_EQ(memcmp(u40, &temp64[3], BYTES_5), 0);

	// 6 bytes
	EXPECT_EQ(ipx_set_uint_be(u48, BYTES_6, u48_above), IPX_ERR_TRUNC);
	EXPECT_EQ(memcmp(u48, &temp_max, BYTES_6), 0);
	EXPECT_EQ(ipx_set_uint_be(u48, BYTES_6, IPX_UINT48_MAX), IPX_OK);
	EXPECT_EQ(memcmp(u48, &temp_max, BYTES_6), 0);
	EXPECT_EQ(ipx_set_uint_be(u48, BYTES_6, u48_below), IPX_OK);
	*((uint64_t *) temp64) = htobe64(u48_below);
	EXPECT_EQ(memcmp(u48, &temp64[2], BYTES_6), 0);

	// 7 bytes
	EXPECT_EQ(ipx_set_uint_be(u56, BYTES_7, u56_above), IPX_ERR_TRUNC);
	EXPECT_EQ(memcmp(u56, &temp_max, BYTES_7), 0);
	EXPECT_EQ(ipx_set_uint_be(u56, BYTES_7, IPX_UINT56_MAX), IPX_OK);
	EXPECT_EQ(memcmp(u56, &temp_max, BYTES_7), 0);
	EXPECT_EQ(ipx_set_uint_be(u56, BYTES_7, u56_below), IPX_OK);
	*((uint64_t *) temp64) = htobe64(u56_below);
	EXPECT_EQ(memcmp(u56, &temp64[1], BYTES_7), 0);
}

/*
 * "Random" values in the valid interval for 1 - 8 bytes unsigned values
 */
TEST_F(ConverterUint, SetUintInRandom)
{
	// 1 byte
	const uint8_t u8_rand1 =  12U;
	const uint8_t u8_rand2 =  93U;
	const uint8_t u8_rand3 = 235U;
	EXPECT_EQ(ipx_set_uint_be(u8, BYTES_1, u8_rand1), IPX_OK);
	EXPECT_EQ(*u8, u8_rand1);
	EXPECT_EQ(ipx_set_uint_be(u8, BYTES_1, u8_rand2), IPX_OK);
	EXPECT_EQ(*u8, u8_rand2);
	EXPECT_EQ(ipx_set_uint_be(u8, BYTES_1, u8_rand3), IPX_OK);
	EXPECT_EQ(*u8, u8_rand3);

	// 2 bytes
	const uint16_t u16_rand1 =  1342U;
	const uint16_t u16_rand2 = 25432U;
	const uint16_t u16_rand3 = 45391U;
	EXPECT_EQ(ipx_set_uint_be(u16, BYTES_2, u16_rand1), IPX_OK);
	EXPECT_EQ(*u16, htons(u16_rand1));
	EXPECT_EQ(ipx_set_uint_be(u16, BYTES_2, u16_rand2), IPX_OK);
	EXPECT_EQ(*u16, htons(u16_rand2));
	EXPECT_EQ(ipx_set_uint_be(u16, BYTES_2, u16_rand3), IPX_OK);
	EXPECT_EQ(*u16, htons(u16_rand3));

	// 4 bytes
	const uint32_t u32_rand1 =      50832UL;
	const uint32_t u32_rand2 =   11370824UL;
	const uint32_t u32_rand3 = 3793805425UL;
	EXPECT_EQ(ipx_set_uint_be(u32, BYTES_4, u32_rand1), IPX_OK);
	EXPECT_EQ(*u32, htonl(u32_rand1));
	EXPECT_EQ(ipx_set_uint_be(u32, BYTES_4, u32_rand2), IPX_OK);
	EXPECT_EQ(*u32, htonl(u32_rand2));
	EXPECT_EQ(ipx_set_uint_be(u32, BYTES_4, u32_rand3), IPX_OK);
	EXPECT_EQ(*u32, htonl(u32_rand3));

	// 8 bytes
	const uint64_t u64_rand1 =         428760872517ULL;
	const uint64_t u64_rand2 =     8275792237734210ULL;
	const uint64_t u64_rand3 = 17326724161708531625ULL;
	EXPECT_EQ(ipx_set_uint_be(u64, BYTES_8, u64_rand1), IPX_OK);
	EXPECT_EQ(*u64, htobe64(u64_rand1));
	EXPECT_EQ(ipx_set_uint_be(u64, BYTES_8, u64_rand2), IPX_OK);
	EXPECT_EQ(*u64, htobe64(u64_rand2));
	EXPECT_EQ(ipx_set_uint_be(u64, BYTES_8, u64_rand3), IPX_OK);
	EXPECT_EQ(*u64, htobe64(u64_rand3));

	// Other (unusual situations i.e. 3, 5, 6 and 7 bytes)
	uint8_t temp32[4];
	uint8_t temp64[8];

	// 3 bytes
	const uint32_t u24_rand1 =    22311UL;
	const uint32_t u24_rand2 =   861354UL;
	const uint32_t u24_rand3 = 14075499UL;
	EXPECT_EQ(ipx_set_uint_be(u24, BYTES_3, u24_rand1), IPX_OK);  // Rand 1
	*((uint32_t *) temp32) = htonl(u24_rand1);
	EXPECT_EQ(memcmp(u24, &temp32[1], BYTES_3), 0);
	EXPECT_EQ(ipx_set_uint_be(u24, BYTES_3, u24_rand2), IPX_OK);  // Rand 2
	*((uint32_t *) temp32) = htonl(u24_rand2);
	EXPECT_EQ(memcmp(u24, &temp32[1], BYTES_3), 0);
	EXPECT_EQ(ipx_set_uint_be(u24, BYTES_3, u24_rand3), IPX_OK);  // Rand 3
	*((uint32_t *) temp32) = htonl(u24_rand3);
	EXPECT_EQ(memcmp(u24, &temp32[1], BYTES_3), 0);

	// 5 bytes
	const uint64_t u40_rand1 =       360214ULL;
	const uint64_t u40_rand2 =    240285687ULL;
	const uint64_t u40_rand3 = 796219095503ULL;
	EXPECT_EQ(ipx_set_uint_be(u40, BYTES_5, u40_rand1), IPX_OK); // Rand 1
	*((uint64_t *) temp64) = htobe64(u40_rand1);
	EXPECT_EQ(memcmp(u40, &temp64[3], BYTES_5), 0);
	EXPECT_EQ(ipx_set_uint_be(u40, BYTES_5, u40_rand2), IPX_OK); // Rand 2
	*((uint64_t *) temp64) = htobe64(u40_rand2);
	EXPECT_EQ(memcmp(u40, &temp64[3], BYTES_5), 0);
	EXPECT_EQ(ipx_set_uint_be(u40, BYTES_5, u40_rand3), IPX_OK); // Rand 3
	*((uint64_t *) temp64) = htobe64(u40_rand3);
	EXPECT_EQ(memcmp(u40, &temp64[3], BYTES_5), 0);

	// 6 bytes
	const uint64_t u48_rand1 =       696468180ULL;
	const uint64_t u48_rand2 =    671963163167ULL;
	const uint64_t u48_rand3 = 209841476899288ULL;
	EXPECT_EQ(ipx_set_uint_be(u48, BYTES_6, u48_rand1), IPX_OK); // Rand 1
	*((uint64_t *) temp64) = htobe64(u48_rand1);
	EXPECT_EQ(memcmp(u48, &temp64[2], BYTES_6), 0);
	EXPECT_EQ(ipx_set_uint_be(u48, BYTES_6, u48_rand2), IPX_OK); // Rand 2
	*((uint64_t *) temp64) = htobe64(u48_rand2);
	EXPECT_EQ(memcmp(u48, &temp64[2], BYTES_6), 0);
	EXPECT_EQ(ipx_set_uint_be(u48, BYTES_6, u48_rand3), IPX_OK); // Rand 3
	*((uint64_t *) temp64) = htobe64(u48_rand3);
	EXPECT_EQ(memcmp(u48, &temp64[2], BYTES_6), 0);

	// 7 bytes
	const uint64_t u56_rand1 =      194728764120ULL;
	const uint64_t u56_rand2 =   128273048983421ULL;
	const uint64_t u56_rand3 = 66086893994497342ULL;
	EXPECT_EQ(ipx_set_uint_be(u56, BYTES_7, u56_rand1), IPX_OK); // Rand 1
	*((uint64_t *) temp64) = htobe64(u56_rand1);
	EXPECT_EQ(memcmp(u56, &temp64[1], BYTES_7), 0);
	EXPECT_EQ(ipx_set_uint_be(u56, BYTES_7, u56_rand2), IPX_OK); // Rand 2
	*((uint64_t *) temp64) = htobe64(u56_rand2);
	EXPECT_EQ(memcmp(u56, &temp64[1], BYTES_7), 0);
	EXPECT_EQ(ipx_set_uint_be(u56, BYTES_7, u56_rand3), IPX_OK); // Rand 3
	*((uint64_t *) temp64) = htobe64(u56_rand3);
	EXPECT_EQ(memcmp(u56, &temp64[1], BYTES_7), 0);
}

/*
 * Test unsupported size of data fields
 */
TEST_F(ConverterUint, SetUintOutOfRange)
{
	const uint64_t value = 123456ULL; // Just random number

	// Just random sizes of arrays
	const size_t temp72_size =   9;
	const size_t temp88_size =  11;
	const size_t temp128_size = 16;
	const size_t temp192_size = 24;
	const size_t temp256_size = 32;

	uint8_t temp72[temp72_size];
	uint8_t temp88[temp88_size];
	uint8_t temp128[temp128_size];
	uint8_t temp192[temp192_size];
	uint8_t temp256[temp256_size];

	EXPECT_EQ(ipx_set_uint_be(temp72, 0, value), IPX_ERR_ARG);
	EXPECT_EQ(ipx_set_uint_be(temp72, temp72_size, value), IPX_ERR_ARG);
	EXPECT_EQ(ipx_set_uint_be(temp88, temp88_size, value), IPX_ERR_ARG);
	EXPECT_EQ(ipx_set_uint_be(temp128, temp128_size, value), IPX_ERR_ARG);
	EXPECT_EQ(ipx_set_uint_be(temp192, temp192_size, value), IPX_ERR_ARG);
	EXPECT_EQ(ipx_set_uint_be(temp256, temp256_size, value), IPX_ERR_ARG);
}

/*
 * Test getter for maximum and minimum values
 */
TEST_F(ConverterUint, GetUintMaxMin)
{
	uint64_t conv_res;

	// 1 byte
	EXPECT_EQ(ipx_set_uint_be(u8, BYTES_1, UINT8_MAX), IPX_OK); // Max
	EXPECT_EQ(ipx_get_uint_be(u8, BYTES_1, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, (uint8_t) UINT8_MAX);

	EXPECT_EQ(ipx_set_uint_be(u8, BYTES_1, 0), IPX_OK); // Min
	EXPECT_EQ(ipx_get_uint_be(u8, BYTES_1, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, 0U);

	// 2 bytes
	EXPECT_EQ(ipx_set_uint_be(u16, BYTES_2, UINT16_MAX), IPX_OK); // Max
	EXPECT_EQ(ipx_get_uint_be(u16, BYTES_2, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, (uint16_t) UINT16_MAX);

	EXPECT_EQ(ipx_set_uint_be(u16, BYTES_2, 0), IPX_OK); // Min
	EXPECT_EQ(ipx_get_uint_be(u16, BYTES_2, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, 0U);

	// 4 bytes
	EXPECT_EQ(ipx_set_uint_be(u32, BYTES_4, UINT32_MAX), IPX_OK); // Max
	EXPECT_EQ(ipx_get_uint_be(u32, BYTES_4, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, (uint32_t) UINT32_MAX);

	EXPECT_EQ(ipx_set_uint_be(u32, BYTES_4, 0), IPX_OK); // Min
	EXPECT_EQ(ipx_get_uint_be(u32, BYTES_4, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, 0U);

	// 8 bytes
	EXPECT_EQ(ipx_set_uint_be(u64, BYTES_8, UINT64_MAX), IPX_OK); // Max
	EXPECT_EQ(ipx_get_uint_be(u64, BYTES_8, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, (uint64_t) UINT64_MAX);

	EXPECT_EQ(ipx_set_uint_be(u64, BYTES_8, 0), IPX_OK); // Min
	EXPECT_EQ(ipx_get_uint_be(u64, BYTES_8, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, 0U);

	// Other (unusual situations i.e. 3, 5, 6 and 7 bytes)
	// 3 bytes
	EXPECT_EQ(ipx_set_uint_be(u24, BYTES_3, IPX_UINT24_MAX), IPX_OK); // Max
	EXPECT_EQ(ipx_get_uint_be(u24, BYTES_3, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, IPX_UINT24_MAX);

	EXPECT_EQ(ipx_set_uint_be(u24, BYTES_3, 0), IPX_OK); // Min
	EXPECT_EQ(ipx_get_uint_be(u24, BYTES_3, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, 0U);

	// 5 bytes
	EXPECT_EQ(ipx_set_uint_be(u40, BYTES_5, IPX_UINT40_MAX), IPX_OK); // Max
	EXPECT_EQ(ipx_get_uint_be(u40, BYTES_5, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, IPX_UINT40_MAX);

	EXPECT_EQ(ipx_set_uint_be(u40, BYTES_5, 0), IPX_OK); // Min
	EXPECT_EQ(ipx_get_uint_be(u40, BYTES_5, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, 0U);

	// 6 bytes
	EXPECT_EQ(ipx_set_uint_be(u48, BYTES_6, IPX_UINT48_MAX), IPX_OK); // Max
	EXPECT_EQ(ipx_get_uint_be(u48, BYTES_6, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, IPX_UINT48_MAX);

	EXPECT_EQ(ipx_set_uint_be(u48, BYTES_6, 0), IPX_OK); // Min
	EXPECT_EQ(ipx_get_uint_be(u48, BYTES_6, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, 0U);

	// 7 bytes
	EXPECT_EQ(ipx_set_uint_be(u56, BYTES_7, IPX_UINT56_MAX), IPX_OK); // Max
	EXPECT_EQ(ipx_get_uint_be(u56, BYTES_7, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, IPX_UINT56_MAX);

	EXPECT_EQ(ipx_set_uint_be(u56, BYTES_7, 0), IPX_OK); // Min
	EXPECT_EQ(ipx_get_uint_be(u56, BYTES_7, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, 0U);
}

/*
 * Test getter for random values in the interval
 */
TEST_F(ConverterUint, GetUintRandom)
{
	uint64_t conv_res;

	// 1 byte
	const uint8_t u8_rand1 =  53U;
	const uint8_t u8_rand2 = 123U;
	const uint8_t u8_rand3 = 212U;
	EXPECT_EQ(ipx_set_uint_be(u8, BYTES_1, u8_rand1), IPX_OK); // Rand 1
	EXPECT_EQ(ipx_get_uint_be(u8, BYTES_1, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, u8_rand1);
	EXPECT_EQ(ipx_set_uint_be(u8, BYTES_1, u8_rand2), IPX_OK); // Rand 2
	EXPECT_EQ(ipx_get_uint_be(u8, BYTES_1, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, u8_rand2);
	EXPECT_EQ(ipx_set_uint_be(u8, BYTES_1, u8_rand3), IPX_OK); // Rand 3
	EXPECT_EQ(ipx_get_uint_be(u8, BYTES_1, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, u8_rand3);

	// 2 bytes
	const uint16_t u16_rand1 =   421U;
	const uint16_t u16_rand2 =  2471U;
	const uint16_t u16_rand3 = 37245U;
	EXPECT_EQ(ipx_set_uint_be(u16, BYTES_2, u16_rand1), IPX_OK); // Rand 1
	EXPECT_EQ(ipx_get_uint_be(u16, BYTES_2, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, u16_rand1);
	EXPECT_EQ(ipx_set_uint_be(u16, BYTES_2, u16_rand2), IPX_OK); // Rand 2
	EXPECT_EQ(ipx_get_uint_be(u16, BYTES_2, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, u16_rand2);
	EXPECT_EQ(ipx_set_uint_be(u16, BYTES_2, u16_rand3), IPX_OK); // Rand 3
	EXPECT_EQ(ipx_get_uint_be(u16, BYTES_2, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, u16_rand3);

	// 4 bytes
	const uint32_t u32_rand1 =     109127UL;
	const uint32_t u32_rand2 =   28947291UL;
	const uint32_t u32_rand3 = 1975298731UL;
	EXPECT_EQ(ipx_set_uint_be(u32, BYTES_4, u32_rand1), IPX_OK); // Rand 1
	EXPECT_EQ(ipx_get_uint_be(u32, BYTES_4, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, u32_rand1);
	EXPECT_EQ(ipx_set_uint_be(u32, BYTES_4, u32_rand2), IPX_OK); // Rand 2
	EXPECT_EQ(ipx_get_uint_be(u32, BYTES_4, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, u32_rand2);
	EXPECT_EQ(ipx_set_uint_be(u32, BYTES_4, u32_rand3), IPX_OK); // Rand 3
	EXPECT_EQ(ipx_get_uint_be(u32, BYTES_4, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, u32_rand3);

	// 8 bytes
	const uint64_t u64_rand1 =         147984727321ULL;
	const uint64_t u64_rand2 =     2876987613687162ULL;
	const uint64_t u64_rand3 = 11298373761876598719ULL;
	EXPECT_EQ(ipx_set_uint_be(u64, BYTES_8, u64_rand1), IPX_OK); // Rand 1
	EXPECT_EQ(ipx_get_uint_be(u64, BYTES_8, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, u64_rand1);
	EXPECT_EQ(ipx_set_uint_be(u64, BYTES_8, u64_rand2), IPX_OK); // Rand 2
	EXPECT_EQ(ipx_get_uint_be(u64, BYTES_8, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, u64_rand2);
	EXPECT_EQ(ipx_set_uint_be(u64, BYTES_8, u64_rand3), IPX_OK); // Rand 3
	EXPECT_EQ(ipx_get_uint_be(u64, BYTES_8, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, u64_rand3);

	// Other (unusual situations i.e. 3, 5, 6 and 7 bytes)
	// 3 bytes
	const uint32_t u24_rand1 =    38276UL;
	const uint32_t u24_rand2 =   763547UL;
	const uint32_t u24_rand3 = 11287321UL;
	EXPECT_EQ(ipx_set_uint_be(u24, BYTES_3, u24_rand1), IPX_OK); // Rand 1
	EXPECT_EQ(ipx_get_uint_be(u24, BYTES_3, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, u24_rand1);
	EXPECT_EQ(ipx_set_uint_be(u24, BYTES_3, u24_rand2), IPX_OK); // Rand 2
	EXPECT_EQ(ipx_get_uint_be(u24, BYTES_3, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, u24_rand2);
	EXPECT_EQ(ipx_set_uint_be(u24, BYTES_3, u24_rand3), IPX_OK); // Rand 3
	EXPECT_EQ(ipx_get_uint_be(u24, BYTES_3, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, u24_rand3);

	// 5 bytes
	const uint64_t u40_rand1 =       278632ULL;
	const uint64_t u40_rand2 =    287638124ULL;
	const uint64_t u40_rand3 = 527836261240ULL;
	EXPECT_EQ(ipx_set_uint_be(u40, BYTES_5, u40_rand1), IPX_OK); // Rand 1
	EXPECT_EQ(ipx_get_uint_be(u40, BYTES_5, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, u40_rand1);
	EXPECT_EQ(ipx_set_uint_be(u40, BYTES_5, u40_rand2), IPX_OK); // Rand 2
	EXPECT_EQ(ipx_get_uint_be(u40, BYTES_5, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, u40_rand2);
	EXPECT_EQ(ipx_set_uint_be(u40, BYTES_5, u40_rand3), IPX_OK); // Rand 3
	EXPECT_EQ(ipx_get_uint_be(u40, BYTES_5, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, u40_rand3);

	// 6 bytes
	const uint64_t u48_rand1 =       287468172ULL;
	const uint64_t u48_rand2 =    897287628371ULL;
	const uint64_t u48_rand3 = 219879286827632ULL;
	EXPECT_EQ(ipx_set_uint_be(u48, BYTES_6, u48_rand1), IPX_OK); // Rand 1
	EXPECT_EQ(ipx_get_uint_be(u48, BYTES_6, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, u48_rand1);
	EXPECT_EQ(ipx_set_uint_be(u48, BYTES_6, u48_rand2), IPX_OK); // Rand 2
	EXPECT_EQ(ipx_get_uint_be(u48, BYTES_6, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, u48_rand2);
	EXPECT_EQ(ipx_set_uint_be(u48, BYTES_6, u48_rand3), IPX_OK); // Rand 3
	EXPECT_EQ(ipx_get_uint_be(u48, BYTES_6, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, u48_rand3);

	// 7 bytes
	const uint64_t u56_rand1 =      387648182713ULL;
	const uint64_t u56_rand2 =   258628761274610ULL;
	const uint64_t u56_rand3 = 58762617654765176ULL;
	EXPECT_EQ(ipx_set_uint_be(u56, BYTES_7, u56_rand1), IPX_OK); // Rand 1
	EXPECT_EQ(ipx_get_uint_be(u56, BYTES_7, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, u56_rand1);
	EXPECT_EQ(ipx_set_uint_be(u56, BYTES_7, u56_rand2), IPX_OK); // Rand 2
	EXPECT_EQ(ipx_get_uint_be(u56, BYTES_7, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, u56_rand2);
	EXPECT_EQ(ipx_set_uint_be(u56, BYTES_7, u56_rand3), IPX_OK); // Rand 3
	EXPECT_EQ(ipx_get_uint_be(u56, BYTES_7, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, u56_rand3);
}

/*
 * Test unsupported size of data fields
 */
TEST_F(ConverterUint, GetUintOutOfRange)
{
	const uint64_t c_value = 1234567890123456789ULL; // Just random number
	uint64_t value = c_value;
	void *ptr = NULL; // Invalid pointer

	// Just random sizes of arrays
	const size_t temp72_size =   9;
	const size_t temp88_size =  11;
	const size_t temp128_size = 16;
	const size_t temp192_size = 24;
	const size_t temp256_size = 32;

	uint8_t temp72[temp72_size];
	uint8_t temp88[temp88_size];
	uint8_t temp128[temp128_size];
	uint8_t temp192[temp192_size];
	uint8_t temp256[temp256_size];

	EXPECT_EQ(ipx_get_uint_be(temp72, 0U, &value), IPX_ERR_ARG);
	EXPECT_TRUE(value == c_value);
	EXPECT_EQ(ipx_get_uint_be(temp72, temp72_size, &value), IPX_ERR_ARG);
	EXPECT_TRUE(value == c_value);
	EXPECT_EQ(ipx_get_uint_be(temp88, temp88_size, &value), IPX_ERR_ARG);
	EXPECT_TRUE(value == c_value);
	EXPECT_EQ(ipx_get_uint_be(temp128, temp128_size, &value), IPX_ERR_ARG);
	EXPECT_TRUE(value == c_value);
	EXPECT_EQ(ipx_get_uint_be(temp192, temp192_size, &value), IPX_ERR_ARG);
	EXPECT_TRUE(value == c_value);
	EXPECT_EQ(ipx_get_uint_be(temp256, temp256_size, &value), IPX_ERR_ARG);
	EXPECT_TRUE(value == c_value);

	EXPECT_EQ(ipx_get_uint_be(ptr, 0U, &value), IPX_ERR_ARG);
	EXPECT_TRUE(value == c_value);
	EXPECT_EQ(ipx_get_uint_be(ptr, temp72_size, &value), IPX_ERR_ARG);
	EXPECT_TRUE(value == c_value);
	EXPECT_EQ(ipx_get_uint_be(ptr, temp88_size, &value), IPX_ERR_ARG);
	EXPECT_TRUE(value == c_value);
	EXPECT_EQ(ipx_get_uint_be(ptr, temp128_size, &value), IPX_ERR_ARG);
	EXPECT_TRUE(value == c_value);
	EXPECT_EQ(ipx_get_uint_be(ptr, temp192_size, &value), IPX_ERR_ARG);
	EXPECT_TRUE(value == c_value);
	EXPECT_EQ(ipx_get_uint_be(ptr, temp256_size, &value), IPX_ERR_ARG);
	EXPECT_TRUE(value == c_value);
}

/**
 * \brief Test fixture for Signed Integer tests
 */
class ConverterInt : public ::testing::Test {
protected:
	/*
	 * We want to have all variables dynamically allocated so Valgrind can check
	 * access out of bounds, etc.
     */
	int8_t  *i8;
	int16_t *i16;
	int32_t *i32;
	int64_t *i64;

	int8_t *i24;
	int8_t *i40;
	int8_t *i48;
	int8_t *i56;

public:
	/** Create variables for tests */
	virtual void SetUp() {
		i8  = new int8_t;
		i16 = new int16_t;
		i32 = new int32_t;
		i64 = new int64_t;

		i24 = new int8_t[3];
		i40 = new int8_t[5];
		i48 = new int8_t[6];
		i56 = new int8_t[7];
	}

	/** Destroy variables for the tests */
	virtual void TearDown() {
		delete i8;
		delete i16;
		delete i32;
		delete i64;

		delete[] i24;
		delete[] i40;
		delete[] i48;
		delete[] i56;
	}
};

/*
 * Insert the maximum possible value i.e. "INT64_MAX" and the minimum possible
 * value i.e. "INT64_MIN" into 1 - 8 byte variables. The test expects
 * truncation of values.
 */
TEST_F(ConverterInt, SetIntMaxMin) {
	// SetUp
	const int64_t max_val = INT64_MAX;
	const int64_t min_val = INT64_MIN;

	// Execute
	// 1 byte
	EXPECT_EQ(ipx_set_int_be(i8, BYTES_1, max_val), IPX_ERR_TRUNC);
	EXPECT_EQ(*i8, INT8_MAX);
	EXPECT_EQ(ipx_set_int_be(i8, BYTES_1, min_val), IPX_ERR_TRUNC);
	EXPECT_EQ(*i8, INT8_MIN);

	// 2 bytes
	EXPECT_EQ(ipx_set_int_be(i16, BYTES_2, max_val), IPX_ERR_TRUNC);
	EXPECT_EQ(*i16, (int16_t) htons(INT16_MAX));
	EXPECT_EQ(ipx_set_int_be(i16, BYTES_2, min_val), IPX_ERR_TRUNC);
	EXPECT_EQ(*i16, (int16_t) htons(INT16_MIN));

	// 4 bytes
	EXPECT_EQ(ipx_set_int_be(i32, BYTES_4, max_val), IPX_ERR_TRUNC);
	EXPECT_EQ(*i32, (int32_t) htonl(INT32_MAX));
	EXPECT_EQ(ipx_set_int_be(i32, BYTES_4, min_val), IPX_ERR_TRUNC);
	EXPECT_EQ(*i32, (int32_t) htonl(INT32_MIN));

	// 8 bytes
	EXPECT_EQ(ipx_set_int_be(i64, BYTES_8, max_val), IPX_OK);
	EXPECT_EQ(*i64, (int64_t) htobe64(INT64_MAX));
	EXPECT_EQ(ipx_set_int_be(i64, BYTES_8, min_val), IPX_OK);
	EXPECT_EQ(*i64, (int64_t) htobe64(INT64_MIN));

	// Other (unusual situations i.e. 3, 5, 6 and 7 bytes)
	int8_t temp32[4];
	int8_t temp64[8];

	// 3 bytes
	EXPECT_EQ(ipx_set_int_be(i24, BYTES_3, max_val), IPX_ERR_TRUNC);
	*((uint32_t *) temp32) = htonl(IPX_INT24_MAX);
	EXPECT_EQ(memcmp(i24, &temp32[1], BYTES_3), 0);
	EXPECT_EQ(ipx_set_int_be(i24, BYTES_3, min_val), IPX_ERR_TRUNC);
	*((uint32_t *) temp32) = htonl(IPX_INT24_MIN);
	EXPECT_EQ(memcmp(i24, &temp32[1], BYTES_3), 0);

	// 5 bytes
	EXPECT_EQ(ipx_set_int_be(i40, BYTES_5, max_val), IPX_ERR_TRUNC);
	*((uint64_t *) temp64) = htobe64(IPX_INT40_MAX);
	EXPECT_EQ(memcmp(i40, &temp64[3], BYTES_5), 0);
	EXPECT_EQ(ipx_set_int_be(i40, BYTES_5, min_val), IPX_ERR_TRUNC);
	*((uint64_t *) temp64) = htobe64(IPX_INT40_MIN);
	EXPECT_EQ(memcmp(i40, &temp64[3], BYTES_5), 0);

	// 6 bytes
	EXPECT_EQ(ipx_set_int_be(i48, BYTES_6, max_val), IPX_ERR_TRUNC);
	*((uint64_t *) temp64) = htobe64(IPX_INT48_MAX);
	EXPECT_EQ(memcmp(i48, &temp64[2], BYTES_6), 0);
	EXPECT_EQ(ipx_set_int_be(i48, BYTES_6, min_val), IPX_ERR_TRUNC);
	*((uint64_t *) temp64) = htobe64(IPX_INT48_MIN);
	EXPECT_EQ(memcmp(i48, &temp64[2], BYTES_6), 0);

	// 7 bytes
	EXPECT_EQ(ipx_set_int_be(i56, BYTES_7, max_val), IPX_ERR_TRUNC);
	*((uint64_t *) temp64) = htobe64(IPX_INT56_MAX);
	EXPECT_EQ(memcmp(i56, &temp64[1], BYTES_7), 0);
	EXPECT_EQ(ipx_set_int_be(i56, BYTES_7, min_val), IPX_ERR_TRUNC);
	*((uint64_t *) temp64) = htobe64(IPX_INT56_MIN);
	EXPECT_EQ(memcmp(i56, &temp64[1], BYTES_7), 0);
}

/*
 * Insert max + 1/max/max - 1, min - 1/min/min + 1 and -1/0/+1 values into
 * 1 - 8 byte variables.
 */
TEST_F(ConverterInt, SetIntAboveBelow)
{
	// SetUp
	const int16_t i8_max_above  = ((int16_t) INT8_MAX) + 1;
	const int8_t  i8_max_below  = INT8_MAX - 1;
	const int32_t i16_max_above = ((int32_t) INT16_MAX) + 1;
	const int16_t i16_max_below = INT16_MAX - 1;
	const int64_t i32_max_above = ((int64_t) INT32_MAX) + 1;
	const int32_t i32_max_below = INT32_MAX - 1;
	const int64_t i64_max_below = INT64_MAX - 1;

	const int32_t i24_max_above = IPX_INT24_MAX + 1;
	const int32_t i24_max_below = IPX_INT24_MAX - 1;
	const int64_t i40_max_above = IPX_INT40_MAX + 1;
	const int64_t i40_max_below = IPX_INT40_MAX - 1;
	const int64_t i48_max_above = IPX_INT48_MAX + 1;
	const int64_t i48_max_below = IPX_INT48_MAX - 1;
	const int64_t i56_max_above = IPX_INT56_MAX + 1;
	const int64_t i56_max_below = IPX_INT56_MAX - 1;

	const int8_t  i8_min_above  = INT8_MIN + 1;
	const int16_t i8_min_below  = ((int16_t) INT8_MIN) - 1;
	const int16_t i16_min_above = INT16_MIN + 1;
	const int32_t i16_min_below = ((int32_t) INT16_MIN) - 1;
	const int32_t i32_min_above = INT32_MIN + 1;
	const int64_t i32_min_below = ((int64_t) INT32_MIN) - 1;
	const int64_t i64_min_above = INT64_MIN + 1;

	const int32_t i24_min_above = IPX_INT24_MIN + 1;
	const int32_t i24_min_below = IPX_INT24_MIN - 1;
	const int64_t i40_min_above = IPX_INT40_MIN + 1;
	const int64_t i40_min_below = IPX_INT40_MIN - 1;
	const int64_t i48_min_above = IPX_INT48_MIN + 1;
	const int64_t i48_min_below = IPX_INT48_MIN - 1;
	const int64_t i56_min_above = IPX_INT56_MIN + 1;
	const int64_t i56_min_below = IPX_INT56_MIN - 1;

	const int8_t zero_above = +1;
	const int8_t zero       =  0;
	const int8_t zero_below = -1;

	// Execute
	// 1 byte
	EXPECT_EQ(ipx_set_int_be(i8, BYTES_1, i8_max_above), IPX_ERR_TRUNC);
	EXPECT_EQ(*i8, INT8_MAX);
	EXPECT_EQ(ipx_set_int_be(i8, BYTES_1, INT8_MAX), IPX_OK);
	EXPECT_EQ(*i8, INT8_MAX);
	EXPECT_EQ(ipx_set_int_be(i8, BYTES_1, i8_max_below), IPX_OK);
	EXPECT_EQ(*i8, i8_max_below);

	EXPECT_EQ(ipx_set_int_be(i8, BYTES_1, i8_min_above), IPX_OK);
	EXPECT_EQ(*i8, i8_min_above);
	EXPECT_EQ(ipx_set_int_be(i8, BYTES_1, INT8_MIN), IPX_OK);
	EXPECT_EQ(*i8, INT8_MIN);
	EXPECT_EQ(ipx_set_int_be(i8, BYTES_1, i8_min_below), IPX_ERR_TRUNC);
	EXPECT_EQ(*i8, INT8_MIN);

	EXPECT_EQ(ipx_set_int_be(i8, BYTES_1, zero_above), IPX_OK);
	EXPECT_EQ(*i8, zero_above);
	EXPECT_EQ(ipx_set_int_be(i8, BYTES_1, zero), IPX_OK);
	EXPECT_EQ(*i8, zero);
	EXPECT_EQ(ipx_set_int_be(i8, BYTES_1, zero_below), IPX_OK);
	EXPECT_EQ(*i8, zero_below);

	// 2 bytes
	EXPECT_EQ(ipx_set_int_be(i16, BYTES_2, i16_max_above), IPX_ERR_TRUNC);
	EXPECT_EQ(*i16, (int16_t) htons(INT16_MAX));
	EXPECT_EQ(ipx_set_int_be(i16, BYTES_2, INT16_MAX), IPX_OK);
	EXPECT_EQ(*i16, (int16_t) htons(INT16_MAX));
	EXPECT_EQ(ipx_set_int_be(i16, BYTES_2, i16_max_below), IPX_OK);
	EXPECT_EQ(*i16, (int16_t) htons(i16_max_below));

	EXPECT_EQ(ipx_set_int_be(i16, BYTES_2, i16_min_above), IPX_OK);
	EXPECT_EQ(*i16, (int16_t) htons(i16_min_above));
	EXPECT_EQ(ipx_set_int_be(i16, BYTES_2, INT16_MIN), IPX_OK);
	EXPECT_EQ(*i16, (int16_t) htons(INT16_MIN));
	EXPECT_EQ(ipx_set_int_be(i16, BYTES_2, i16_min_below), IPX_ERR_TRUNC);
	EXPECT_EQ(*i16, (int16_t) htons(INT16_MIN));

	EXPECT_EQ(ipx_set_int_be(i16, BYTES_2, zero_above), IPX_OK);
	EXPECT_EQ(*i16, (int16_t) htons((int16_t) zero_above));
	EXPECT_EQ(ipx_set_int_be(i16, BYTES_2, zero), IPX_OK);
	EXPECT_EQ(*i16, (int16_t) htons((int16_t) zero));
	EXPECT_EQ(ipx_set_int_be(i16, BYTES_2, zero_below), IPX_OK);
	EXPECT_EQ(*i16, (int16_t) htons((int16_t) zero_below));

	// 4 bytes
	EXPECT_EQ(ipx_set_int_be(i32, BYTES_4, i32_max_above), IPX_ERR_TRUNC);
	EXPECT_EQ(*i32, (int32_t) htonl(INT32_MAX));
	EXPECT_EQ(ipx_set_int_be(i32, BYTES_4, INT32_MAX), IPX_OK);
	EXPECT_EQ(*i32, (int32_t) htonl(INT32_MAX));
	EXPECT_EQ(ipx_set_int_be(i32, BYTES_4, i32_max_below), IPX_OK);
	EXPECT_EQ(*i32, (int32_t) htonl(i32_max_below));

	EXPECT_EQ(ipx_set_int_be(i32, BYTES_4, i32_min_above), IPX_OK);
	EXPECT_EQ(*i32, (int32_t) htonl(i32_min_above));
	EXPECT_EQ(ipx_set_int_be(i32, BYTES_4, INT32_MIN), IPX_OK);
	EXPECT_EQ(*i32, (int32_t) htonl(INT32_MIN));
	EXPECT_EQ(ipx_set_int_be(i32, BYTES_4, i32_min_below), IPX_ERR_TRUNC);
	EXPECT_EQ(*i32, (int32_t) htonl(INT32_MIN));

	EXPECT_EQ(ipx_set_int_be(i32, BYTES_4, zero_above), IPX_OK);
	EXPECT_EQ(*i32, (int32_t) htonl((int32_t) zero_above));
	EXPECT_EQ(ipx_set_int_be(i32, BYTES_4, zero), IPX_OK);
	EXPECT_EQ(*i32, (int32_t) htonl((int32_t) zero));
	EXPECT_EQ(ipx_set_int_be(i32, BYTES_4, zero_below), IPX_OK);
	EXPECT_EQ(*i32, (int32_t) htonl((int32_t) zero_below));

	// 8 bytes
	EXPECT_EQ(ipx_set_int_be(i64, BYTES_8, INT64_MAX), IPX_OK);
	EXPECT_EQ(*i64, (int64_t) htobe64(INT64_MAX));
	EXPECT_EQ(ipx_set_int_be(i64, BYTES_8, i64_max_below), IPX_OK);
	EXPECT_EQ(*i64, (int64_t) htobe64(i64_max_below));

	EXPECT_EQ(ipx_set_int_be(i64, BYTES_8, i64_min_above), IPX_OK);
	EXPECT_EQ(*i64, (int64_t) htobe64(i64_min_above));
	EXPECT_EQ(ipx_set_int_be(i64, BYTES_8, INT64_MIN), IPX_OK);
	EXPECT_EQ(*i64, (int64_t) htobe64(INT64_MIN));

	EXPECT_EQ(ipx_set_int_be(i64, BYTES_8, zero_above), IPX_OK);
	EXPECT_EQ(*i64, (int64_t) htobe64((int64_t) zero_above));
	EXPECT_EQ(ipx_set_int_be(i64, BYTES_8, zero), IPX_OK);
	EXPECT_EQ(*i64, (int64_t) htobe64((int64_t) zero));
	EXPECT_EQ(ipx_set_int_be(i64, BYTES_8, zero_below), IPX_OK);
	EXPECT_EQ(*i64, (int64_t) htobe64((int64_t) zero_below));

	// Other (unusual situations i.e. 3, 5, 6 and 7 bytes)
	int8_t temp32[4];
	int8_t temp64[8];

	// 3 bytes
	EXPECT_EQ(ipx_set_int_be(i24, BYTES_3, i24_max_above), IPX_ERR_TRUNC);
	*((uint32_t *) temp32) = htonl(IPX_INT24_MAX);
	EXPECT_EQ(memcmp(i24, &temp32[1], BYTES_3), 0);
	EXPECT_EQ(ipx_set_int_be(i24, BYTES_3, IPX_INT24_MAX), IPX_OK);
	*((uint32_t *) temp32) = htonl(IPX_INT24_MAX);
	EXPECT_EQ(memcmp(i24, &temp32[1], BYTES_3), 0);
	EXPECT_EQ(ipx_set_int_be(i24, BYTES_3, i24_max_below), IPX_OK);
	*((uint32_t *) temp32) = htonl(i24_max_below);
	EXPECT_EQ(memcmp(i24, &temp32[1], BYTES_3), 0);

	EXPECT_EQ(ipx_set_int_be(i24, BYTES_3, i24_min_above), IPX_OK);
	*((uint32_t *) temp32) = htonl(i24_min_above);
	EXPECT_EQ(memcmp(i24, &temp32[1], BYTES_3), 0);
	EXPECT_EQ(ipx_set_int_be(i24, BYTES_3, IPX_INT24_MIN), IPX_OK);
	*((uint32_t *) temp32) = htonl(IPX_INT24_MIN);
	EXPECT_EQ(memcmp(i24, &temp32[1], BYTES_3), 0);
	EXPECT_EQ(ipx_set_int_be(i24, BYTES_3, i24_min_below), IPX_ERR_TRUNC);
	*((uint32_t *) temp32) = htonl(IPX_INT24_MIN);
	EXPECT_EQ(memcmp(i24, &temp32[1], BYTES_3), 0);

	EXPECT_EQ(ipx_set_int_be(i24, BYTES_3, zero_above), IPX_OK);
	*((uint32_t *) temp32) = htonl((int32_t) zero_above);
	EXPECT_EQ(memcmp(i24, &temp32[1], BYTES_3), 0);
	EXPECT_EQ(ipx_set_int_be(i24, BYTES_3, zero), IPX_OK);
	*((uint32_t *) temp32) = htonl((int32_t) zero);
	EXPECT_EQ(memcmp(i24, &temp32[1], BYTES_3), 0);
	EXPECT_EQ(ipx_set_int_be(i24, BYTES_3, zero_below), IPX_OK);
	*((uint32_t *) temp32) = htonl((int32_t) zero_below);
	EXPECT_EQ(memcmp(i24, &temp32[1], BYTES_3), 0);

	// 5 bytes
	EXPECT_EQ(ipx_set_int_be(i40, BYTES_5, i40_max_above), IPX_ERR_TRUNC);
	*((uint64_t *) temp64) = htobe64(IPX_INT40_MAX);
	EXPECT_EQ(memcmp(i40, &temp64[3], BYTES_5), 0);
	EXPECT_EQ(ipx_set_int_be(i40, BYTES_5, IPX_INT40_MAX), IPX_OK);
	*((uint64_t *) temp64) = htobe64(IPX_INT40_MAX);
	EXPECT_EQ(memcmp(i40, &temp64[3], BYTES_5), 0);
	EXPECT_EQ(ipx_set_int_be(i40, BYTES_5, i40_max_below), IPX_OK);
	*((uint64_t *) temp64) = htobe64(i40_max_below);
	EXPECT_EQ(memcmp(i40, &temp64[3], BYTES_5), 0);

	EXPECT_EQ(ipx_set_int_be(i40, BYTES_5, i40_min_above), IPX_OK);
	*((uint64_t *) temp64) = htobe64(i40_min_above);
	EXPECT_EQ(memcmp(i40, &temp64[3], BYTES_5), 0);
	EXPECT_EQ(ipx_set_int_be(i40, BYTES_5, IPX_INT40_MIN), IPX_OK);
	*((uint64_t *) temp64) = htobe64(IPX_INT40_MIN);
	EXPECT_EQ(memcmp(i40, &temp64[3], BYTES_5), 0);
	EXPECT_EQ(ipx_set_int_be(i40, BYTES_5, i40_min_below), IPX_ERR_TRUNC);
	*((uint64_t *) temp64) = htobe64(IPX_INT40_MIN);
	EXPECT_EQ(memcmp(i40, &temp64[3], BYTES_5), 0);

	EXPECT_EQ(ipx_set_int_be(i40, BYTES_5, zero_above), IPX_OK);
	*((uint64_t *) temp64) = htobe64((int64_t) zero_above);
	EXPECT_EQ(memcmp(i40, &temp64[3], BYTES_5), 0);
	EXPECT_EQ(ipx_set_int_be(i40, BYTES_5, zero), IPX_OK);
	*((uint64_t *) temp64) = htobe64((int64_t) zero);
	EXPECT_EQ(memcmp(i40, &temp64[3], BYTES_5), 0);
	EXPECT_EQ(ipx_set_int_be(i40, BYTES_5, zero_below), IPX_OK);
	*((uint64_t *) temp64) = htobe64((int64_t) zero_below);
	EXPECT_EQ(memcmp(i40, &temp64[3], BYTES_5), 0);

	// 6 bytes
	EXPECT_EQ(ipx_set_int_be(i48, BYTES_6, i48_max_above), IPX_ERR_TRUNC);
	*((uint64_t *) temp64) = htobe64(IPX_INT48_MAX);
	EXPECT_EQ(memcmp(i48, &temp64[2], BYTES_6), 0);
	EXPECT_EQ(ipx_set_int_be(i48, BYTES_6, IPX_INT48_MAX), IPX_OK);
	*((uint64_t *) temp64) = htobe64(IPX_INT48_MAX);
	EXPECT_EQ(memcmp(i48, &temp64[2], BYTES_6), 0);
	EXPECT_EQ(ipx_set_int_be(i48, BYTES_6, i48_max_below), IPX_OK);
	*((uint64_t *) temp64) = htobe64(i48_max_below);
	EXPECT_EQ(memcmp(i48, &temp64[2], BYTES_6), 0);

	EXPECT_EQ(ipx_set_int_be(i48, BYTES_6, i48_min_above), IPX_OK);
	*((uint64_t *) temp64) = htobe64(i48_min_above);
	EXPECT_EQ(memcmp(i48, &temp64[2], BYTES_6), 0);
	EXPECT_EQ(ipx_set_int_be(i48, BYTES_6, IPX_INT48_MIN), IPX_OK);
	*((uint64_t *) temp64) = htobe64(IPX_INT48_MIN);
	EXPECT_EQ(memcmp(i48, &temp64[2], BYTES_6), 0);
	EXPECT_EQ(ipx_set_int_be(i48, BYTES_6, i48_min_below), IPX_ERR_TRUNC);
	*((uint64_t *) temp64) = htobe64(IPX_INT48_MIN);
	EXPECT_EQ(memcmp(i48, &temp64[2], BYTES_6), 0);

	EXPECT_EQ(ipx_set_int_be(i48, BYTES_6, zero_above), IPX_OK);
	*((uint64_t *) temp64) = htobe64((int64_t) zero_above);
	EXPECT_EQ(memcmp(i48, &temp64[2], BYTES_6), 0);
	EXPECT_EQ(ipx_set_int_be(i48, BYTES_6, zero), IPX_OK);
	*((uint64_t *) temp64) = htobe64((int64_t) zero);
	EXPECT_EQ(memcmp(i48, &temp64[2], BYTES_6), 0);
	EXPECT_EQ(ipx_set_int_be(i48, BYTES_6, zero_below), IPX_OK);
	*((uint64_t *) temp64) = htobe64((int64_t) zero_below);
	EXPECT_EQ(memcmp(i48, &temp64[2], BYTES_6), 0);

	// 7 bytes
	EXPECT_EQ(ipx_set_int_be(i56, BYTES_7, i56_max_above), IPX_ERR_TRUNC);
	*((uint64_t *) temp64) = htobe64(IPX_INT56_MAX);
	EXPECT_EQ(memcmp(i56, &temp64[1], BYTES_7), 0);
	EXPECT_EQ(ipx_set_int_be(i56, BYTES_7, IPX_INT56_MAX), IPX_OK);
	*((uint64_t *) temp64) = htobe64(IPX_INT56_MAX);
	EXPECT_EQ(memcmp(i56, &temp64[1], BYTES_7), 0);
	EXPECT_EQ(ipx_set_int_be(i56, BYTES_7, i56_max_below), IPX_OK);
	*((uint64_t *) temp64) = htobe64(i56_max_below);
	EXPECT_EQ(memcmp(i56, &temp64[1], BYTES_7), 0);

	EXPECT_EQ(ipx_set_int_be(i56, BYTES_7, i56_min_above), IPX_OK);
	*((uint64_t *) temp64) = htobe64(i56_min_above);
	EXPECT_EQ(memcmp(i56, &temp64[1], BYTES_7), 0);
	EXPECT_EQ(ipx_set_int_be(i56, BYTES_7, IPX_INT56_MIN), IPX_OK);
	*((uint64_t *) temp64) = htobe64(IPX_INT56_MIN);
	EXPECT_EQ(memcmp(i56, &temp64[1], BYTES_7), 0);
	EXPECT_EQ(ipx_set_int_be(i56, BYTES_7, i56_min_below), IPX_ERR_TRUNC);
	*((uint64_t *) temp64) = htobe64(IPX_INT56_MIN);
	EXPECT_EQ(memcmp(i56, &temp64[1], BYTES_7), 0);

	EXPECT_EQ(ipx_set_int_be(i56, BYTES_7, zero_above), IPX_OK);
	*((uint64_t *) temp64) = htobe64((int64_t) zero_above);
	EXPECT_EQ(memcmp(i56, &temp64[1], BYTES_7), 0);
	EXPECT_EQ(ipx_set_int_be(i56, BYTES_7, zero), IPX_OK);
	*((uint64_t *) temp64) = htobe64((int64_t) zero);
	EXPECT_EQ(memcmp(i56, &temp64[1], BYTES_7), 0);
	EXPECT_EQ(ipx_set_int_be(i56, BYTES_7, zero_below), IPX_OK);
	*((uint64_t *) temp64) = htobe64((int64_t) zero_below);
	EXPECT_EQ(memcmp(i56, &temp64[1], BYTES_7), 0);
}


/*
 * "Random" values in the valid interval for 1 - 8 bytes unsigned values
 */
TEST_F(ConverterInt, SetIntInRandom)
{
	// 1 byte
	const int8_t i8_rand1 = -102;
	const int8_t i8_rand2 =  -50;
	const int8_t i8_rand3 =   24;
	const int8_t i8_rand4 =  115;
	EXPECT_EQ(ipx_set_int_be(i8, BYTES_1, i8_rand1), IPX_OK);
	EXPECT_EQ(*i8, i8_rand1);
	EXPECT_EQ(ipx_set_int_be(i8, BYTES_1, i8_rand2), IPX_OK);
	EXPECT_EQ(*i8, i8_rand2);
	EXPECT_EQ(ipx_set_int_be(i8, BYTES_1, i8_rand3), IPX_OK);
	EXPECT_EQ(*i8, i8_rand3);
	EXPECT_EQ(ipx_set_int_be(i8, BYTES_1, i8_rand4), IPX_OK);
	EXPECT_EQ(*i8, i8_rand4);

	// 2 bytes
	const int16_t i16_rand1 = -24854;
	const int16_t i16_rand2 =  -5120;
	const int16_t i16_rand3 =  16542;
	const int16_t i16_rand4 =  27858;
	EXPECT_EQ(ipx_set_int_be(i16, BYTES_2, i16_rand1), IPX_OK);
	EXPECT_EQ((int16_t) ntohs(*i16), i16_rand1);
	EXPECT_EQ(ipx_set_int_be(i16, BYTES_2, i16_rand2), IPX_OK);
	EXPECT_EQ((int16_t) ntohs(*i16), i16_rand2);
	EXPECT_EQ(ipx_set_int_be(i16, BYTES_2, i16_rand3), IPX_OK);
	EXPECT_EQ((int16_t) ntohs(*i16), i16_rand3);
	EXPECT_EQ(ipx_set_int_be(i16, BYTES_2, i16_rand4), IPX_OK);
	EXPECT_EQ((int16_t) ntohs(*i16), i16_rand4);

	// 4 bytes
	const int32_t i32_rand1 = -2044382111L;
	const int32_t i32_rand2 =    -9254501L;
	const int32_t i32_rand3 =      544554L;
	const int32_t i32_rand4 =  1523208977L;
	EXPECT_EQ(ipx_set_int_be(i32, BYTES_4, i32_rand1), IPX_OK);
	EXPECT_EQ((int32_t) ntohl(*i32), i32_rand1);
	EXPECT_EQ(ipx_set_int_be(i32, BYTES_4, i32_rand2), IPX_OK);
	EXPECT_EQ((int32_t) ntohl(*i32), i32_rand2);
	EXPECT_EQ(ipx_set_int_be(i32, BYTES_4, i32_rand3), IPX_OK);
	EXPECT_EQ((int32_t) ntohl(*i32), i32_rand3);
	EXPECT_EQ(ipx_set_int_be(i32, BYTES_4, i32_rand4), IPX_OK);
	EXPECT_EQ((int32_t) ntohl(*i32), i32_rand4);

	// 8 bytes
	const int64_t i64_rand1 = -5647897131547987134LL;
	const int64_t i64_rand2 =    -5668713216840254LL;
	const int64_t i64_rand3 =        4687125544554LL;
	const int64_t i64_rand4 =  8792165454120271047LL;
	EXPECT_EQ(ipx_set_int_be(i64, BYTES_8, i64_rand1), IPX_OK);
	EXPECT_EQ((int64_t) be64toh(*i64), i64_rand1);
	EXPECT_EQ(ipx_set_int_be(i64, BYTES_8, i64_rand2), IPX_OK);
	EXPECT_EQ((int64_t) be64toh(*i64), i64_rand2);
	EXPECT_EQ(ipx_set_int_be(i64, BYTES_8, i64_rand3), IPX_OK);
	EXPECT_EQ((int64_t) be64toh(*i64), i64_rand3);
	EXPECT_EQ(ipx_set_int_be(i64, BYTES_8, i64_rand4), IPX_OK);
	EXPECT_EQ((int64_t) be64toh(*i64), i64_rand4);

	// Other (unusual situations i.e. 3, 5, 6 and 7 bytes)
	int8_t temp32[4];
	int8_t temp64[8];

	// 3 bytes
	const int32_t i24_rand1 = -7165410L;
	const int32_t i24_rand2 =   -54547L;
	const int32_t i24_rand3 =   478455L;
	const int32_t i24_rand4 =  4518712L;
	EXPECT_EQ(ipx_set_int_be(i24, BYTES_3, i24_rand1), IPX_OK);  // Rand 1
	*((uint32_t *) temp32) = htonl(i24_rand1);
	EXPECT_EQ(memcmp(i24, &temp32[1], BYTES_3), 0);
	EXPECT_EQ(ipx_set_int_be(i24, BYTES_3, i24_rand2), IPX_OK);  // Rand 2
	*((uint32_t *) temp32) = htonl(i24_rand2);
	EXPECT_EQ(memcmp(i24, &temp32[1], BYTES_3), 0);
	EXPECT_EQ(ipx_set_int_be(i24, BYTES_3, i24_rand3), IPX_OK);  // Rand 3
	*((uint32_t *) temp32) = htonl(i24_rand3);
	EXPECT_EQ(memcmp(i24, &temp32[1], BYTES_3), 0);
	EXPECT_EQ(ipx_set_int_be(i24, BYTES_3, i24_rand4), IPX_OK);  // Rand 4
	*((uint32_t *) temp32) = htonl(i24_rand4);
	EXPECT_EQ(memcmp(i24, &temp32[1], BYTES_3), 0);

	// 5 bytes
	const int64_t i40_rand1 = -423012588921LL;
	const int64_t i40_rand2 =    -452102107LL;
	const int64_t i40_rand3 =    2313510007LL;
	const int64_t i40_rand4 =  203234869894LL;
	EXPECT_EQ(ipx_set_int_be(i40, BYTES_5, i40_rand1), IPX_OK);  // Rand 1
	*((uint64_t *) temp64) = htobe64(i40_rand1);
	EXPECT_EQ(memcmp(i40, &temp64[3], BYTES_5), 0);
	EXPECT_EQ(ipx_set_int_be(i40, BYTES_5, i40_rand2), IPX_OK);  // Rand 2
	*((uint64_t *) temp64) = htobe64(i40_rand2);
	EXPECT_EQ(memcmp(i40, &temp64[3], BYTES_5), 0);
	EXPECT_EQ(ipx_set_int_be(i40, BYTES_5, i40_rand3), IPX_OK);  // Rand 3
	*((uint64_t *) temp64) = htobe64(i40_rand3);
	EXPECT_EQ(memcmp(i40, &temp64[3], BYTES_5), 0);
	EXPECT_EQ(ipx_set_int_be(i40, BYTES_5, i40_rand4), IPX_OK);  // Rand 4
	*((uint64_t *) temp64) = htobe64(i40_rand4);
	EXPECT_EQ(memcmp(i40, &temp64[3], BYTES_5), 0);

	// 6 bytes
	const int64_t i48_rand1 = -102364510354981LL;
	const int64_t i48_rand2 =    -213535351004LL;
	const int64_t i48_rand3 =       1242136586LL;
	const int64_t i48_rand4 =   80256465413247LL;
	EXPECT_EQ(ipx_set_int_be(i48, BYTES_6, i48_rand1), IPX_OK);  // Rand 1
	*((uint64_t *) temp64) = htobe64(i48_rand1);
	EXPECT_EQ(memcmp(i48, &temp64[2], BYTES_6), 0);
	EXPECT_EQ(ipx_set_int_be(i48, BYTES_6, i48_rand2), IPX_OK);  // Rand 2
	*((uint64_t *) temp64) = htobe64(i48_rand2);
	EXPECT_EQ(memcmp(i48, &temp64[2], BYTES_6), 0);
	EXPECT_EQ(ipx_set_int_be(i48, BYTES_6, i48_rand3), IPX_OK);  // Rand 3
	*((uint64_t *) temp64) = htobe64(i48_rand3);
	EXPECT_EQ(memcmp(i48, &temp64[2], BYTES_6), 0);
	EXPECT_EQ(ipx_set_int_be(i48, BYTES_6, i48_rand4), IPX_OK);  // Rand 4
	*((uint64_t *) temp64) = htobe64(i48_rand4);
	EXPECT_EQ(memcmp(i48, &temp64[2], BYTES_6), 0);

	// 7 bytes
	const int64_t i56_rand1 = -21080498120778701LL;
	const int64_t i56_rand2 =     -4101202471240LL;
	const int64_t i56_rand3 =        14688791411LL;
	const int64_t i56_rand4 =   4875421204710279LL;
	EXPECT_EQ(ipx_set_int_be(i56, BYTES_7, i56_rand1), IPX_OK);  // Rand 1
	*((uint64_t *) temp64) = htobe64(i56_rand1);
	EXPECT_EQ(memcmp(i56, &temp64[1], BYTES_7), 0);
	EXPECT_EQ(ipx_set_int_be(i56, BYTES_7, i56_rand2), IPX_OK);  // Rand 2
	*((uint64_t *) temp64) = htobe64(i56_rand2);
	EXPECT_EQ(memcmp(i56, &temp64[1], BYTES_7), 0);
	EXPECT_EQ(ipx_set_int_be(i56, BYTES_7, i56_rand3), IPX_OK);  // Rand 3
	*((uint64_t *) temp64) = htobe64(i56_rand3);
	EXPECT_EQ(memcmp(i56, &temp64[1], BYTES_7), 0);
	EXPECT_EQ(ipx_set_int_be(i56, BYTES_7, i56_rand4), IPX_OK);  // Rand 4
	*((uint64_t *) temp64) = htobe64(i56_rand4);
	EXPECT_EQ(memcmp(i56, &temp64[1], BYTES_7), 0);
}

/*
 * Test unsupported size of data fields
 */
TEST_F(ConverterInt, SetIntOutOfRange)
{
	const int64_t value = -123456LL; // Just random number

	// Just random sizes of arrays
	const size_t temp128_size = 16;
	const uint8_t c_temp128[temp128_size] =
		{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
	uint8_t temp128[temp128_size];
	std::memcpy(temp128, c_temp128, temp128_size);

	EXPECT_EQ(ipx_set_int_be(temp128, 0, value), IPX_ERR_ARG);
	EXPECT_EQ(memcmp(temp128, c_temp128, temp128_size), 0);
	EXPECT_EQ(ipx_set_int_be(temp128, 9, value), IPX_ERR_ARG);
	EXPECT_EQ(memcmp(temp128, c_temp128, temp128_size), 0);
	EXPECT_EQ(ipx_set_int_be(temp128, 11, value), IPX_ERR_ARG);
	EXPECT_EQ(memcmp(temp128, c_temp128, temp128_size), 0);
	EXPECT_EQ(ipx_set_int_be(temp128, 16, value), IPX_ERR_ARG);
	EXPECT_EQ(memcmp(temp128, c_temp128, temp128_size), 0);
	EXPECT_EQ(ipx_set_int_be(temp128, 24, value), IPX_ERR_ARG);
	EXPECT_EQ(memcmp(temp128, c_temp128, temp128_size), 0);
	EXPECT_EQ(ipx_set_int_be(temp128, 32, value), IPX_ERR_ARG);
	EXPECT_EQ(memcmp(temp128, c_temp128, temp128_size), 0);
}

/*
 * Test getter for maximum and minimum values
 */
TEST_F(ConverterInt, GetIntMaxMin)
{
	int64_t conv_res;

	// 1 byte
	EXPECT_EQ(ipx_set_int_be(i8, BYTES_1, INT8_MAX), IPX_OK); // Max
	EXPECT_EQ(ipx_get_int_be(i8, BYTES_1, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, (int8_t) INT8_MAX);

	EXPECT_EQ(ipx_set_int_be(i8, BYTES_1, INT8_MIN), IPX_OK); // Min
	EXPECT_EQ(ipx_get_int_be(i8, BYTES_1, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, (int8_t) INT8_MIN);

	// 2 byte
	EXPECT_EQ(ipx_set_int_be(i16, BYTES_2, INT16_MAX), IPX_OK); // Max
	EXPECT_EQ(ipx_get_int_be(i16, BYTES_2, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, (int16_t) INT16_MAX);

	EXPECT_EQ(ipx_set_int_be(i16, BYTES_2, INT16_MIN), IPX_OK); // Min
	EXPECT_EQ(ipx_get_int_be(i16, BYTES_2, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, (int16_t) INT16_MIN);

	// 4 byte
	EXPECT_EQ(ipx_set_int_be(i32, BYTES_4, INT32_MAX), IPX_OK); // Max
	EXPECT_EQ(ipx_get_int_be(i32, BYTES_4, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, (int32_t) INT32_MAX);

	EXPECT_EQ(ipx_set_int_be(i32, BYTES_4, INT32_MIN), IPX_OK); // Min
	EXPECT_EQ(ipx_get_int_be(i32, BYTES_4, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, (int32_t) INT32_MIN);

	// 8 byte
	EXPECT_EQ(ipx_set_int_be(i64, BYTES_8, INT64_MAX), IPX_OK); // Max
	EXPECT_EQ(ipx_get_int_be(i64, BYTES_8, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, (int64_t) INT64_MAX);

	EXPECT_EQ(ipx_set_int_be(i64, BYTES_8, INT64_MIN), IPX_OK); // Min
	EXPECT_EQ(ipx_get_int_be(i64, BYTES_8, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, (int64_t) INT64_MIN);

	// Other (unusual situations i.e. 3, 5, 6 and 7 bytes)
	// 3 bytes
	EXPECT_EQ(ipx_set_int_be(i24, BYTES_3, IPX_INT24_MAX), IPX_OK); // Max
	EXPECT_EQ(ipx_get_int_be(i24, BYTES_3, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, IPX_INT24_MAX);

	EXPECT_EQ(ipx_set_int_be(i24, BYTES_3, IPX_INT24_MIN), IPX_OK); // Min
	EXPECT_EQ(ipx_get_int_be(i24, BYTES_3, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, IPX_INT24_MIN);

	// 5 bytes
	EXPECT_EQ(ipx_set_int_be(i40, BYTES_5, IPX_INT40_MAX), IPX_OK); // Max
	EXPECT_EQ(ipx_get_int_be(i40, BYTES_5, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, IPX_INT40_MAX);

	EXPECT_EQ(ipx_set_int_be(i40, BYTES_5, IPX_INT40_MIN), IPX_OK); // Min
	EXPECT_EQ(ipx_get_int_be(i40, BYTES_5, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, IPX_INT40_MIN);

	// 6 bytes
	EXPECT_EQ(ipx_set_int_be(i48, BYTES_6, IPX_INT48_MAX), IPX_OK); // Max
	EXPECT_EQ(ipx_get_int_be(i48, BYTES_6, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, IPX_INT48_MAX);

	EXPECT_EQ(ipx_set_int_be(i48, BYTES_6, IPX_INT48_MIN), IPX_OK); // Min
	EXPECT_EQ(ipx_get_int_be(i48, BYTES_6, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, IPX_INT48_MIN);

	// 7 bytes
	EXPECT_EQ(ipx_set_int_be(i56, BYTES_7, IPX_INT56_MAX), IPX_OK); // Max
	EXPECT_EQ(ipx_get_int_be(i56, BYTES_7, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, IPX_INT56_MAX);

	EXPECT_EQ(ipx_set_int_be(i56, BYTES_7, IPX_INT56_MIN), IPX_OK); // Min
	EXPECT_EQ(ipx_get_int_be(i56, BYTES_7, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, IPX_INT56_MIN);
}

/*
 * Test getter for random values in the interval
 */
TEST_F(ConverterInt, GetIntRandom)
{
	int64_t conv_res;

	// 1 byte
	const int8_t i8_rand1 = -78;
	const int8_t i8_rand2 =  -5;
	const int8_t i8_rand3 =  56;
	const int8_t i8_rand4 =  89;
	EXPECT_EQ(ipx_set_int_be(i8, BYTES_1, i8_rand1), IPX_OK); // Rand 1
	EXPECT_EQ(ipx_get_int_be(i8, BYTES_1, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, i8_rand1);
	EXPECT_EQ(ipx_set_int_be(i8, BYTES_1, i8_rand2), IPX_OK); // Rand 2
	EXPECT_EQ(ipx_get_int_be(i8, BYTES_1, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, i8_rand2);
	EXPECT_EQ(ipx_set_int_be(i8, BYTES_1, i8_rand3), IPX_OK); // Rand 3
	EXPECT_EQ(ipx_get_int_be(i8, BYTES_1, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, i8_rand3);
	EXPECT_EQ(ipx_set_int_be(i8, BYTES_1, i8_rand4), IPX_OK); // Rand 4
	EXPECT_EQ(ipx_get_int_be(i8, BYTES_1, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, i8_rand4);

	// 2 bytes
	const int16_t i16_rand1 = -18987;
	const int16_t i16_rand2 =   -879;
	const int16_t i16_rand3 =  10124;
	const int16_t i16_rand4 =  22033;
	EXPECT_EQ(ipx_set_int_be(i16, BYTES_2, i16_rand1), IPX_OK); // Rand 1
	EXPECT_EQ(ipx_get_int_be(i16, BYTES_2, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, i16_rand1);
	EXPECT_EQ(ipx_set_int_be(i16, BYTES_2, i16_rand2), IPX_OK); // Rand 2
	EXPECT_EQ(ipx_get_int_be(i16, BYTES_2, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, i16_rand2);
	EXPECT_EQ(ipx_set_int_be(i16, BYTES_2, i16_rand3), IPX_OK); // Rand 3
	EXPECT_EQ(ipx_get_int_be(i16, BYTES_2, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, i16_rand3);
	EXPECT_EQ(ipx_set_int_be(i16, BYTES_2, i16_rand4), IPX_OK); // Rand 4
	EXPECT_EQ(ipx_get_int_be(i16, BYTES_2, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, i16_rand4);

	// 4 bytes
	const int32_t i32_rand1 = -1985468745L;
	const int32_t i32_rand2 =    -2351536L;
	const int32_t i32_rand3 =      155651L;
	const int32_t i32_rand4 =   965477985L;
	EXPECT_EQ(ipx_set_int_be(i32, BYTES_4, i32_rand1), IPX_OK); // Rand 1
	EXPECT_EQ(ipx_get_int_be(i32, BYTES_4, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, i32_rand1);
	EXPECT_EQ(ipx_set_int_be(i32, BYTES_4, i32_rand2), IPX_OK); // Rand 2
	EXPECT_EQ(ipx_get_int_be(i32, BYTES_4, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, i32_rand2);
	EXPECT_EQ(ipx_set_int_be(i32, BYTES_4, i32_rand3), IPX_OK); // Rand 3
	EXPECT_EQ(ipx_get_int_be(i32, BYTES_4, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, i32_rand3);
	EXPECT_EQ(ipx_set_int_be(i32, BYTES_4, i32_rand4), IPX_OK); // Rand 4
	EXPECT_EQ(ipx_get_int_be(i32, BYTES_4, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, i32_rand4);

	// 8 bytes
	const int64_t i64_rand1 = -5565163879885325165LL;
	const int64_t i64_rand2 =      -12357887981021LL;
	const int64_t i64_rand3 =             65468810LL;
	const int64_t i64_rand4 =      568848400000012LL;
	EXPECT_EQ(ipx_set_int_be(i64, BYTES_8, i64_rand1), IPX_OK); // Rand 1
	EXPECT_EQ(ipx_get_int_be(i64, BYTES_8, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, i64_rand1);
	EXPECT_EQ(ipx_set_int_be(i64, BYTES_8, i64_rand2), IPX_OK); // Rand 2
	EXPECT_EQ(ipx_get_int_be(i64, BYTES_8, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, i64_rand2);
	EXPECT_EQ(ipx_set_int_be(i64, BYTES_8, i64_rand3), IPX_OK); // Rand 3
	EXPECT_EQ(ipx_get_int_be(i64, BYTES_8, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, i64_rand3);
	EXPECT_EQ(ipx_set_int_be(i64, BYTES_8, i64_rand4), IPX_OK); // Rand 4
	EXPECT_EQ(ipx_get_int_be(i64, BYTES_8, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, i64_rand4);

	// Other (unusual situations i.e. 3, 5, 6 and 7 bytes)
	// 3 bytes
	const int32_t i24_rand1 = -1688987L;
	const int32_t i24_rand2 =     -156L;
	const int32_t i24_rand3 =   168897L;
	const int32_t i24_rand4 =  7056878L;
	EXPECT_EQ(ipx_set_int_be(i24, BYTES_3, i24_rand1), IPX_OK); // Rand 1
	EXPECT_EQ(ipx_get_int_be(i24, BYTES_3, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, i24_rand1);
	EXPECT_EQ(ipx_set_int_be(i24, BYTES_3, i24_rand2), IPX_OK); // Rand 2
	EXPECT_EQ(ipx_get_int_be(i24, BYTES_3, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, i24_rand2);
	EXPECT_EQ(ipx_set_int_be(i24, BYTES_3, i24_rand3), IPX_OK); // Rand 3
	EXPECT_EQ(ipx_get_int_be(i24, BYTES_3, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, i24_rand3);
	EXPECT_EQ(ipx_set_int_be(i24, BYTES_3, i24_rand4), IPX_OK); // Rand 4
	EXPECT_EQ(ipx_get_int_be(i24, BYTES_3, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, i24_rand4);

	// 5 bytes
	const int64_t i40_rand1 = -123456789223LL;
	const int64_t i40_rand2 =   -1567881320LL;
	const int64_t i40_rand3 =       2167897LL;
	const int64_t i40_rand4 =  323205154498LL;
	EXPECT_EQ(ipx_set_int_be(i40, BYTES_5, i40_rand1), IPX_OK); // Rand 1
	EXPECT_EQ(ipx_get_int_be(i40, BYTES_5, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, i40_rand1);
	EXPECT_EQ(ipx_set_int_be(i40, BYTES_5, i40_rand2), IPX_OK); // Rand 2
	EXPECT_EQ(ipx_get_int_be(i40, BYTES_5, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, i40_rand2);
	EXPECT_EQ(ipx_set_int_be(i40, BYTES_5, i40_rand3), IPX_OK); // Rand 3
	EXPECT_EQ(ipx_get_int_be(i40, BYTES_5, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, i40_rand3);
	EXPECT_EQ(ipx_set_int_be(i40, BYTES_5, i40_rand4), IPX_OK); // Rand 4
	EXPECT_EQ(ipx_get_int_be(i40, BYTES_5, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, i40_rand4);

	// 6 bytes
	const int64_t i48_rand1 =   -2135898412234LL;
	const int64_t i48_rand2 =        -21304788LL;
	const int64_t i48_rand3 =         56489897LL;
	const int64_t i48_rand4 =  100002654681452LL;
	EXPECT_EQ(ipx_set_int_be(i48, BYTES_6, i48_rand1), IPX_OK); // Rand 1
	EXPECT_EQ(ipx_get_int_be(i48, BYTES_6, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, i48_rand1);
	EXPECT_EQ(ipx_set_int_be(i48, BYTES_6, i48_rand2), IPX_OK); // Rand 2
	EXPECT_EQ(ipx_get_int_be(i48, BYTES_6, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, i48_rand2);
	EXPECT_EQ(ipx_set_int_be(i48, BYTES_6, i48_rand3), IPX_OK); // Rand 3
	EXPECT_EQ(ipx_get_int_be(i48, BYTES_6, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, i48_rand3);
	EXPECT_EQ(ipx_set_int_be(i48, BYTES_6, i48_rand4), IPX_OK); // Rand 4
	EXPECT_EQ(ipx_get_int_be(i48, BYTES_6, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, i48_rand4);

	// 7 bytes
	const int64_t i56_rand1 =  -9178813217894101LL;
	const int64_t i56_rand2 =     -1232320787412LL;
	const int64_t i56_rand3 =          567899720LL;
	const int64_t i56_rand4 =  12688987230320574LL;
	EXPECT_EQ(ipx_set_int_be(i56, BYTES_7, i56_rand1), IPX_OK); // Rand 1
	EXPECT_EQ(ipx_get_int_be(i56, BYTES_7, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, i56_rand1);
	EXPECT_EQ(ipx_set_int_be(i56, BYTES_7, i56_rand2), IPX_OK); // Rand 2
	EXPECT_EQ(ipx_get_int_be(i56, BYTES_7, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, i56_rand2);
	EXPECT_EQ(ipx_set_int_be(i56, BYTES_7, i56_rand3), IPX_OK); // Rand 3
	EXPECT_EQ(ipx_get_int_be(i56, BYTES_7, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, i56_rand3);
	EXPECT_EQ(ipx_set_int_be(i56, BYTES_7, i56_rand4), IPX_OK); // Rand 4
	EXPECT_EQ(ipx_get_int_be(i56, BYTES_7, &conv_res), IPX_OK);
	EXPECT_EQ(conv_res, i56_rand4);
}

/*
 * Test unsupported size of data fields
 */
TEST_F(ConverterInt, GetIntOutOfRange)
{
	const int64_t c_value = 1234567890123456789LL; // Just random number
	int64_t value = c_value;
	void *ptr = NULL; // Invalid pointer

	// Just random sizes of arrays
	const size_t temp72_size =   9;
	const size_t temp88_size =  11;
	const size_t temp128_size = 16;
	const size_t temp192_size = 24;
	const size_t temp256_size = 32;

	int8_t temp72[temp72_size];
	int8_t temp88[temp88_size];
	int8_t temp128[temp128_size];
	int8_t temp192[temp192_size];
	int8_t temp256[temp256_size];

	// Valgrind will possibly report an error when the arrays are accessed.
	EXPECT_EQ(ipx_get_int_be(temp72, 0U, &value), IPX_ERR_ARG);
	EXPECT_TRUE(value == c_value);
	EXPECT_EQ(ipx_get_int_be(temp72, temp72_size, &value), IPX_ERR_ARG);
	EXPECT_TRUE(value == c_value);
	EXPECT_EQ(ipx_get_int_be(temp88, temp88_size, &value), IPX_ERR_ARG);
	EXPECT_TRUE(value == c_value);
	EXPECT_EQ(ipx_get_int_be(temp128, temp128_size, &value), IPX_ERR_ARG);
	EXPECT_TRUE(value == c_value);
	EXPECT_EQ(ipx_get_int_be(temp192, temp192_size, &value), IPX_ERR_ARG);
	EXPECT_TRUE(value == c_value);
	EXPECT_EQ(ipx_get_int_be(temp256, temp256_size, &value), IPX_ERR_ARG);
	EXPECT_TRUE(value == c_value);

	EXPECT_EQ(ipx_get_int_be(ptr, 0U, &value), IPX_ERR_ARG);
	EXPECT_TRUE(value == c_value);
	EXPECT_EQ(ipx_get_int_be(ptr, temp72_size, &value), IPX_ERR_ARG);
	EXPECT_TRUE(value == c_value);
	EXPECT_EQ(ipx_get_int_be(ptr, temp88_size, &value), IPX_ERR_ARG);
	EXPECT_TRUE(value == c_value);
	EXPECT_EQ(ipx_get_int_be(ptr, temp128_size, &value), IPX_ERR_ARG);
	EXPECT_TRUE(value == c_value);
	EXPECT_EQ(ipx_get_int_be(ptr, temp192_size, &value), IPX_ERR_ARG);
	EXPECT_TRUE(value == c_value);
	EXPECT_EQ(ipx_get_int_be(ptr, temp256_size, &value), IPX_ERR_ARG);
	EXPECT_TRUE(value == c_value);
}

/**
 * \brief Test fixture for SetUint tests
 */
class ConverterFloat : public ::testing::Test {
protected:
	/*
	 * We need cast helpers to check correctness of data
	 */
	union fcast_u {
		float flt;
		uint32_t uint;
	};

	union dcast_u {
		double flt;
		uint64_t uint;
	};

	/*
	 * We want to have all variables dynamically allocated so Valgrind can check
	 * access out of bounds, etc.
     */
	union fcast_u *cast32;
	union dcast_u *cast64;

	// The positive/negative maximum numbers
	const double flt_max_plus = std::numeric_limits<float>::max();
	const double flt_max_minus = std::numeric_limits<float>::lowest();
	const double dbl_max_plus = std::numeric_limits<double>::max();
	const double dbl_max_minus = std::numeric_limits<double>::lowest();

	// The smallest magnitude number
	const double flt_smallest_plus = std::numeric_limits<float>::min();
	const double flt_smallest_minus = -std::numeric_limits<float>::min();
	const double dbl_smallest_plus = std::numeric_limits<double>::min();
	const double dbl_smallest_minus = -std::numeric_limits<double>::min();

public:
	/** Create variables for tests */
	virtual void SetUp() {
		cast32 = new union fcast_u;
		cast64 = new union dcast_u;
	}

	/** Destroy variables for the tests */
	virtual void TearDown() {
		delete cast32;
		delete cast64;
	}
};

/*
 * Test predicates
 * If these tests are not passed, other floating-point tests are not reliable.
 */
TEST_F(ConverterFloat, Predicate)
{
	EXPECT_FLOAT_EQ(flt_smallest_plus + flt_smallest_minus, 0.0f);
	EXPECT_FLOAT_EQ(flt_max_plus + flt_max_minus, 0.0f);

	EXPECT_DOUBLE_EQ(dbl_smallest_plus + dbl_smallest_minus, 0.0);
	EXPECT_DOUBLE_EQ(dbl_max_plus + dbl_max_minus, 0.0);

	EXPECT_NE(flt_max_plus, 0.0f);
	EXPECT_NE(flt_max_minus, 0.0f);
	EXPECT_NE(dbl_max_plus, 0.0);
	EXPECT_NE(dbl_max_minus, 0.0);

	EXPECT_NE(flt_smallest_plus, 0.0f);
	EXPECT_NE(flt_smallest_minus, 0.0f);
	EXPECT_NE(dbl_smallest_plus, 0.0);
	EXPECT_NE(dbl_smallest_plus, 0.0);
}

/*
 * Insert the maximum possible value i.e. "float64 max", the minimum possible
 * value i.e. "negative float64 max" and the smallest valid values.
 */
TEST_F(ConverterFloat, SetMaxMin)
{
	union fcast_u fcast;
	union dcast_u dcast;

	// 4 bytes float - positive/negative maximum value (out of range)
	EXPECT_EQ(ipx_set_float_be(&cast32->flt, BYTES_4, dbl_max_plus), IPX_ERR_TRUNC);
	fcast.uint = ntohl(cast32->uint);
	EXPECT_FLOAT_EQ(fcast.flt, flt_max_plus);

	EXPECT_EQ(ipx_set_float_be(&cast32->flt, BYTES_4, dbl_max_minus), IPX_ERR_TRUNC);
	fcast.uint = ntohl(cast32->uint);
	EXPECT_FLOAT_EQ(fcast.flt, flt_max_minus);

	// 4 bytes float - positive/negative maximum value (inside range)
	EXPECT_EQ(ipx_set_float_be(&cast32->flt, BYTES_4, flt_max_plus), IPX_OK);
	fcast.uint = ntohl(cast32->uint);
	EXPECT_FLOAT_EQ(fcast.flt, flt_max_plus);

	EXPECT_EQ(ipx_set_float_be(&cast32->flt, BYTES_4, flt_max_minus), IPX_OK);
	fcast.uint = ntohl(cast32->uint);
	EXPECT_FLOAT_EQ(fcast.flt, flt_max_minus);

	// 8 bytes float - positive/negative maximum value
	EXPECT_EQ(ipx_set_float_be(&cast64->flt, BYTES_8, dbl_max_plus), IPX_OK);
	dcast.uint = be64toh(cast64->uint);
	EXPECT_DOUBLE_EQ(dcast.flt, dbl_max_plus);

	EXPECT_EQ(ipx_set_float_be(&cast64->flt, BYTES_8, dbl_max_minus), IPX_OK);
	dcast.uint = be64toh(cast64->uint);
	EXPECT_DOUBLE_EQ(dcast.flt, dbl_max_minus);
}

/*
 * Insert the positive/negative smallest value and the positive and negative zero.
 */
TEST_F(ConverterFloat, SetZeroAndSmallest)
{
	union fcast_u fcast;
	union dcast_u dcast;

	// 4 bytes float - positive/negative zero
	EXPECT_EQ(ipx_set_float_be(&cast32->flt, BYTES_4, 0.0), IPX_OK);
	fcast.uint = ntohl(cast32->uint);
	EXPECT_FLOAT_EQ(fcast.flt, 0.0f);
	EXPECT_FALSE(std::signbit(fcast.flt));

	EXPECT_EQ(ipx_set_float_be(&cast32->flt, BYTES_4, -0.0), IPX_OK);
	fcast.uint = ntohl(cast32->uint);
	EXPECT_FLOAT_EQ(fcast.flt, -0.0f);
	EXPECT_TRUE(std::signbit(fcast.flt));

	// 4 bytes float - the positive/negative smallest value
	EXPECT_EQ(ipx_set_float_be(&cast32->flt, BYTES_4, flt_smallest_plus), IPX_OK);
	fcast.uint = ntohl(cast32->uint);
	EXPECT_FLOAT_EQ(fcast.flt, flt_smallest_plus);

	EXPECT_EQ(ipx_set_float_be(&cast32->flt, BYTES_4, flt_smallest_minus), IPX_OK);
	fcast.uint = ntohl(cast32->uint);
	EXPECT_FLOAT_EQ(fcast.flt, flt_smallest_minus);

	// 8 bytes float - positive/negative zero
	EXPECT_EQ(ipx_set_float_be(&cast64->flt, BYTES_8, 0.0), IPX_OK);
	dcast.uint = be64toh(cast64->uint);
	EXPECT_DOUBLE_EQ(dcast.flt, 0.0);
	EXPECT_FALSE(std::signbit(dcast.flt));

	EXPECT_EQ(ipx_set_float_be(&cast64->flt, BYTES_8, -0.0), IPX_OK);
	dcast.uint = be64toh(cast64->uint);
	EXPECT_DOUBLE_EQ(dcast.flt, -0.0);
	EXPECT_TRUE(std::signbit(dcast.flt));

	// 8 bytes float - the positive/negative smallest value
	EXPECT_EQ(ipx_set_float_be(&cast64->flt, BYTES_8, dbl_smallest_plus), IPX_OK);
	dcast.uint = be64toh(cast64->uint);
	EXPECT_DOUBLE_EQ(dcast.flt, dbl_smallest_plus);

	EXPECT_EQ(ipx_set_float_be(&cast64->flt, BYTES_8, dbl_smallest_minus), IPX_OK);
	dcast.uint = be64toh(cast64->uint);
	EXPECT_DOUBLE_EQ(dcast.flt, dbl_smallest_minus);
}

/*
 * Insert max + 1/max/max - 1 values into float and double variables. In this
 * case the value "1" represents the smallest possible value
 */
TEST_F(ConverterFloat, SetAboveBelow)
{
	const double dbl_eps = std::numeric_limits<double>::epsilon();
	const double dbl_below_max_plus = dbl_max_plus - (dbl_eps * dbl_max_plus);
	const double dbl_above_max_minus = dbl_max_minus - (dbl_eps * dbl_max_minus);

	const double flt_above_max_plus = flt_max_plus + (dbl_eps * flt_max_plus);
	const double flt_above_max_minus = flt_max_minus - (dbl_eps * flt_max_minus);
	const double flt_below_max_plus = flt_max_plus - (dbl_eps * flt_max_plus);
	const double flt_below_max_minus = flt_max_minus + (dbl_eps * flt_max_minus);

	union fcast_u fcast;
	union dcast_u dcast;

	// 4 bytes float
	EXPECT_EQ(ipx_set_float_be(&cast32->flt, BYTES_4, flt_above_max_plus), IPX_ERR_TRUNC);
	fcast.uint = ntohl(cast32->uint);
	EXPECT_FLOAT_EQ(fcast.flt, flt_max_plus);

	EXPECT_EQ(ipx_set_float_be(&cast32->flt, BYTES_4, flt_above_max_minus), IPX_OK);
	fcast.uint = ntohl(cast32->uint);
	EXPECT_FLOAT_EQ(fcast.flt, flt_above_max_minus);

	EXPECT_EQ(ipx_set_float_be(&cast32->flt, BYTES_4, flt_below_max_plus), IPX_OK);
	fcast.uint = ntohl(cast32->uint);
	EXPECT_FLOAT_EQ(fcast.flt, flt_below_max_plus);

	EXPECT_EQ(ipx_set_float_be(&cast32->flt, BYTES_4, flt_below_max_minus), IPX_ERR_TRUNC);
	fcast.uint = ntohl(cast32->uint);
	EXPECT_FLOAT_EQ(fcast.flt, flt_max_minus);

	// 8 bytes float
	EXPECT_EQ(ipx_set_float_be(&cast64->flt, BYTES_8, dbl_below_max_plus), IPX_OK);
	dcast.uint = be64toh(cast64->uint);
	EXPECT_DOUBLE_EQ(dcast.flt, dbl_below_max_plus);

	EXPECT_EQ(ipx_set_float_be(&cast64->flt, BYTES_8, dbl_above_max_minus), IPX_OK);
	dcast.uint = be64toh(cast64->uint);
	EXPECT_DOUBLE_EQ(dcast.flt, dbl_above_max_minus);
}

/*
 * "Random: value in the valid interval for 4/8 bytes float
 */
TEST_F(ConverterFloat, SetRandom)
{
	union fcast_u fcast;
	union dcast_u dcast;

	// 4 bytes
	const float flt_rand1 = 6.897151e+13f;
	const float flt_rand2 = 2.358792e-24f;
	const float flt_rand3 = -8.128795e+12f;
	const float flt_rand4 = -1.897987e-33f;

	EXPECT_EQ(ipx_set_float_be(&cast32->flt, BYTES_4, flt_rand1), IPX_OK);
	fcast.uint = ntohl(cast32->uint);
	EXPECT_FLOAT_EQ(fcast.flt, flt_rand1);

	EXPECT_EQ(ipx_set_float_be(&cast32->flt, BYTES_4, flt_rand2), IPX_OK);
	fcast.uint = ntohl(cast32->uint);
	EXPECT_FLOAT_EQ(fcast.flt, flt_rand2);

	EXPECT_EQ(ipx_set_float_be(&cast32->flt, BYTES_4, flt_rand3), IPX_OK);
	fcast.uint = ntohl(cast32->uint);
	EXPECT_FLOAT_EQ(fcast.flt, flt_rand3);

	EXPECT_EQ(ipx_set_float_be(&cast32->flt, BYTES_4, flt_rand4), IPX_OK);
	fcast.uint = ntohl(cast32->uint);
	EXPECT_FLOAT_EQ(fcast.flt, flt_rand4);

	// 8 bytes
	const double dbl_rand1 = 2.5496842132000588e+101;
	const double dbl_rand2 = 9.4684001478787714e-258;
	const double dbl_rand3 = -1.9999999997898005e+55;
	const double dbl_rand4 = -8.5465460047004713e-146;

	EXPECT_EQ(ipx_set_float_be(&cast64->flt, BYTES_8, dbl_rand1), IPX_OK);
	dcast.uint = be64toh(cast64->uint);
	EXPECT_DOUBLE_EQ(dcast.flt, dbl_rand1);

	EXPECT_EQ(ipx_set_float_be(&cast64->flt, BYTES_8, dbl_rand2), IPX_OK);
	dcast.uint = be64toh(cast64->uint);
	EXPECT_DOUBLE_EQ(dcast.flt, dbl_rand2);

	EXPECT_EQ(ipx_set_float_be(&cast64->flt, BYTES_8, dbl_rand3), IPX_OK);
	dcast.uint = be64toh(cast64->uint);
	EXPECT_DOUBLE_EQ(dcast.flt, dbl_rand3);

	EXPECT_EQ(ipx_set_float_be(&cast64->flt, BYTES_8, dbl_rand4), IPX_OK);
	dcast.uint = be64toh(cast64->uint);
	EXPECT_DOUBLE_EQ(dcast.flt, dbl_rand4);
}

/*
 * Test unsupported size of data fields
 */
TEST_F(ConverterFloat, SetOutOfRange)
{
	const double value = 1.65468e+15;
	const size_t temp128_size = 16;

	const uint8_t c_temp128[temp128_size] =
		{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
	uint8_t temp128[temp128_size];
	std::memcpy(temp128, c_temp128, temp128_size);

	EXPECT_EQ(ipx_set_float_be(temp128, 0, value), IPX_ERR_ARG);
	EXPECT_EQ(memcmp(temp128, c_temp128, temp128_size), 0);
	EXPECT_EQ(ipx_set_float_be(temp128, 1, value), IPX_ERR_ARG);
	EXPECT_EQ(memcmp(temp128, c_temp128, temp128_size), 0);
	EXPECT_EQ(ipx_set_float_be(temp128, 2, value), IPX_ERR_ARG);
	EXPECT_EQ(memcmp(temp128, c_temp128, temp128_size), 0);
	EXPECT_EQ(ipx_set_float_be(temp128, 3, value), IPX_ERR_ARG);
	EXPECT_EQ(memcmp(temp128, c_temp128, temp128_size), 0);
	EXPECT_EQ(ipx_set_float_be(temp128, 5, value), IPX_ERR_ARG);
	EXPECT_EQ(memcmp(temp128, c_temp128, temp128_size), 0);
	EXPECT_EQ(ipx_set_float_be(temp128, 6, value), IPX_ERR_ARG);
	EXPECT_EQ(memcmp(temp128, c_temp128, temp128_size), 0);
	EXPECT_EQ(ipx_set_float_be(temp128, 7, value), IPX_ERR_ARG);
	EXPECT_EQ(memcmp(temp128, c_temp128, temp128_size), 0);
	EXPECT_EQ(ipx_set_float_be(temp128, 9, value), IPX_ERR_ARG);
	EXPECT_EQ(memcmp(temp128, c_temp128, temp128_size), 0);
	EXPECT_EQ(ipx_set_float_be(temp128, 16, value), IPX_ERR_ARG);
	EXPECT_EQ(memcmp(temp128, c_temp128, temp128_size), 0);
}

/*
 * Test getter for maximum and minimum values
 */
TEST_F(ConverterFloat, GetMaxMin)
{
	double conv_res;

	// 4 bytes float
	EXPECT_EQ(ipx_set_float_be(&cast32->flt, BYTES_4, flt_max_plus), IPX_OK);
	EXPECT_EQ(ipx_get_float_be(&cast32->flt, BYTES_4, &conv_res), IPX_OK);
	EXPECT_FLOAT_EQ(conv_res, flt_max_plus);

	EXPECT_EQ(ipx_set_float_be(&cast32->flt, BYTES_4, flt_max_minus), IPX_OK);
	EXPECT_EQ(ipx_get_float_be(&cast32->flt, BYTES_4, &conv_res), IPX_OK);
	EXPECT_FLOAT_EQ(conv_res, flt_max_minus);

	// 8 bytes float
	EXPECT_EQ(ipx_set_float_be(&cast64->flt, BYTES_8, dbl_max_plus), IPX_OK);
	EXPECT_EQ(ipx_get_float_be(&cast64->flt, BYTES_8, &conv_res), IPX_OK);
	EXPECT_DOUBLE_EQ(conv_res, dbl_max_plus);

	EXPECT_EQ(ipx_set_float_be(&cast64->flt, BYTES_8, dbl_max_minus), IPX_OK);
	EXPECT_EQ(ipx_get_float_be(&cast64->flt, BYTES_8, &conv_res), IPX_OK);
	EXPECT_DOUBLE_EQ(conv_res, dbl_max_minus);
}

/*
 * Get the positive/negative smallest value and the positive and negative zero.
 */
TEST_F(ConverterFloat, GetZeroAndSmallest)
{
	double conv_res;

	// 4 bytes float - positive/negative zero
	EXPECT_EQ(ipx_set_float_be(&cast32->flt, BYTES_4, 0.0), IPX_OK);
	EXPECT_EQ(ipx_get_float_be(&cast32->flt, BYTES_4, &conv_res), IPX_OK);
	EXPECT_FLOAT_EQ(conv_res, 0.0f);
	EXPECT_FALSE(std::signbit(conv_res));

	EXPECT_EQ(ipx_set_float_be(&cast32->flt, BYTES_4, -0.0), IPX_OK);
	EXPECT_EQ(ipx_get_float_be(&cast32->flt, BYTES_4, &conv_res), IPX_OK);
	EXPECT_FLOAT_EQ(conv_res, -0.0f);
	EXPECT_TRUE(std::signbit(conv_res));

	// 4 bytes float - the positive/negative smallest value
	EXPECT_EQ(ipx_set_float_be(&cast32->flt, BYTES_4, flt_smallest_plus), IPX_OK);
	EXPECT_EQ(ipx_get_float_be(&cast32->flt, BYTES_4, &conv_res), IPX_OK);
	EXPECT_FLOAT_EQ(conv_res, flt_smallest_plus);

	EXPECT_EQ(ipx_set_float_be(&cast32->flt, BYTES_4, flt_smallest_minus), IPX_OK);
	EXPECT_EQ(ipx_get_float_be(&cast32->flt, BYTES_4, &conv_res), IPX_OK);
	EXPECT_FLOAT_EQ(conv_res, flt_smallest_minus);

	// 8 bytes float - positive/negative zero
	EXPECT_EQ(ipx_set_float_be(&cast64->flt, BYTES_8, 0.0), IPX_OK);
	EXPECT_EQ(ipx_get_float_be(&cast64->flt, BYTES_8, &conv_res), IPX_OK);
	EXPECT_DOUBLE_EQ(conv_res, 0.0);
	EXPECT_FALSE(std::signbit(conv_res));

	EXPECT_EQ(ipx_set_float_be(&cast64->flt, BYTES_8, -0.0), IPX_OK);
	EXPECT_EQ(ipx_get_float_be(&cast64->flt, BYTES_8, &conv_res), IPX_OK);
	EXPECT_DOUBLE_EQ(conv_res, -0.0);
	EXPECT_TRUE(std::signbit(conv_res));

	// 8 bytes float - the positive/negative smallest value
	EXPECT_EQ(ipx_set_float_be(&cast64->flt, BYTES_8, dbl_smallest_plus), IPX_OK);
	EXPECT_EQ(ipx_get_float_be(&cast64->flt, BYTES_8, &conv_res), IPX_OK);
	EXPECT_DOUBLE_EQ(conv_res, dbl_smallest_plus);

	EXPECT_EQ(ipx_set_float_be(&cast64->flt, BYTES_8, dbl_smallest_minus), IPX_OK);
	EXPECT_EQ(ipx_get_float_be(&cast64->flt, BYTES_8, &conv_res), IPX_OK);
	EXPECT_DOUBLE_EQ(conv_res, dbl_smallest_minus);
}

/*
 * Test getter for random values in the interval
 */
TEST_F(ConverterFloat, GetRandom)
{
	double conv_res;

	// 4 bytes float
	const float flt_rand1 = 2.468877e+24f;
	const float flt_rand2 = 9.897987e-2f;
	const float flt_rand3 = -3.123545e+2f;
	const float flt_rand4 = -1.562152e-33f;

	EXPECT_EQ(ipx_set_float_be(&cast32->flt, BYTES_4, flt_rand1), IPX_OK);
	EXPECT_EQ(ipx_get_float_be(&cast32->flt, BYTES_4, &conv_res), IPX_OK);
	EXPECT_FLOAT_EQ(conv_res, flt_rand1);

	EXPECT_EQ(ipx_set_float_be(&cast32->flt, BYTES_4, flt_rand2), IPX_OK);
	EXPECT_EQ(ipx_get_float_be(&cast32->flt, BYTES_4, &conv_res), IPX_OK);
	EXPECT_FLOAT_EQ(conv_res, flt_rand2);

	EXPECT_EQ(ipx_set_float_be(&cast32->flt, BYTES_4, flt_rand3), IPX_OK);
	EXPECT_EQ(ipx_get_float_be(&cast32->flt, BYTES_4, &conv_res), IPX_OK);
	EXPECT_FLOAT_EQ(conv_res, flt_rand3);

	EXPECT_EQ(ipx_set_float_be(&cast32->flt, BYTES_4, flt_rand4), IPX_OK);
	EXPECT_EQ(ipx_get_float_be(&cast32->flt, BYTES_4, &conv_res), IPX_OK);
	EXPECT_FLOAT_EQ(conv_res, flt_rand4);

	// 8 bytes float
	const double dbl_rand1 = 8.2130045014424771e+254;
	const double dbl_rand2 = 3.9879810211388147e-101;
	const double dbl_rand3 = -9.987654321012345e+168;
	const double dbl_rand4 = -1.234567890123456e-99;

	EXPECT_EQ(ipx_set_float_be(&cast64->flt, BYTES_8, dbl_rand1), IPX_OK);
	EXPECT_EQ(ipx_get_float_be(&cast64->flt, BYTES_8, &conv_res), IPX_OK);
	EXPECT_DOUBLE_EQ(conv_res, dbl_rand1);

	EXPECT_EQ(ipx_set_float_be(&cast64->flt, BYTES_8, dbl_rand2), IPX_OK);
	EXPECT_EQ(ipx_get_float_be(&cast64->flt, BYTES_8, &conv_res), IPX_OK);
	EXPECT_DOUBLE_EQ(conv_res, dbl_rand2);

	EXPECT_EQ(ipx_set_float_be(&cast64->flt, BYTES_8, dbl_rand3), IPX_OK);
	EXPECT_EQ(ipx_get_float_be(&cast64->flt, BYTES_8, &conv_res), IPX_OK);
	EXPECT_DOUBLE_EQ(conv_res, dbl_rand3);

	EXPECT_EQ(ipx_set_float_be(&cast64->flt, BYTES_8, dbl_rand4), IPX_OK);
	EXPECT_EQ(ipx_get_float_be(&cast64->flt, BYTES_8, &conv_res), IPX_OK);
	EXPECT_DOUBLE_EQ(conv_res, dbl_rand4);
}

/*
 * Test unsupported size of data fields
 */
TEST_F(ConverterFloat, GetOutOfRange)
{
	void *ptr = NULL; // Invalid pointer
	const double c_value = -1.234567890123e+23;
	double value = c_value;

	uint8_t mem128[16];
	EXPECT_EQ(ipx_get_float_be(mem128, 0, &value), IPX_ERR_ARG);
	EXPECT_TRUE(value == c_value);
	EXPECT_EQ(ipx_get_float_be(mem128, 1, &value), IPX_ERR_ARG);
	EXPECT_TRUE(value == c_value);
	EXPECT_EQ(ipx_get_float_be(mem128, 2, &value), IPX_ERR_ARG);
	EXPECT_TRUE(value == c_value);
	EXPECT_EQ(ipx_get_float_be(mem128, 3, &value), IPX_ERR_ARG);
	EXPECT_TRUE(value == c_value);
	EXPECT_EQ(ipx_get_float_be(mem128, 5, &value), IPX_ERR_ARG);
	EXPECT_TRUE(value == c_value);
	EXPECT_EQ(ipx_get_float_be(mem128, 6, &value), IPX_ERR_ARG);
	EXPECT_TRUE(value == c_value);
	EXPECT_EQ(ipx_get_float_be(mem128, 7, &value), IPX_ERR_ARG);
	EXPECT_TRUE(value == c_value);
	EXPECT_EQ(ipx_get_float_be(mem128, 9, &value), IPX_ERR_ARG);
	EXPECT_TRUE(value == c_value);
	EXPECT_EQ(ipx_get_float_be(mem128, 16, &value), IPX_ERR_ARG);
	EXPECT_TRUE(value == c_value);

	EXPECT_EQ(ipx_get_float_be(ptr, 0, &value), IPX_ERR_ARG);
	EXPECT_TRUE(value == c_value);
	EXPECT_EQ(ipx_get_float_be(ptr, 1, &value), IPX_ERR_ARG);
	EXPECT_TRUE(value == c_value);
	EXPECT_EQ(ipx_get_float_be(ptr, 2, &value), IPX_ERR_ARG);
	EXPECT_TRUE(value == c_value);
	EXPECT_EQ(ipx_get_float_be(ptr, 3, &value), IPX_ERR_ARG);
	EXPECT_TRUE(value == c_value);
	EXPECT_EQ(ipx_get_float_be(ptr, 5, &value), IPX_ERR_ARG);
	EXPECT_TRUE(value == c_value);
	EXPECT_EQ(ipx_get_float_be(ptr, 6, &value), IPX_ERR_ARG);
	EXPECT_TRUE(value == c_value);
	EXPECT_EQ(ipx_get_float_be(ptr, 7, &value), IPX_ERR_ARG);
	EXPECT_TRUE(value == c_value);
	EXPECT_EQ(ipx_get_float_be(ptr, 9, &value), IPX_ERR_ARG);
	EXPECT_TRUE(value == c_value);
	EXPECT_EQ(ipx_get_float_be(ptr, 16, &value), IPX_ERR_ARG);
	EXPECT_TRUE(value == c_value);
}

TEST_F(ConverterFloat, SetAndGetInfinity)
{
	const double dbl_inf = std::numeric_limits<double>::infinity();
	const float flt_inf = std::numeric_limits<float>::infinity();
	double conv_res;

	// 4 bytes float
	EXPECT_EQ(ipx_set_float_be(&cast32->flt, BYTES_4, flt_inf), IPX_OK);
	EXPECT_EQ(ipx_get_float_be(&cast32->flt, BYTES_4, &conv_res), IPX_OK);
	EXPECT_FLOAT_EQ(conv_res, flt_inf);

	EXPECT_EQ(ipx_set_float_be(&cast32->flt, BYTES_4, -flt_inf), IPX_OK);
	EXPECT_EQ(ipx_get_float_be(&cast32->flt, BYTES_4, &conv_res), IPX_OK);
	EXPECT_FLOAT_EQ(conv_res, -flt_inf);

	// 8 bytes float
	EXPECT_EQ(ipx_set_float_be(&cast64->flt, BYTES_8, dbl_inf), IPX_OK);
	EXPECT_EQ(ipx_get_float_be(&cast64->flt, BYTES_8, &conv_res), IPX_OK);
	EXPECT_DOUBLE_EQ(conv_res, dbl_inf);

	EXPECT_EQ(ipx_set_float_be(&cast64->flt, BYTES_8, -dbl_inf), IPX_OK);
	EXPECT_EQ(ipx_get_float_be(&cast64->flt, BYTES_8, &conv_res), IPX_OK);
	EXPECT_DOUBLE_EQ(conv_res, -dbl_inf);
}

TEST_F(ConverterFloat, SetAndGetNan)
{
	const double dbl_nan = std::numeric_limits<double>::quiet_NaN();
	const float flt_nan = std::numeric_limits<float>::quiet_NaN();
	double conv_res;

	// 4 bytes float
	EXPECT_EQ(ipx_set_float_be(&cast32->flt, BYTES_4, flt_nan), IPX_OK);
	EXPECT_EQ(ipx_get_float_be(&cast32->flt, BYTES_4, &conv_res), IPX_OK);
	EXPECT_TRUE(isnan(conv_res));

	EXPECT_EQ(ipx_set_float_be(&cast32->flt, BYTES_4, -flt_nan), IPX_OK);
	EXPECT_EQ(ipx_get_float_be(&cast32->flt, BYTES_4, &conv_res), IPX_OK);
	EXPECT_TRUE(isnan(conv_res));

	// 8 bytes float
	EXPECT_EQ(ipx_set_float_be(&cast64->flt, BYTES_8, dbl_nan), IPX_OK);
	EXPECT_EQ(ipx_get_float_be(&cast64->flt, BYTES_8, &conv_res), IPX_OK);
	EXPECT_TRUE(isnan(conv_res));

	EXPECT_EQ(ipx_set_float_be(&cast64->flt, BYTES_8, -dbl_nan), IPX_OK);
	EXPECT_EQ(ipx_get_float_be(&cast64->flt, BYTES_8, &conv_res), IPX_OK);
	EXPECT_TRUE(isnan(conv_res));
}

/**
 * @}
 */

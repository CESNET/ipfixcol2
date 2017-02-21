/**
 * \file tests/unit_tests/src/convertors.cpp
 * \author Lukas Hutak <xhutak01@stud.fit.vutbr.cz>
 * \brief Convertor tests
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
 * \defgroup ipx_convertors_test Data conversion tests
 *
 * \note In many cases, test functions use dynamically allocated variables,
 *   because it is usefull for valgrind memory check (accessing an array out
 *   of bounds, etc.)
 * @{
 */

#include <gtest/gtest.h>
#include <cstring>
#include <endian.h>

extern "C" {
	#include <ipfixcol2/convertors.h>
}

// Auxiliary definitions of maximal values for UINT_XX for 3, 5, 6 and 7 bytes
const uint32_t IPX_UINT24_MAX = 0xFFFFFFUL;
const uint64_t IPX_UINT40_MAX = 0xFFFFFFFFFFULL;
const uint64_t IPX_UINT48_MAX = 0xFFFFFFFFFFFFULL;
const uint64_t IPX_UINT56_MAX = 0xFFFFFFFFFFFFFFULL;

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

/**
 * \brief Test fixture for SetUint tests
 */
class SetUint : public ::testing::Test {
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
TEST_F(SetUint, maxMin) {
	// SetUp
	const uint64_t max_val = UINT64_MAX;
	const uint64_t min_val = 0U;

	// Execute
	// 1 byte
	EXPECT_EQ(ipx_set_uint(u8, 1,  max_val), IPX_CONVERT_ERR_TRUNC);
	EXPECT_EQ(*u8,  UINT8_MAX);
	EXPECT_EQ(ipx_set_uint(u8, 1,  min_val), 0);
	EXPECT_EQ(*u8,  0U);

	// 2 bytes
	EXPECT_EQ(ipx_set_uint(u16, 2, max_val), IPX_CONVERT_ERR_TRUNC);
	EXPECT_EQ(*u16, UINT16_MAX);
	EXPECT_EQ(ipx_set_uint(u16, 2, min_val), 0);
	EXPECT_EQ(*u16, 0U);

	// 4 bytes
	EXPECT_EQ(ipx_set_uint(u32, 4, max_val), IPX_CONVERT_ERR_TRUNC);
	EXPECT_EQ(*u32, UINT32_MAX);
	EXPECT_EQ(ipx_set_uint(u32, 4, min_val), 0);
	EXPECT_EQ(*u32, 0U);

	// 8 bytes
	EXPECT_EQ(ipx_set_uint(u64, 8, max_val), 0);
	EXPECT_EQ(*u64, UINT64_MAX);
	EXPECT_EQ(ipx_set_uint(u64, 8, min_val), 0);
	EXPECT_EQ(*u64, 0U);

	// Other (unusual situations i.e. 3, 5, 6 and 7 bytes)
	// 3 bytes
	EXPECT_EQ(ipx_set_uint(u24, 3, max_val), IPX_CONVERT_ERR_TRUNC);
	EXPECT_EQ(memcmp(u24, &max_val, 3), 0);
	EXPECT_EQ(ipx_set_uint(u24, 3, min_val), 0);
	EXPECT_EQ(memcmp(u24, &min_val, 3), 0);

	// 5 bytes
	EXPECT_EQ(ipx_set_uint(u40, 5, max_val), IPX_CONVERT_ERR_TRUNC);
	EXPECT_EQ(memcmp(u40, &max_val, 5), 0);
	EXPECT_EQ(ipx_set_uint(u40, 5, min_val), 0);
	EXPECT_EQ(memcmp(u40, &min_val, 5), 0);

	// 6 bytes
	EXPECT_EQ(ipx_set_uint(u48, 6, max_val), IPX_CONVERT_ERR_TRUNC);
	EXPECT_EQ(memcmp(u48, &max_val, 6), 0);
	EXPECT_EQ(ipx_set_uint(u48, 6, min_val), 0);
	EXPECT_EQ(memcmp(u48, &min_val, 6), 0);

	// 7 bytes
	EXPECT_EQ(ipx_set_uint(u56, 7, max_val), IPX_CONVERT_ERR_TRUNC);
	EXPECT_EQ(memcmp(u56, &max_val, 7), 0);
	EXPECT_EQ(ipx_set_uint(u56, 7, min_val), 0);
	EXPECT_EQ(memcmp(u56, &min_val, 7), 0);
}

/*
 * Insert max + 1/max/max - 1 values into 1 - 8 byte variables.
 */
TEST_F(SetUint, aboveBelow)
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
	EXPECT_EQ(ipx_set_uint(u8, 1, u8_above), IPX_CONVERT_ERR_TRUNC);
	EXPECT_EQ(*u8, UINT8_MAX);  // No endian conversion needed
	EXPECT_EQ(ipx_set_uint(u8, 1, UINT8_MAX), 0);
	EXPECT_EQ(*u8, UINT8_MAX);  // No endian conversion needed
	EXPECT_EQ(ipx_set_uint(u8, 1, u8_below), 0);
	EXPECT_EQ(*u8, u8_below);   // No endian conversion needed (only 1 byte)

	// 2 bytes
	EXPECT_EQ(ipx_set_uint(u16, 2, u16_above), IPX_CONVERT_ERR_TRUNC);
	EXPECT_EQ(*u16, UINT16_MAX); // No endian conversion needed
	EXPECT_EQ(ipx_set_uint(u16, 2, UINT16_MAX), 0);
	EXPECT_EQ(*u16, UINT16_MAX);  // No endian conversion needed
	EXPECT_EQ(ipx_set_uint(u16, 2, u16_below), 0);
	EXPECT_EQ(*u16, htons(u16_below));

	// 4 bytes
	EXPECT_EQ(ipx_set_uint(u32, 4, u32_above), IPX_CONVERT_ERR_TRUNC);
	EXPECT_EQ(*u32, UINT32_MAX); // No endian conversion needed
	EXPECT_EQ(ipx_set_uint(u32, 4, UINT32_MAX), 0);
	EXPECT_EQ(*u32, UINT32_MAX);  // No endian conversion needed
	EXPECT_EQ(ipx_set_uint(u32, 4, u32_below), 0);
	EXPECT_EQ(*u32, htonl(u32_below));

	// 8 bytes (only the value below MAX and MAX)
	EXPECT_EQ(ipx_set_uint(u64, 8, UINT64_MAX), 0);
	EXPECT_EQ(*u64, UINT64_MAX);  // No endian conversion needed
	EXPECT_EQ(ipx_set_uint(u64, 8, u64_below), 0);
	EXPECT_EQ(*u64, htobe64(u64_below));

	// Other (unusual situations i.e. 3, 5, 6 and 7 bytes)
	const uint64_t temp_max = UINT64_MAX;
	uint8_t temp32[4];
	uint8_t temp64[8];

	// 3 bytes
	EXPECT_EQ(ipx_set_uint(u24, 3, u24_above), IPX_CONVERT_ERR_TRUNC);
	EXPECT_EQ(memcmp(u24, &temp_max, 3), 0);
	EXPECT_EQ(ipx_set_uint(u24, 3, IPX_UINT24_MAX), 0);
	EXPECT_EQ(memcmp(u24, &temp_max, 3), 0);
	EXPECT_EQ(ipx_set_uint(u24, 3, u24_below), 0);
	*((uint32_t *) temp32) = htonl(u24_below);
	EXPECT_EQ(memcmp(u24, &temp32[1], 3), 0);

	// 5 bytes
	EXPECT_EQ(ipx_set_uint(u40, 5, u40_above), IPX_CONVERT_ERR_TRUNC);
	EXPECT_EQ(memcmp(u40, &temp_max, 5), 0);
	EXPECT_EQ(ipx_set_uint(u40, 5, IPX_UINT40_MAX), 0);
	EXPECT_EQ(memcmp(u40, &temp_max, 5), 0);
	EXPECT_EQ(ipx_set_uint(u40, 5, u40_below), 0);
	*((uint64_t *) temp64) = htobe64(u40_below);
	EXPECT_EQ(memcmp(u40, &temp64[3], 5), 0);

	// 6 bytes
	EXPECT_EQ(ipx_set_uint(u48, 6, u48_above), IPX_CONVERT_ERR_TRUNC);
	EXPECT_EQ(memcmp(u48, &temp_max, 6), 0);
	EXPECT_EQ(ipx_set_uint(u48, 6, IPX_UINT48_MAX), 0);
	EXPECT_EQ(memcmp(u48, &temp_max, 6), 0);
	EXPECT_EQ(ipx_set_uint(u48, 6, u48_below), 0);
	*((uint64_t *) temp64) = htobe64(u48_below);
	EXPECT_EQ(memcmp(u48, &temp64[2], 6), 0);

	// 7 bytes
	EXPECT_EQ(ipx_set_uint(u56, 7, u56_above), IPX_CONVERT_ERR_TRUNC);
	EXPECT_EQ(memcmp(u56, &temp_max, 7), 0);
	EXPECT_EQ(ipx_set_uint(u56, 7, IPX_UINT56_MAX), 0);
	EXPECT_EQ(memcmp(u56, &temp_max, 7), 0);
	EXPECT_EQ(ipx_set_uint(u56, 7, u56_below), 0);
	*((uint64_t *) temp64) = htobe64(u56_below);
	EXPECT_EQ(memcmp(u56, &temp64[1], 7), 0);
}

/*
 * "Random" values in the valid interval for 1 - 8 bytes unsigned values
 */
TEST_F(SetUint, inInterval)
{
	// 1 byte
	const uint8_t u8_rand1 =  12U;
	const uint8_t u8_rand2 =  93U;
	const uint8_t u8_rand3 = 112U;
	EXPECT_EQ(ipx_set_uint(u8, 1, u8_rand1), 0);
	EXPECT_EQ(*u8, u8_rand1);
	EXPECT_EQ(ipx_set_uint(u8, 1, u8_rand2), 0);
	EXPECT_EQ(*u8, u8_rand2);
	EXPECT_EQ(ipx_set_uint(u8, 1, u8_rand3), 0);
	EXPECT_EQ(*u8, u8_rand3);

	// 2 bytes
	const uint16_t u16_rand1 =  1342U;
	const uint16_t u16_rand2 = 25432U;
	const uint16_t u16_rand3 = 45391U;
	EXPECT_EQ(ipx_set_uint(u16, 2, u16_rand1), 0);
	EXPECT_EQ(*u16, htons(u16_rand1));
	EXPECT_EQ(ipx_set_uint(u16, 2, u16_rand2), 0);
	EXPECT_EQ(*u16, htons(u16_rand2));
	EXPECT_EQ(ipx_set_uint(u16, 2, u16_rand3), 0);
	EXPECT_EQ(*u16, htons(u16_rand3));

	// 4 bytes
	const uint32_t u32_rand1 =      50832UL;
	const uint32_t u32_rand2 =   11370824UL;
	const uint32_t u32_rand3 = 3793805425UL;
	EXPECT_EQ(ipx_set_uint(u32, 4, u32_rand1), 0);
	EXPECT_EQ(*u32, htonl(u32_rand1));
	EXPECT_EQ(ipx_set_uint(u32, 4, u32_rand2), 0);
	EXPECT_EQ(*u32, htonl(u32_rand2));
	EXPECT_EQ(ipx_set_uint(u32, 4, u32_rand3), 0);
	EXPECT_EQ(*u32, htonl(u32_rand3));

	// 8 bytes
	const uint64_t u64_rand1 =         428760872517ULL;
	const uint64_t u64_rand2 =     8275792237734210ULL;
	const uint64_t u64_rand3 = 17326724161708531625ULL;
	EXPECT_EQ(ipx_set_uint(u64, 8, u64_rand1), 0);
	EXPECT_EQ(*u64, htobe64(u64_rand1));
	EXPECT_EQ(ipx_set_uint(u64, 8, u64_rand2), 0);
	EXPECT_EQ(*u64, htobe64(u64_rand2));
	EXPECT_EQ(ipx_set_uint(u64, 8, u64_rand3), 0);
	EXPECT_EQ(*u64, htobe64(u64_rand3));

	// Other (unusual situations i.e. 3, 5, 6 and 7 bytes)
	// 3 bytes
	uint8_t temp32[4];
	uint8_t temp64[8];

	const uint32_t u24_rand1 =    22311UL;
	const uint32_t u24_rand2 =   861354UL;
	const uint32_t u24_rand3 = 14075499UL;
	EXPECT_EQ(ipx_set_uint(u24, 3, u24_rand1), 0);  // Rand 1
	*((uint32_t *) temp32) = htonl(u24_rand1);
	EXPECT_EQ(memcmp(u24, &temp32[1], 3), 0);
	EXPECT_EQ(ipx_set_uint(u24, 3, u24_rand2), 0);  // Rand 2
	*((uint32_t *) temp32) = htonl(u24_rand2);
	EXPECT_EQ(memcmp(u24, &temp32[1], 3), 0);
	EXPECT_EQ(ipx_set_uint(u24, 3, u24_rand3), 0);  // Rand 3
	*((uint32_t *) temp32) = htonl(u24_rand3);
	EXPECT_EQ(memcmp(u24, &temp32[1], 3), 0);

	// 5 bytes
	const uint64_t u40_rand1 =       360214ULL;
	const uint64_t u40_rand2 =    240285687ULL;
	const uint64_t u40_rand3 = 796219095503ULL;
	EXPECT_EQ(ipx_set_uint(u40, 5, u40_rand1), 0); // Rand 1
	*((uint64_t *) temp64) = htobe64(u40_rand1);
	EXPECT_EQ(memcmp(u40, &temp64[3], 5), 0);
	EXPECT_EQ(ipx_set_uint(u40, 5, u40_rand2), 0); // Rand 2
	*((uint64_t *) temp64) = htobe64(u40_rand2);
	EXPECT_EQ(memcmp(u40, &temp64[3], 5), 0);
	EXPECT_EQ(ipx_set_uint(u40, 5, u40_rand3), 0); // Rand 3
	*((uint64_t *) temp64) = htobe64(u40_rand3);
	EXPECT_EQ(memcmp(u40, &temp64[3], 5), 0);

	// 6 bytes
	const uint64_t u48_rand1 =       696468180ULL;
	const uint64_t u48_rand2 =    671963163167ULL;
	const uint64_t u48_rand3 = 209841476899288ULL;
	EXPECT_EQ(ipx_set_uint(u48, 6, u48_rand1), 0); // Rand 1
	*((uint64_t *) temp64) = htobe64(u48_rand1);
	EXPECT_EQ(memcmp(u48, &temp64[2], 6), 0);
	EXPECT_EQ(ipx_set_uint(u48, 6, u48_rand2), 0); // Rand 2
	*((uint64_t *) temp64) = htobe64(u48_rand2);
	EXPECT_EQ(memcmp(u48, &temp64[2], 6), 0);
	EXPECT_EQ(ipx_set_uint(u48, 6, u48_rand3), 0); // Rand 3
	*((uint64_t *) temp64) = htobe64(u48_rand3);
	EXPECT_EQ(memcmp(u48, &temp64[2], 6), 0);

	// 7 bytes
	const uint64_t u56_rand1 =      194728764120ULL;
	const uint64_t u56_rand2 =   128273048983421ULL;
	const uint64_t u56_rand3 = 66086893994497342ULL;
	EXPECT_EQ(ipx_set_uint(u56, 7, u56_rand1), 0); // Rand 1
	*((uint64_t *) temp64) = htobe64(u56_rand1);
	EXPECT_EQ(memcmp(u56, &temp64[1], 7), 0);
	EXPECT_EQ(ipx_set_uint(u56, 7, u56_rand2), 0); // Rand 2
	*((uint64_t *) temp64) = htobe64(u56_rand2);
	EXPECT_EQ(memcmp(u56, &temp64[1], 7), 0);
	EXPECT_EQ(ipx_set_uint(u56, 7, u56_rand3), 0); // Rand 3
	*((uint64_t *) temp64) = htobe64(u56_rand3);
	EXPECT_EQ(memcmp(u56, &temp64[1], 7), 0);
}


/*
 * Test unsupported size of data fields
 */
TEST_F(SetUint, setUintOutOfRange)
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

	EXPECT_EQ(ipx_set_uint(temp72, 0, value), IPX_CONVERT_ERR_ARG);
	EXPECT_EQ(ipx_set_uint(temp72, temp72_size, value), IPX_CONVERT_ERR_ARG);
	EXPECT_EQ(ipx_set_uint(temp88, temp88_size, value), IPX_CONVERT_ERR_ARG);
	EXPECT_EQ(ipx_set_uint(temp128, temp128_size, value), IPX_CONVERT_ERR_ARG);
	EXPECT_EQ(ipx_set_uint(temp192, temp192_size, value), IPX_CONVERT_ERR_ARG);
	EXPECT_EQ(ipx_set_uint(temp256, temp256_size, value), IPX_CONVERT_ERR_ARG);
}




/**
 * @}
 */

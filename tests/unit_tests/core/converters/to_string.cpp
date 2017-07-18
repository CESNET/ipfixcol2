/**
 * \file tests/unit_tests/core/converters/to_string.cpp
 * \author Lukas Hutak <xhutak01@stud.fit.vutbr.cz>
 * \brief Converters tests (to string)
 */
/* Copyright (C) 2017 CESNET, z.s.p.o.
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
 * \defgroup ipx_converters_tostring Data conversion tests of "to string" functions
 *
 * @{
 */

#include <gtest/gtest.h>
#include <endian.h>
#include <string>
#include <memory>

#define BYTES_1 (1U)
#define BYTES_2 (2U)
#define BYTES_3 (3U)
#define BYTES_4 (4U)
#define BYTES_5 (5U)
#define BYTES_6 (6U)
#define BYTES_7 (7U)
#define BYTES_8 (8U)

extern "C" {
#include <ipfixcol2/converters.h>
}

int main(int argc, char **argv)
{
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}

/*
 * Test the unsigned integer to string converter
 */
void
uint2strNormal_check(size_t data_size, uint64_t value)
{
	SCOPED_TRACE("Data size: " + std::to_string(data_size));
	// Calculate expected result
	std::string res_str = std::to_string(value);
	size_t res_size = res_str.length() + 1; // 1 == '\0'

	std::unique_ptr<uint8_t[]> data_ptr{new uint8_t[data_size]};
	std::unique_ptr<char[]> res_ptr{new char[res_size]};

	EXPECT_EQ(ipx_set_uint_be(data_ptr.get(), data_size, value), IPX_CONVERT_OK);
	EXPECT_EQ(ipx_uint2str_be(data_ptr.get(), data_size, res_ptr.get(), res_size),
		static_cast<int>(res_size - 1));
	EXPECT_EQ(res_str, res_ptr.get());
}

TEST(ConveterToStrings, uint2strNormal)
{
	for (size_t i = 1; i <= 8U; ++i) {
		uint64_t value = (i - 1) << 8 * (i - 1); // Just "random" numbers
		uint2strNormal_check(i, value);
	}
}

/*
 * Test insufficient size of an output string buffer
 */
void
uint2strSmallBuffer_check(size_t data_size, uint64_t value)
{
	SCOPED_TRACE("Data size: " + std::to_string(data_size));
	size_t res_size = std::to_string(value).length() + 1; // 1 == '\0'
	res_size -= 1; // Make sure that the length of the output buffer is insufficient

	std::unique_ptr<uint8_t[]> data_ptr{new uint8_t[data_size]};
	std::unique_ptr<char[]> str_ptr{new char[res_size]};

	EXPECT_EQ(ipx_set_uint_be(data_ptr.get(), data_size, value), IPX_CONVERT_OK);
	EXPECT_EQ(ipx_uint2str_be(data_ptr.get(), data_size, str_ptr.get(), res_size),
		IPX_CONVERT_ERR_BUFFER);
}

TEST(ConveterToStrings, uint2strSmallBuffer)
{
	for (size_t i = 1; i <= 8U; ++i) {
		uint64_t value = i << 8 * (i - 1); // Just "random" numbers
		uint2strSmallBuffer_check(i, value);
	}
}

/*
 * Test the signed integer to string converter
 */
void
int2strNormal_check(size_t data_size, int64_t value)
{
	SCOPED_TRACE("Data size: " + std::to_string(data_size));
	// Calculate expected result
	std::string res_str = std::to_string(value);
	size_t res_size = res_str.length() + 1; // 1 == '\0'

	std::unique_ptr<uint8_t[]> data_ptr{new uint8_t[data_size]};
	std::unique_ptr<char[]> res_ptr{new char[res_size]};

	EXPECT_EQ(ipx_set_int_be(data_ptr.get(), data_size, value), IPX_CONVERT_OK);
	EXPECT_EQ(ipx_int2str_be(data_ptr.get(), data_size, res_ptr.get(), res_size),
		static_cast<int>(res_size - 1));
	EXPECT_EQ(res_str, res_ptr.get());
}

TEST(ConveterToStrings, int2strNormal)
{
	for (size_t i = 1; i <= 8U; ++i) {
		int64_t value = (i - 1) << 8 * (i - 1); // Just "random" numbers
		value *= (i / 2) ? 1 : (-1);
		int2strNormal_check(i, value);
	}
}

/*
 * Test insufficient size of an output string buffer
 */
void
int2strSmallBuffer_check(size_t data_size, int64_t value)
{
	SCOPED_TRACE("Data size: " + std::to_string(data_size));
	size_t res_size = std::to_string(value).length() + 1; // 1 == '\0'
	res_size -= 1; // Make sure that the length of the output buffer is insufficient

	std::unique_ptr<uint8_t[]> data_ptr{new uint8_t[data_size]};
	std::unique_ptr<char[]> str_ptr{new char[res_size]};

	EXPECT_EQ(ipx_set_int_be(data_ptr.get(), data_size, value), IPX_CONVERT_OK);
	EXPECT_EQ(ipx_int2str_be(data_ptr.get(), data_size, str_ptr.get(), res_size),
		IPX_CONVERT_ERR_BUFFER);
}

TEST(ConveterToStrings, int2strSmallBuffer)
{
	for (size_t i = 1; i <= 8U; ++i) {
		int64_t value = i << 8 * (i - 1); // Just "random" numbers
		value *= (i / 2) ? 1 : (-1);
		int2strSmallBuffer_check(i, value);
	}
}

/**
 * @}
 */

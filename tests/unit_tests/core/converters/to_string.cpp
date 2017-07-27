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

#include <iomanip> // setprecision
#include <sstream> // stringstream

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

TEST(ConverterToStrings, uint2strNormal)
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

TEST(ConverterToStrings, uint2strSmallBuffer)
{
	for (size_t i = 1; i <= 8U; ++i) {
		uint64_t value = i << 8 * (i - 1); // Just "random" numbers
		uint2strSmallBuffer_check(i, value);
	}
}

TEST(ConverterToStrings, uint2strFormatErr)
{
	const size_t size = 16;
	std::unique_ptr<uint8_t[]> data_ptr{new uint8_t[size]};
	std::unique_ptr<char[]> str_ptr{new char[size]};

	// Test invalid size of the field
	EXPECT_EQ(ipx_uint2str_be(data_ptr.get(), 0, str_ptr.get(), size),
		IPX_CONVERT_ERR_ARG);
	EXPECT_EQ(ipx_uint2str_be(data_ptr.get(), 9, str_ptr.get(), size),
			IPX_CONVERT_ERR_ARG);

	// Test that memory is not accessed if invalid size is defined.
	EXPECT_EQ(ipx_uint2str_be(NULL, 0, str_ptr.get(), size),
			IPX_CONVERT_ERR_ARG);
	EXPECT_EQ(ipx_uint2str_be(NULL, 9, str_ptr.get(), size),
			IPX_CONVERT_ERR_ARG);
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

TEST(ConverterToStrings, int2strNormal)
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

TEST(ConverterToStrings, int2strSmallBuffer)
{
	for (size_t i = 1; i <= 8U; ++i) {
		int64_t value = i << 8 * (i - 1); // Just "random" numbers
		value *= (i / 2) ? 1 : (-1);
		int2strSmallBuffer_check(i, value);
	}
}

TEST(ConverterToStrings, int2strFormatErr)
{
	const size_t size = 16;
	std::unique_ptr<uint8_t[]> data_ptr{new uint8_t[size]};
	std::unique_ptr<char[]> str_ptr{new char[size]};

	// Test invalid size of the field
	EXPECT_EQ(ipx_int2str_be(data_ptr.get(), 0, str_ptr.get(), size),
			IPX_CONVERT_ERR_ARG);
	EXPECT_EQ(ipx_int2str_be(data_ptr.get(), 9, str_ptr.get(), size),
			IPX_CONVERT_ERR_ARG);

	// Test that memory is not accessed if invalid size is defined.
	EXPECT_EQ(ipx_int2str_be(NULL, 0, str_ptr.get(), size),
			IPX_CONVERT_ERR_ARG);
	EXPECT_EQ(ipx_int2str_be(NULL, 9, str_ptr.get(), size),
			IPX_CONVERT_ERR_ARG);
}

/*
 * Test the 32bit float to string converter
 */
void
float2strNormal_32check(float value)
{
	SCOPED_TRACE("Test value: " + std::to_string(value));
	const size_t data_size = sizeof(float);
	const size_t res_size = 16;

	// Prepare auxiliary arrays
	std::unique_ptr<uint8_t[]> data_ptr{new uint8_t[data_size]};
	std::unique_ptr<char[]> res_ptr{new char[res_size]};

	// Store and convert data using converters
	EXPECT_EQ(ipx_set_float_be(data_ptr.get(), data_size, value), IPX_CONVERT_OK);
	EXPECT_GT(ipx_float2str_be(data_ptr.get(), data_size, res_ptr.get(), res_size), 0);

	// Try to convert the string back to the float number
	std::string res_str {res_ptr.get()};
	size_t idx = 0;
	float new_result = std::stof(res_str, &idx);
	EXPECT_EQ(idx, res_str.size());

	if (isnan(value)) {
		/*
		 * Special case: expected result is not a number
		 * Because (NaN == NaN) is always false, we have to check it separately
		 */
		EXPECT_TRUE(isnan(new_result));
	} else if (isinf(value)) {
		/*
		 * Special case: expected result is infinity
		 */
		EXPECT_EQ(isinf(value), isinf(new_result));
	} else {
		/*
		 * When the converter converts float to string only 6 valid digits are
		 * printed. This also causes truncation and, therefore, the  epsilon is
		 * only 10e-6.
		 */
		const float eps = 1.1e-6;
		EXPECT_NEAR(value, new_result, fabsf(eps * value));
	}
}

TEST(ConverterToStrings, float2strNormal32)
{
	// The positive/negative maximum numbers
	const float flt_max_plus = std::numeric_limits<float>::max();
	const float flt_max_minus = std::numeric_limits<float>::lowest();
	float2strNormal_32check(flt_max_plus);
	float2strNormal_32check(flt_max_minus);

	// Infinity and NaN
	const float flt_inf = std::numeric_limits<float>::infinity();
	const float flt_nan = std::numeric_limits<float>::quiet_NaN();
	float2strNormal_32check(+flt_inf);
	float2strNormal_32check(-flt_inf);
	float2strNormal_32check(+flt_nan);
	float2strNormal_32check(-flt_nan);

	// Random values
	float2strNormal_32check(0.0f);
	float2strNormal_32check(123.56e-21f);
	float2strNormal_32check(-4.12348e32f);
	float2strNormal_32check(2.46017e+25f);
	float2strNormal_32check(8.56481e-33f);
}

/*
 * Test the 64bit float to string converter
 */
void
float2strNormal_64check(double value)
{
	SCOPED_TRACE("Test value: " + std::to_string(value));
	const size_t data_size = sizeof(double);
	const size_t res_size = 32;

	// Prepare auxiliary arrays
	std::unique_ptr<uint8_t[]> data_ptr{new uint8_t[data_size]};
	std::unique_ptr<char[]> res_ptr{new char[res_size]};

	// Store and convert data using converters
	EXPECT_EQ(ipx_set_float_be(data_ptr.get(), data_size, value), IPX_CONVERT_OK);
	EXPECT_GT(ipx_float2str_be(data_ptr.get(), data_size, res_ptr.get(), res_size), 0);

	// Try to convert the string back to the float number
	std::string res_str {res_ptr.get()};
	size_t idx = 0;
	double new_result = std::stod(res_str, &idx);
	EXPECT_EQ(idx, res_str.size());

	if (isnan(value)) {
		/*
		 * Special case: expected result is not a number
		 * Because (NaN == NaN) is always false, we have to check it separately
		 */
		EXPECT_TRUE(isnan(new_result));
	} else if (isinf(value)) {
		/*
		 * Special case: expected result is infinity
		 */
		EXPECT_EQ(isinf(value), isinf(new_result));
	} else {
		/*
		 * When the converter converts float to string only 15 valid digits are
		 * printed. This also causes truncation and, therefore, the  epsilon is
		 * only 10e-15.
		 */
		const double eps = 1.1e-15;
		EXPECT_NEAR(value, new_result, fabs(eps * value));
	}
}

TEST(ConverterToStrings, float2strNormal64)
{
	// The positive/negative maximum numbers
	double dbl_max_plus = std::numeric_limits<double>::max();
	double dbl_max_minus = std::numeric_limits<double>::lowest();

	/*
	 * This modification prevents string rounding of the value the way it exceed
	 * the maximum/minimum possible value and cause an exception of std::stod
	 */
	const double dbl_eps = std::numeric_limits<double>::epsilon();
	dbl_max_plus -= dbl_max_plus * (10 * dbl_eps);
	dbl_max_minus += dbl_max_minus * (10 * dbl_eps);

	float2strNormal_64check(dbl_max_plus);
	float2strNormal_64check(dbl_max_minus);

	// Infinity and NaN
	const double dbl_inf = std::numeric_limits<double>::infinity();
	const double dbl_nan = std::numeric_limits<double>::quiet_NaN();
	float2strNormal_64check(+dbl_inf);
	float2strNormal_64check(-dbl_inf);
	float2strNormal_64check(+dbl_nan);
	float2strNormal_64check(-dbl_nan);

	// Random values
	float2strNormal_64check(0.0);
	float2strNormal_64check(8.21300450144247e+254);
	float2strNormal_64check(-4.12348565421410e+32);
	float2strNormal_64check(2.46099841105657e-25);
	float2strNormal_64check(3.98798102113881e-101);
}

/*
 * Test invalid size of input field i.e. anything expect 4 a 8 bytes.
 */
TEST(ConverterToStrings, float2strSmallBuffer)
{
	const size_t res_size = 32;

	for (size_t i = 0; i < 10; ++i) {
		if (i == BYTES_4 || i == BYTES_8) {
			// Skip valid sizes
			continue;
		}

		std::unique_ptr<uint8_t[]> data_ptr{new uint8_t[i]};
		std::unique_ptr<char[]> res_ptr{new char[res_size]};

		EXPECT_EQ(ipx_float2str_be(data_ptr.get(), i, res_ptr.get(), res_size),
			IPX_CONVERT_ERR_ARG);
	}
}

/*
 * Test insufficient size of an output string buffer
 */
void
float2strSmallBuffer_32check(float value)
{
	SCOPED_TRACE("Test value: " + std::to_string(value));
	const size_t data_size = sizeof(float);
	std::unique_ptr<uint8_t[]> data_ptr{new uint8_t[data_size]};

	// Convert float to string
	const int valid_nums = std::numeric_limits<float>::digits10;
	std::stringstream ss;
	ss << std::setprecision(valid_nums) << value;
	std::string res_str = ss.str();

	// Calculate expected length and make the output buffer insufficient
	size_t out_len = res_str.length(); // Not enough space for '\0'
	std::unique_ptr<char[]> out_ptr{new char[out_len]};

	EXPECT_EQ(ipx_set_float_be(data_ptr.get(), data_size, value), IPX_CONVERT_OK);
	EXPECT_EQ(ipx_float2str_be(data_ptr.get(), data_size, out_ptr.get(), out_len),
		IPX_CONVERT_ERR_BUFFER);
}

void
float2strSmallBuffer_64check(double value)
{
	SCOPED_TRACE("Test value: " + std::to_string(value));
	const size_t data_size = sizeof(double);
	std::unique_ptr<uint8_t[]> data_ptr{new uint8_t[data_size]};

	// Convert float to string
	const int valid_nums = std::numeric_limits<double>::digits10;
	std::stringstream ss;
	ss << std::setprecision(valid_nums) << value;
	std::string res_str = ss.str();

	// Calculate expected length and make the output buffer insufficient
	size_t out_len = res_str.length(); // Not enough space for '\0'
	std::unique_ptr<char[]> out_ptr{new char[out_len]};

	EXPECT_EQ(ipx_set_float_be(data_ptr.get(), data_size, value), IPX_CONVERT_OK);
	EXPECT_EQ(ipx_float2str_be(data_ptr.get(), data_size, out_ptr.get(), out_len),
			IPX_CONVERT_ERR_BUFFER);
}

TEST(ConverterToStrings, float2strInvalidInput)
{
	float2strSmallBuffer_32check(1.12470e10);
	float2strSmallBuffer_32check(8.26578e-23);
	float2strSmallBuffer_32check(-5.16578e10);
	float2strSmallBuffer_32check(-1.65117e-10);

	float2strSmallBuffer_64check(8.21300450144247e+254);
	float2strSmallBuffer_64check(5.02465721798100e-23);
	float2strSmallBuffer_64check(-1.54643210045789e50);
	float2strSmallBuffer_64check(-8.2234687921134e-123);
}

/**
 * @}
 */

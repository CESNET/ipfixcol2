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

// -----------------------------------------------------------------------------

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

// -----------------------------------------------------------------------------

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

// -----------------------------------------------------------------------------

/*
 * Test the bool to string converter
 */
TEST(ConverterToStrings, bool2strNormal)
{
	const size_t data_size = BYTES_1;
	std::unique_ptr<uint8_t[]> data_ptr{new uint8_t[data_size]};

	// Test "True"
	std::string true_str{"true"};
	size_t res_size = true_str.length() + 1; // 1 == '\0'
	std::unique_ptr<char[]> res_ptr{new char[res_size]};

	EXPECT_EQ(ipx_set_bool(data_ptr.get(), data_size, true), IPX_CONVERT_OK);
	EXPECT_GT(ipx_bool2str(data_ptr.get(), res_ptr.get(), res_size), 0);
	EXPECT_EQ(true_str, res_ptr.get());

	// Test "False"
	std::string false_str{"false"};
	res_size = false_str.length() + 1; // 1 == '\0'
	res_ptr.reset(new char[res_size]);

	EXPECT_EQ(ipx_set_bool(data_ptr.get(), data_size, false), IPX_CONVERT_OK);
	EXPECT_GT(ipx_bool2str(data_ptr.get(), res_ptr.get(), res_size), 0);
	EXPECT_EQ(false_str, res_ptr.get());
}

/*
 *  Test invalid boolean values stored in a data field
 */
TEST(ConverterToStrings, bool2strInvalidInput)
{
	uint8_t data;
	const size_t res_size = 32;
	std::unique_ptr<char[]> res_ptr{new char[res_size]};

	for (uint16_t i = 0; i < 256; ++i) {
		if (i == 1 || i == 2) { // true == 1 and false == 2
			continue;
		}

		data = i;
		EXPECT_EQ(ipx_bool2str(&data, res_ptr.get(), res_size), IPX_CONVERT_ERR_ARG);
	}
}

/*
 * Test insufficient size of an output string buffer
 */
TEST(ConverterToStrings, bool2strSmallBuffer)
{
	const size_t data_size = BYTES_1;
	std::unique_ptr<uint8_t[]> data_ptr{new uint8_t[data_size]};

	// Test true
	std::string true_str{"true"};
	size_t res_size = true_str.length(); // Not enough space for '\0'
	std::unique_ptr<char[]> res_ptr{new char[res_size]};

	EXPECT_EQ(ipx_set_bool(data_ptr.get(), data_size, true), IPX_CONVERT_OK);
	EXPECT_EQ(ipx_bool2str(data_ptr.get(), res_ptr.get(), res_size), IPX_CONVERT_ERR_BUFFER);

	// Test "False"
	std::string false_str{"false"};
	res_size = false_str.length(); // Not enough space for '\0'
	res_ptr.reset(new char[res_size]);

	EXPECT_EQ(ipx_set_bool(data_ptr.get(), data_size, false), IPX_CONVERT_OK);
	EXPECT_EQ(ipx_bool2str(data_ptr.get(), res_ptr.get(), res_size), IPX_CONVERT_ERR_BUFFER);
}

// -----------------------------------------------------------------------------

/*
 * Test the datetime to string converter
 */
// Compare timestamps
void
datetime2strNormal_compare(const std::string &str1, const std::string &str2,
    uint64_t frac_eps = 0)
{
    SCOPED_TRACE("Test value: '" + str1 + "' and '" + str2 + "'");

    // Locate start of fractions
    size_t frac1_start = str1.find('.');
    size_t frac2_start = str2.find('.');
    ASSERT_EQ(frac1_start, frac2_start);

    // Compare the parts before fractions
    ASSERT_EQ(str1.substr(0, frac1_start), str2.substr(0, frac2_start));

    if (frac1_start == std::string::npos && frac2_start == std::string::npos) {
        // Nothing more to compare
        return;
    }

    ASSERT_NE(frac1_start, std::string::npos);
    ASSERT_NE(frac2_start, std::string::npos);

    size_t frac1_end;
    size_t frac2_end;
    uint64_t num1 = std::stoul(str1.substr(frac1_start + 1), &frac1_end);
    uint64_t num2 = std::stoul(str2.substr(frac2_start + 1), &frac2_end);
    frac1_end += frac1_start + 1;
    frac2_end += frac2_start + 1;

    // Compare length of fractions
    ASSERT_EQ(frac1_end - frac1_start, frac2_end - frac2_start);

    // Compare numbers
    uint64_t diff = (num1 > num2) ? num1 - num2 : num2 - num1;
    EXPECT_LE(diff, frac_eps) << "The difference between " + std::to_string(num1) + " and "
        + std::to_string(num2) + " is " + std::to_string(diff) + ", which exceeds "
        + std::to_string(frac_eps) + ".";

    // Compare the rest
    std::string str1_end = str1.substr(frac1_end);
    std::string str2_end = str2.substr(frac2_end);
    EXPECT_EQ(str1_end, str2_end);
}

void
datetime2strNormal_check(struct timespec ts, size_t data_size, enum ipx_element_type type)
{
    SCOPED_TRACE("Test value: " + std::to_string(ts.tv_sec) + " seconds, "
        + std::to_string(ts.tv_nsec) + " nanoseconds, type " + std::to_string(type)
        + ", size " + std::to_string(data_size));

    const size_t res_size = IPX_CONVERT_STRLEN_DATE;
    std::unique_ptr<char[]> res_ptr{new char[res_size]};
    std::unique_ptr<uint8_t[]> data_ptr{new uint8_t[data_size]};
    enum ipx_convert_time_fmt fmt;

    // Store the timestamp to a field
    ASSERT_EQ(ipx_set_datetime_hp_be(data_ptr.get(), data_size, type, ts), IPX_CONVERT_OK);

	// Compute the expected result
    switch (type) {
    case IPX_ET_DATE_TIME_SECONDS:
        ts.tv_nsec = 0;
        break;
    case IPX_ET_DATE_TIME_MILLISECONDS:
        ts.tv_nsec /= 1000000;
        ts.tv_nsec *= 1000000;
        break;
    case IPX_ET_DATE_TIME_MICROSECONDS:
    case IPX_ET_DATE_TIME_NANOSECONDS:
        // Nothing to do, because these both types use the same encoding
        break;
    default:
        FAIL() << "We shouldn't get here.";
    }

    struct tm time_utc;
    ASSERT_NE(gmtime_r(&ts.tv_sec, &time_utc), nullptr);
    std::unique_ptr<char[]> tmp_ptr{new char[res_size]};
    ASSERT_GT(strftime(tmp_ptr.get(), res_size, "%FT%T", &time_utc), 0U);

    std::string exp_str{tmp_ptr.get()};
    exp_str += ".";
    std::stringstream ss;
    ss << std::setw(9) << std::setfill('0') << ts.tv_nsec;
    exp_str += ss.str();
    exp_str += "Z";

    /*
     * Test format "nanoseconds"
     * Note: In case you try to read a value stored in microseconds as nanoseconds
     *   result can vary by up to 477 nanoseconds due to identical encoding.
     *   See https://tools.ietf.org/html/rfc7011#section-6.1.9
     */
    fmt = IPX_CONVERT_TF_NSEC_UTC;
    EXPECT_GT(ipx_datetime2str_be(data_ptr.get(), data_size, type, res_ptr.get(), res_size,
        fmt), 0);

    if (type == IPX_ET_DATE_TIME_MICROSECONDS) {
        // Special case
        const uint64_t max_diff = 477;
        datetime2strNormal_compare(exp_str, std::string(res_ptr.get()), max_diff);
    } else {
        // Normal case
        datetime2strNormal_compare(exp_str, std::string(res_ptr.get()));
    }

    // Test format "microseconds"
    fmt = IPX_CONVERT_TF_USEC_UTC;
    EXPECT_GT(ipx_datetime2str_be(data_ptr.get(), data_size, type, res_ptr.get(), res_size,
        fmt), 0);
    exp_str.erase(exp_str.size() - 4); // Decrease precision
    exp_str += "Z";
    datetime2strNormal_compare(exp_str, std::string(res_ptr.get()));

    // Test format "milliseconds"
    fmt = IPX_CONVERT_TF_MSEC_UTC;
    EXPECT_GT(ipx_datetime2str_be(data_ptr.get(), data_size, type, res_ptr.get(), res_size,
        fmt), 0);
    exp_str.erase(exp_str.size() - 4); // Decrease precision
    exp_str += "Z";
    datetime2strNormal_compare(exp_str, std::string(res_ptr.get()));

    // Test format "seconds"
    fmt = IPX_CONVERT_TF_SEC_UTC;
    EXPECT_GT(ipx_datetime2str_be(data_ptr.get(), data_size, type, res_ptr.get(), res_size,
        fmt), 0);
    exp_str.erase(exp_str.size() - 5); // Decrease precision
    exp_str += "Z";
    datetime2strNormal_compare(exp_str, std::string(res_ptr.get()));
}

TEST(ConverterToStrings, ipx_datetime2strUTCNormal)
{
    const uint64_t ntp_era_end_as_unix = 2085978495UL; // 7 February 2036 6:28:15
    const time_t msec_max_val = (std::numeric_limits<uint64_t>::max() / 1000) - 1;

    // Epoch start
    struct timespec t1 = {0, 0};
    datetime2strNormal_check(t1, BYTES_4, IPX_ET_DATE_TIME_SECONDS);
    datetime2strNormal_check(t1, BYTES_8, IPX_ET_DATE_TIME_MILLISECONDS);
    datetime2strNormal_check(t1, BYTES_8, IPX_ET_DATE_TIME_MICROSECONDS);
    datetime2strNormal_check(t1, BYTES_8, IPX_ET_DATE_TIME_NANOSECONDS);

    struct timespec t2 = {1501161713, 123456789};
    datetime2strNormal_check(t2, BYTES_4, IPX_ET_DATE_TIME_SECONDS);
    datetime2strNormal_check(t2, BYTES_8, IPX_ET_DATE_TIME_MILLISECONDS);
    datetime2strNormal_check(t2, BYTES_8, IPX_ET_DATE_TIME_MICROSECONDS);
    datetime2strNormal_check(t2, BYTES_8, IPX_ET_DATE_TIME_NANOSECONDS);

    struct timespec t3 = {ntp_era_end_as_unix, 999999999};
    datetime2strNormal_check(t3, BYTES_4, IPX_ET_DATE_TIME_SECONDS);
    datetime2strNormal_check(t3, BYTES_8, IPX_ET_DATE_TIME_MILLISECONDS);
    datetime2strNormal_check(t3, BYTES_8, IPX_ET_DATE_TIME_MICROSECONDS);
    datetime2strNormal_check(t3, BYTES_8, IPX_ET_DATE_TIME_NANOSECONDS);

    struct timespec t4 = {UINT32_MAX, 999999999};
    datetime2strNormal_check(t4, BYTES_4, IPX_ET_DATE_TIME_SECONDS);
    datetime2strNormal_check(t4, BYTES_8, IPX_ET_DATE_TIME_MILLISECONDS);
    /*
     * Following tests are disabled, because time wraparound is not implemented
     * datetime2strNormal_check(t4, BYTES_8, IPX_ET_DATE_TIME_MICROSECONDS);
     * datetime2strNormal_check(t4, BYTES_8, IPX_ET_DATE_TIME_NANOSECONDS);
    */

    struct timespec t5 = {msec_max_val, 999999999};
    datetime2strNormal_check(t5, BYTES_8, IPX_ET_DATE_TIME_MILLISECONDS);
    /*
     * Following tests are disabled, because time wraparound is not implemented
     * datetime2strNormal_check(t5, BYTES_4, IPX_ET_DATE_TIME_SECONDS);
     * datetime2strNormal_check(t5, BYTES_8, IPX_ET_DATE_TIME_MICROSECONDS);
     * datetime2strNormal_check(t5, BYTES_8, IPX_ET_DATE_TIME_NANOSECONDS);
    */
}

// TODO: add localtime tests

/**
 * @}
 */

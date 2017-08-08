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

    ASSERT_EQ(ipx_set_uint_be(data_ptr.get(), data_size, value), IPX_CONVERT_OK);
    ASSERT_EQ(ipx_uint2str_be(data_ptr.get(), data_size, res_ptr.get(), res_size),
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

    ASSERT_EQ(ipx_set_uint_be(data_ptr.get(), data_size, value), IPX_CONVERT_OK);
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
    EXPECT_EQ(ipx_uint2str_be(data_ptr.get(), 0, str_ptr.get(), size), IPX_CONVERT_ERR_ARG);
    EXPECT_EQ(ipx_uint2str_be(data_ptr.get(), 9, str_ptr.get(), size), IPX_CONVERT_ERR_ARG);

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

    ASSERT_EQ(ipx_set_int_be(data_ptr.get(), data_size, value), IPX_CONVERT_OK);
    ASSERT_EQ(ipx_int2str_be(data_ptr.get(), data_size, res_ptr.get(), res_size),
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

    ASSERT_EQ(ipx_set_int_be(data_ptr.get(), data_size, value), IPX_CONVERT_OK);
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
    EXPECT_EQ(ipx_int2str_be(data_ptr.get(), 0, str_ptr.get(), size), IPX_CONVERT_ERR_ARG);
    EXPECT_EQ(ipx_int2str_be(data_ptr.get(), 9, str_ptr.get(), size), IPX_CONVERT_ERR_ARG);

    // Test that memory is not accessed if invalid size is defined.
    EXPECT_EQ(ipx_int2str_be(NULL, 0, str_ptr.get(), size), IPX_CONVERT_ERR_ARG);
    EXPECT_EQ(ipx_int2str_be(NULL, 9, str_ptr.get(), size), IPX_CONVERT_ERR_ARG);
}

// -----------------------------------------------------------------------------

/*
 * Test the 32bit float to string converter
 */
void
float2strNormal_32check(float value)
{
    SCOPED_TRACE("Test value: " + std::to_string(value));
    int ret_code;
    const size_t data_size = sizeof(float);
    const size_t res_size = 16;

    // Prepare auxiliary arrays
    std::unique_ptr<uint8_t[]> data_ptr{new uint8_t[data_size]};
    std::unique_ptr<char[]> res_ptr{new char[res_size]};

    // Store and convert data using converters
    ASSERT_EQ(ipx_set_float_be(data_ptr.get(), data_size, value), IPX_CONVERT_OK);
    ret_code = ipx_float2str_be(data_ptr.get(), data_size, res_ptr.get(), res_size);
    ASSERT_GT(ret_code, 0);
    EXPECT_EQ(strlen(res_ptr.get()), static_cast<size_t>(ret_code));

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
    int ret_code;
    const size_t data_size = sizeof(double);
    const size_t res_size = 32;

    // Prepare auxiliary arrays
    std::unique_ptr<uint8_t[]> data_ptr{new uint8_t[data_size]};
    std::unique_ptr<char[]> res_ptr{new char[res_size]};

    // Store and convert data using converters
    ASSERT_EQ(ipx_set_float_be(data_ptr.get(), data_size, value), IPX_CONVERT_OK);
    ret_code = ipx_float2str_be(data_ptr.get(), data_size, res_ptr.get(), res_size);
    ASSERT_GT(ret_code, 0);
    EXPECT_EQ(strlen(res_ptr.get()), static_cast<size_t>(ret_code));

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
TEST(ConverterToStrings, float2strInvalidInput)
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

TEST(ConverterToStrings, float2strSmallBuffer)
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
    int ret_code;
    const size_t data_size = BYTES_1;
    std::unique_ptr<uint8_t[]> data_ptr{new uint8_t[data_size]};

    // Test "True"
    std::string true_str{"true"};
    size_t res_size = true_str.length() + 1; // 1 == '\0'
    std::unique_ptr<char[]> res_ptr{new char[res_size]};

    ASSERT_EQ(ipx_set_bool(data_ptr.get(), data_size, true), IPX_CONVERT_OK);
    ret_code = ipx_bool2str(data_ptr.get(), res_ptr.get(), res_size);
    ASSERT_GT(ret_code, 0);
    EXPECT_EQ(strlen(res_ptr.get()), static_cast<size_t>(ret_code));
    EXPECT_EQ(true_str, res_ptr.get());

    // Test "False"
    std::string false_str{"false"};
    res_size = false_str.length() + 1; // 1 == '\0'
    res_ptr.reset(new char[res_size]);

    ASSERT_EQ(ipx_set_bool(data_ptr.get(), data_size, false), IPX_CONVERT_OK);
    ret_code = ipx_bool2str(data_ptr.get(), res_ptr.get(), res_size);
    ASSERT_GT(ret_code, 0);
    EXPECT_EQ(strlen(res_ptr.get()), static_cast<size_t>(ret_code));
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
 * Test the datetime to string converter - UTC and Local timezone
 */
// Compare timestamps
void
datetime2str_compare(const std::string &str1, const std::string &str2,
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

// Compute expected result
void
datetime2str_get_expectation(struct timespec ts, enum ipx_element_type type,
    enum ipx_convert_time_fmt fmt, std::string &result)
{
    SCOPED_TRACE("Test value: " + std::to_string(ts.tv_sec) + " seconds, "
        + std::to_string(ts.tv_nsec) + " nanoseconds, type " + std::to_string(type)
        + ", fmt " + std::to_string(fmt));

    // Change the timestamp based on expected precision
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

    // Get get timestamp and timezone as strings (without fraction)
    std::stringstream ss_suffix;
    struct tm timestamp;

    switch (fmt) {
    case IPX_CONVERT_TF_SEC_UTC:
    case IPX_CONVERT_TF_MSEC_UTC:
    case IPX_CONVERT_TF_USEC_UTC:
    case IPX_CONVERT_TF_NSEC_UTC:
        ASSERT_NE(gmtime_r(&ts.tv_sec, &timestamp), nullptr);
        ss_suffix << "Z";
        break;
    case IPX_CONVERT_TF_SEC_LOCAL:
    case IPX_CONVERT_TF_MSEC_LOCAL:
    case IPX_CONVERT_TF_USEC_LOCAL:
    case IPX_CONVERT_TF_NSEC_LOCAL:
        ASSERT_NE(localtime_r(&ts.tv_sec, &timestamp), nullptr);
        ss_suffix << std::put_time(&timestamp, "%z");
        break;
    default:
        FAIL() << "We shouldn't get here.";
    }

    std::stringstream ss;
    ss << std::put_time(&timestamp, "%FT%T");
    ss << ".";
    ss << std::setw(9) << std::setfill('0') << ts.tv_nsec;
    result = ss.str();

    // Remove fraction part if required
    switch (fmt) {
    case IPX_CONVERT_TF_SEC_UTC:
    case IPX_CONVERT_TF_SEC_LOCAL:
        // Remove last 10 characters (9 numbers and decimal point)
        result.erase(result.size() - 10);
        break;
    case IPX_CONVERT_TF_MSEC_UTC:
    case IPX_CONVERT_TF_MSEC_LOCAL:
        // Remove last 6 characters (6 numbers)
        result.erase(result.size() - 6);
        break;
    case IPX_CONVERT_TF_USEC_UTC:
    case IPX_CONVERT_TF_USEC_LOCAL:
        // Remove last 3 characters (3 numbers)
        result.erase(result.size() - 3);
        break;
    case IPX_CONVERT_TF_NSEC_UTC:
    case IPX_CONVERT_TF_NSEC_LOCAL:
        // Do not remove anything
        break;
    default:
        FAIL() << "We shouldn't get here.";
    }

    result += ss_suffix.str();
}

void
datetime2str_check(struct timespec ts, size_t data_size, enum ipx_element_type type)
{
    SCOPED_TRACE("Test value: " + std::to_string(ts.tv_sec) + " seconds, "
        + std::to_string(ts.tv_nsec) + " nanoseconds, type " + std::to_string(type)
        + ", size " + std::to_string(data_size));

    // Store the timestamp to a field
    std::unique_ptr<uint8_t[]> data_ptr{new uint8_t[data_size]};
    ASSERT_EQ(ipx_set_datetime_hp_be(data_ptr.get(), data_size, type, ts), IPX_CONVERT_OK);

    auto test_func_success = [&](enum ipx_convert_time_fmt fmt) {
        // Prepare the expected result and an output buffer
        std::string exp_value;
        datetime2str_get_expectation(ts, type, fmt, exp_value);

        size_t res_size = exp_value.length() + 1; // +1 for '\0'
        std::unique_ptr<char[]> res_ptr{new char[res_size]};

        // Conversion function
        int ret_code;
        ret_code = ipx_datetime2str_be(data_ptr.get(), data_size, type, res_ptr.get(), res_size,
            fmt);
        ASSERT_GT(ret_code, 0);
        EXPECT_EQ(strlen(res_ptr.get()), static_cast<size_t>(ret_code));

        /* Compare results
         * Note: In case you try to read a value stored in microseconds as nanoseconds
         *   result can vary by up to 477 nanoseconds due to identical encoding.
         *   See https://tools.ietf.org/html/rfc7011#section-6.1.9
         */
        if ((fmt == IPX_CONVERT_TF_NSEC_LOCAL || fmt == IPX_CONVERT_TF_NSEC_UTC)
            && type == IPX_ET_DATE_TIME_MICROSECONDS) {
            const uint64_t max_diff = 477;
            datetime2str_compare(exp_value, std::string(res_ptr.get()), max_diff);
        } else {
            // Normal comparison without tolerance
            datetime2str_compare(exp_value, std::string(res_ptr.get()));
        }
    };

    // Test UTC conversion
    test_func_success(IPX_CONVERT_TF_NSEC_UTC);
    test_func_success(IPX_CONVERT_TF_USEC_UTC);
    test_func_success(IPX_CONVERT_TF_MSEC_UTC);
    test_func_success(IPX_CONVERT_TF_SEC_UTC);

    // Test local time conversion
    test_func_success(IPX_CONVERT_TF_NSEC_LOCAL);
    test_func_success(IPX_CONVERT_TF_USEC_LOCAL);
    test_func_success(IPX_CONVERT_TF_MSEC_LOCAL);
    test_func_success(IPX_CONVERT_TF_SEC_LOCAL);

    // Failure test - insufficient buffer size
    auto test_func_fail = [&](enum ipx_convert_time_fmt fmt) {
        // Prepare the expected result
        std::string exp_value;
        datetime2str_get_expectation(ts, type, fmt, exp_value);

        // Test all cases when the output buffer is always smaller than the required size
        size_t res_size = exp_value.length() + 1; // +1 for '\0'
        for (size_t i = 0; i < res_size; ++i) {
            std::unique_ptr<char[]> res_ptr{new char[i]};
            EXPECT_EQ(ipx_datetime2str_be(data_ptr.get(), data_size, type, res_ptr.get(), i, fmt),
                IPX_CONVERT_ERR_BUFFER);
        }
    };

    // Test UTC conversion
    test_func_fail(IPX_CONVERT_TF_NSEC_UTC);
    test_func_fail(IPX_CONVERT_TF_USEC_UTC);
    test_func_fail(IPX_CONVERT_TF_MSEC_UTC);
    test_func_fail(IPX_CONVERT_TF_SEC_UTC);

    // Test local time conversion
    test_func_fail(IPX_CONVERT_TF_NSEC_LOCAL);
    test_func_fail(IPX_CONVERT_TF_USEC_LOCAL);
    test_func_fail(IPX_CONVERT_TF_MSEC_LOCAL);
    test_func_fail(IPX_CONVERT_TF_SEC_LOCAL);
}

TEST(ConverterToStrings, datetime2strNormalAndSmallBuffer)
{
    const uint64_t ntp_era_end_as_unix = 2085978495UL; // 7 February 2036 6:28:15
    const time_t msec_max_val = (std::numeric_limits<uint64_t>::max() / 1000) - 1;

    // Epoch start
    struct timespec t1 = {0, 0};
    datetime2str_check(t1, BYTES_4, IPX_ET_DATE_TIME_SECONDS);
    datetime2str_check(t1, BYTES_8, IPX_ET_DATE_TIME_MILLISECONDS);
    datetime2str_check(t1, BYTES_8, IPX_ET_DATE_TIME_MICROSECONDS);
    datetime2str_check(t1, BYTES_8, IPX_ET_DATE_TIME_NANOSECONDS);

    struct timespec t2 = {1501161713, 123456789};
    datetime2str_check(t2, BYTES_4, IPX_ET_DATE_TIME_SECONDS);
    datetime2str_check(t2, BYTES_8, IPX_ET_DATE_TIME_MILLISECONDS);
    datetime2str_check(t2, BYTES_8, IPX_ET_DATE_TIME_MICROSECONDS);
    datetime2str_check(t2, BYTES_8, IPX_ET_DATE_TIME_NANOSECONDS);

    // Maximum possible date of nanosecond and microsecond precision
    struct timespec t3 = {ntp_era_end_as_unix, 999999999};
    datetime2str_check(t3, BYTES_4, IPX_ET_DATE_TIME_SECONDS);
    datetime2str_check(t3, BYTES_8, IPX_ET_DATE_TIME_MILLISECONDS);
    datetime2str_check(t3, BYTES_8, IPX_ET_DATE_TIME_MICROSECONDS);
    datetime2str_check(t3, BYTES_8, IPX_ET_DATE_TIME_NANOSECONDS);

    // Maximum possible date of second precision
    struct timespec t4 = {UINT32_MAX, 999999999};
    datetime2str_check(t4, BYTES_4, IPX_ET_DATE_TIME_SECONDS);
    datetime2str_check(t4, BYTES_8, IPX_ET_DATE_TIME_MILLISECONDS);
    /*
     * Following tests are disabled, because time wraparound is not implemented
     * datetime2str_check(t4, BYTES_8, IPX_ET_DATE_TIME_MICROSECONDS);
     * datetime2str_check(t4, BYTES_8, IPX_ET_DATE_TIME_NANOSECONDS);
    */

    // Maximum possible date of millisecond precision
    struct timespec t5 = {msec_max_val, 999999999};
    datetime2str_check(t5, BYTES_8, IPX_ET_DATE_TIME_MILLISECONDS);
    /*
     * Following tests are disabled, because time wraparound is not implemented
     * datetime2str_check(t5, BYTES_4, IPX_ET_DATE_TIME_SECONDS);
     * datetime2str_check(t5, BYTES_8, IPX_ET_DATE_TIME_MICROSECONDS);
     * datetime2str_check(t5, BYTES_8, IPX_ET_DATE_TIME_NANOSECONDS);
    */
}

/*
 * Test invalid input size
 */
void
datetime2str_invalid_size(enum ipx_convert_time_fmt fmt)
{
    const size_t res_size = IPX_CONVERT_STRLEN_DATE;
    std::unique_ptr<char[]> res_ptr{new char[res_size]};

    for (size_t i = 0; i <= 16; ++i) {
        // Prepare an array
        std::unique_ptr<uint8_t[]> data_ptr{new uint8_t[i]()};

        if (i != BYTES_4) {
            // 4 bytes are correct -> we want only failures
            EXPECT_EQ(ipx_datetime2str_be(data_ptr.get(), i, IPX_ET_DATE_TIME_SECONDS,
                res_ptr.get(), res_size, fmt), IPX_CONVERT_ERR_ARG);
        }

        if (i != BYTES_8) {
            // 8 bytes are correct -> we want only failures
            EXPECT_EQ(ipx_datetime2str_be(data_ptr.get(), i, IPX_ET_DATE_TIME_MILLISECONDS,
                res_ptr.get(), res_size, fmt), IPX_CONVERT_ERR_ARG);
            EXPECT_EQ(ipx_datetime2str_be(data_ptr.get(), i, IPX_ET_DATE_TIME_MICROSECONDS,
                res_ptr.get(), res_size, fmt), IPX_CONVERT_ERR_ARG);
            EXPECT_EQ(ipx_datetime2str_be(data_ptr.get(), i, IPX_ET_DATE_TIME_NANOSECONDS,
                res_ptr.get(), res_size, fmt), IPX_CONVERT_ERR_ARG);
        }
    }
}

void
datetime2str_invalid_type(enum ipx_element_type type)
{
    const size_t res_size = IPX_CONVERT_STRLEN_DATE;
    std::unique_ptr<char[]> res_ptr{new char[res_size]};

    std::unique_ptr<uint8_t[]> data_ptr{new uint8_t[BYTES_8]()};
    std::vector<enum ipx_convert_time_fmt> fmt_vec = {
        IPX_CONVERT_TF_SEC_UTC, IPX_CONVERT_TF_MSEC_UTC, IPX_CONVERT_TF_USEC_UTC,
        IPX_CONVERT_TF_NSEC_UTC, IPX_CONVERT_TF_SEC_LOCAL, IPX_CONVERT_TF_MSEC_LOCAL,
        IPX_CONVERT_TF_USEC_LOCAL, IPX_CONVERT_TF_NSEC_LOCAL
    };

    for (ipx_convert_time_fmt fmt : fmt_vec) {
        SCOPED_TRACE("Test format: " + std::to_string(fmt));
        EXPECT_EQ(ipx_datetime2str_be(data_ptr.get(), BYTES_4, type, res_ptr.get(), res_size, fmt),
            IPX_CONVERT_ERR_ARG);
        EXPECT_EQ(ipx_datetime2str_be(data_ptr.get(), BYTES_8, type, res_ptr.get(), res_size, fmt),
            IPX_CONVERT_ERR_ARG);
    }
}

TEST(ConverterToStrings, datetime2strInvalidInput)
{
    // Invalid size of input field test
    std::vector<enum ipx_convert_time_fmt> fmt_vec = {
        IPX_CONVERT_TF_SEC_UTC, IPX_CONVERT_TF_MSEC_UTC, IPX_CONVERT_TF_USEC_UTC,
        IPX_CONVERT_TF_NSEC_UTC, IPX_CONVERT_TF_SEC_LOCAL, IPX_CONVERT_TF_MSEC_LOCAL,
        IPX_CONVERT_TF_USEC_LOCAL, IPX_CONVERT_TF_NSEC_LOCAL
    };

    for (ipx_convert_time_fmt fmt : fmt_vec) {
        SCOPED_TRACE("Test format: " + std::to_string(fmt));
        datetime2str_invalid_size(fmt);
    }

    // Invalid field type test
    std::vector<enum ipx_element_type> type_vec = { // Does not include valid datetime types)
        IPX_ET_OCTET_ARRAY, IPX_ET_UNSIGNED_8, IPX_ET_UNSIGNED_16, IPX_ET_UNSIGNED_32,
        IPX_ET_UNSIGNED_64, IPX_ET_SIGNED_8, IPX_ET_SIGNED_16, IPX_ET_SIGNED_32,
        IPX_ET_SIGNED_64, IPX_ET_FLOAT_32, IPX_ET_FLOAT_64, IPX_ET_BOOLEAN, IPX_ET_MAC_ADDRESS,
        IPX_ET_STRING, IPX_ET_IPV4_ADDRESS, IPX_ET_IPV6_ADDRESS, IPX_ET_BASIC_LIST,
        IPX_ET_SUB_TEMPLATE_LIST, IPX_ET_SUB_TEMPLATE_MULTILIST
    };

    for (ipx_element_type type : type_vec) {
        SCOPED_TRACE("Test type: " + std::to_string(type));
        datetime2str_invalid_type(type);
    }
}

// -----------------------------------------------------------------------------

/*
 * Test mac to string converter
 */
void
mac2strNormal_str2mac(const std::string &mac, uint8_t mac_data[6])
{
    unsigned int bytes[6];
    if (sscanf(mac.c_str(), "%02x:%02x:%02x:%02x:%02x:%02x", &bytes[0], &bytes[1], &bytes[2],
        &bytes[3], &bytes[4], &bytes[5]) != 6) {
        FAIL() << "Converting the MAC address " + mac + " to an array of bytes failed";
    }

    for (size_t i = 0; i < 6; ++i) {
        mac_data[i] = static_cast<uint8_t>(bytes[i]);
    }
}

void
mac2strNormal_check(const std::string &mac)
{
    int ret_code;
    const size_t data_size = 6;

    // Convert MAC to bytes
    uint8_t input_arr[6];
    mac2strNormal_str2mac(mac, input_arr);

    // Set data array
    std::unique_ptr<uint8_t[]> data_ptr{new uint8_t[data_size]};
    ASSERT_EQ(ipx_set_mac(data_ptr.get(), data_size, input_arr), IPX_CONVERT_OK);

    // Get the array as a formatted string
    const size_t res_size = IPX_CONVERT_STRLEN_MAC;
    std::unique_ptr<char[]> res_ptr{new char[res_size]};
    ret_code = ipx_mac2str(data_ptr.get(), data_size, res_ptr.get(), res_size);
    ASSERT_GT(ret_code, 0);
    EXPECT_EQ(strlen(res_ptr.get()), static_cast<size_t>(ret_code));

    // Compare strings
    EXPECT_EQ(strcasecmp(mac.c_str(), res_ptr.get()), 0);
}

TEST(ConverterToStrings, mac2strNormal)
{
    mac2strNormal_check("00:00:00:00:00:00");
    mac2strNormal_check("00:FF:00:FF:00:FF");
    mac2strNormal_check("FF:00:FF:00:FF:00");
    mac2strNormal_check("FF:FF:FF:FF:FF:FF");
    mac2strNormal_check("01:23:45:67:89:ab");
    mac2strNormal_check("90:1b:0e:17:17:91");
}

TEST(ConverterToStrings, mac2strInvalidInput)
{
    const size_t mac_size = 6;
    uint8_t mac_data[mac_size];
    mac2strNormal_str2mac("12:23:34:45:56:67", mac_data);

    // Invalid size of the output buffer
    for (size_t i = 0; i < IPX_CONVERT_STRLEN_MAC; ++i) {
        SCOPED_TRACE("i = " + std::to_string(i));
        std::unique_ptr<char[]> res_ptr{new char[i]};
        EXPECT_EQ(ipx_mac2str(mac_data, mac_size, res_ptr.get(), i), IPX_CONVERT_ERR_BUFFER);
    }

    // Invalid size of the field
    const size_t res_size = IPX_CONVERT_STRLEN_MAC;
    std::unique_ptr<char[]> res_ptr{new char[res_size]};

    for (size_t i = 0; i <= 16; ++i) {
        if (i == mac_size) {
            // Skip valid size!
            continue;
        }

        SCOPED_TRACE("i = " + std::to_string(i));
        std::unique_ptr<uint8_t[]> data_ptr{new uint8_t[i]};
        EXPECT_EQ(ipx_mac2str(data_ptr.get(), i, res_ptr.get(), res_size), IPX_CONVERT_ERR_ARG);
    }
}



// TODO: ip, octetarray, string
// TODO: zkontrolovat ze vystup opravdu opovida zapsané delce (ret_code == strlen)

/**
 * @}
 */

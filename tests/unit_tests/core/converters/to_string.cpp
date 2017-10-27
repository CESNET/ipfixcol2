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
#include <arpa/inet.h> // inet_pton

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

    ASSERT_EQ(ipx_set_uint_be(data_ptr.get(), data_size, value), IPX_OK);
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

    ASSERT_EQ(ipx_set_uint_be(data_ptr.get(), data_size, value), IPX_OK);
    EXPECT_EQ(ipx_uint2str_be(data_ptr.get(), data_size, str_ptr.get(), res_size),
        IPX_ERR_BUFFER);
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
    EXPECT_EQ(ipx_uint2str_be(data_ptr.get(), 0, str_ptr.get(), size), IPX_ERR_ARG);
    EXPECT_EQ(ipx_uint2str_be(data_ptr.get(), 9, str_ptr.get(), size), IPX_ERR_ARG);

    // Test that memory is not accessed if invalid size is defined.
    EXPECT_EQ(ipx_uint2str_be(NULL, 0, str_ptr.get(), size),
            IPX_ERR_ARG);
    EXPECT_EQ(ipx_uint2str_be(NULL, 9, str_ptr.get(), size),
            IPX_ERR_ARG);
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

    ASSERT_EQ(ipx_set_int_be(data_ptr.get(), data_size, value), IPX_OK);
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

    ASSERT_EQ(ipx_set_int_be(data_ptr.get(), data_size, value), IPX_OK);
    EXPECT_EQ(ipx_int2str_be(data_ptr.get(), data_size, str_ptr.get(), res_size),
        IPX_ERR_BUFFER);
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
    EXPECT_EQ(ipx_int2str_be(data_ptr.get(), 0, str_ptr.get(), size), IPX_ERR_ARG);
    EXPECT_EQ(ipx_int2str_be(data_ptr.get(), 9, str_ptr.get(), size), IPX_ERR_ARG);

    // Test that memory is not accessed if invalid size is defined.
    EXPECT_EQ(ipx_int2str_be(NULL, 0, str_ptr.get(), size), IPX_ERR_ARG);
    EXPECT_EQ(ipx_int2str_be(NULL, 9, str_ptr.get(), size), IPX_ERR_ARG);
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
    ASSERT_EQ(ipx_set_float_be(data_ptr.get(), data_size, value), IPX_OK);
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
    ASSERT_EQ(ipx_set_float_be(data_ptr.get(), data_size, value), IPX_OK);
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
            IPX_ERR_ARG);
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

    EXPECT_EQ(ipx_set_float_be(data_ptr.get(), data_size, value), IPX_OK);
    EXPECT_EQ(ipx_float2str_be(data_ptr.get(), data_size, out_ptr.get(), out_len),
        IPX_ERR_BUFFER);
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

    EXPECT_EQ(ipx_set_float_be(data_ptr.get(), data_size, value), IPX_OK);
    EXPECT_EQ(ipx_float2str_be(data_ptr.get(), data_size, out_ptr.get(), out_len),
            IPX_ERR_BUFFER);
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

    ASSERT_EQ(ipx_set_bool(data_ptr.get(), data_size, true), IPX_OK);
    ret_code = ipx_bool2str(data_ptr.get(), res_ptr.get(), res_size);
    ASSERT_GT(ret_code, 0);
    EXPECT_EQ(strlen(res_ptr.get()), static_cast<size_t>(ret_code));
    EXPECT_EQ(true_str, res_ptr.get());

    // Test "False"
    std::string false_str{"false"};
    res_size = false_str.length() + 1; // 1 == '\0'
    res_ptr.reset(new char[res_size]);

    ASSERT_EQ(ipx_set_bool(data_ptr.get(), data_size, false), IPX_OK);
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
        EXPECT_EQ(ipx_bool2str(&data, res_ptr.get(), res_size), IPX_ERR_ARG);
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

    EXPECT_EQ(ipx_set_bool(data_ptr.get(), data_size, true), IPX_OK);
    EXPECT_EQ(ipx_bool2str(data_ptr.get(), res_ptr.get(), res_size), IPX_ERR_BUFFER);

    // Test "False"
    std::string false_str{"false"};
    res_size = false_str.length(); // Not enough space for '\0'
    res_ptr.reset(new char[res_size]);

    EXPECT_EQ(ipx_set_bool(data_ptr.get(), data_size, false), IPX_OK);
    EXPECT_EQ(ipx_bool2str(data_ptr.get(), res_ptr.get(), res_size), IPX_ERR_BUFFER);
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
datetime2str_get_expectation(struct timespec ts, enum fds_iemgr_element_type type,
    enum ipx_convert_time_fmt fmt, std::string &result)
{
    SCOPED_TRACE("Test value: " + std::to_string(ts.tv_sec) + " seconds, "
        + std::to_string(ts.tv_nsec) + " nanoseconds, type " + std::to_string(type)
        + ", fmt " + std::to_string(fmt));

    // Change the timestamp based on expected precision
    switch (type) {
    case FDS_ET_DATE_TIME_SECONDS:
        ts.tv_nsec = 0;
        break;
    case FDS_ET_DATE_TIME_MILLISECONDS:
        ts.tv_nsec /= 1000000;
        ts.tv_nsec *= 1000000;
        break;
    case FDS_ET_DATE_TIME_MICROSECONDS:
    case FDS_ET_DATE_TIME_NANOSECONDS:
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
datetime2str_check(struct timespec ts, size_t data_size, enum fds_iemgr_element_type type)
{
    SCOPED_TRACE("Test value: " + std::to_string(ts.tv_sec) + " seconds, "
        + std::to_string(ts.tv_nsec) + " nanoseconds, type " + std::to_string(type)
        + ", size " + std::to_string(data_size));

    // Store the timestamp to a field
    std::unique_ptr<uint8_t[]> data_ptr{new uint8_t[data_size]};
    ASSERT_EQ(ipx_set_datetime_hp_be(data_ptr.get(), data_size, type, ts), IPX_OK);

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
            && type == FDS_ET_DATE_TIME_MICROSECONDS) {
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
                IPX_ERR_BUFFER);
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
    datetime2str_check(t1, BYTES_4, FDS_ET_DATE_TIME_SECONDS);
    datetime2str_check(t1, BYTES_8, FDS_ET_DATE_TIME_MILLISECONDS);
    datetime2str_check(t1, BYTES_8, FDS_ET_DATE_TIME_MICROSECONDS);
    datetime2str_check(t1, BYTES_8, FDS_ET_DATE_TIME_NANOSECONDS);

    struct timespec t2 = {1501161713, 123456789};
    datetime2str_check(t2, BYTES_4, FDS_ET_DATE_TIME_SECONDS);
    datetime2str_check(t2, BYTES_8, FDS_ET_DATE_TIME_MILLISECONDS);
    datetime2str_check(t2, BYTES_8, FDS_ET_DATE_TIME_MICROSECONDS);
    datetime2str_check(t2, BYTES_8, FDS_ET_DATE_TIME_NANOSECONDS);

    // Maximum possible date of nanosecond and microsecond precision
    struct timespec t3 = {ntp_era_end_as_unix, 999999999};
    datetime2str_check(t3, BYTES_4, FDS_ET_DATE_TIME_SECONDS);
    datetime2str_check(t3, BYTES_8, FDS_ET_DATE_TIME_MILLISECONDS);
    datetime2str_check(t3, BYTES_8, FDS_ET_DATE_TIME_MICROSECONDS);
    datetime2str_check(t3, BYTES_8, FDS_ET_DATE_TIME_NANOSECONDS);

    // Maximum possible date of second precision
    struct timespec t4 = {UINT32_MAX, 999999999};
    datetime2str_check(t4, BYTES_4, FDS_ET_DATE_TIME_SECONDS);
    datetime2str_check(t4, BYTES_8, FDS_ET_DATE_TIME_MILLISECONDS);
    /*
     * Following tests are disabled, because time wraparound is not implemented
     * datetime2str_check(t4, BYTES_8, FDS_ET_DATE_TIME_MICROSECONDS);
     * datetime2str_check(t4, BYTES_8, FDS_ET_DATE_TIME_NANOSECONDS);
    */

    // Maximum possible date of millisecond precision
    struct timespec t5 = {msec_max_val, 999999999};
    datetime2str_check(t5, BYTES_8, FDS_ET_DATE_TIME_MILLISECONDS);
    /*
     * Following tests are disabled, because time wraparound is not implemented
     * datetime2str_check(t5, BYTES_4, FDS_ET_DATE_TIME_SECONDS);
     * datetime2str_check(t5, BYTES_8, FDS_ET_DATE_TIME_MICROSECONDS);
     * datetime2str_check(t5, BYTES_8, FDS_ET_DATE_TIME_NANOSECONDS);
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
            EXPECT_EQ(ipx_datetime2str_be(data_ptr.get(), i, FDS_ET_DATE_TIME_SECONDS,
                res_ptr.get(), res_size, fmt), IPX_ERR_ARG);
        }

        if (i != BYTES_8) {
            // 8 bytes are correct -> we want only failures
            EXPECT_EQ(ipx_datetime2str_be(data_ptr.get(), i, FDS_ET_DATE_TIME_MILLISECONDS,
                res_ptr.get(), res_size, fmt), IPX_ERR_ARG);
            EXPECT_EQ(ipx_datetime2str_be(data_ptr.get(), i, FDS_ET_DATE_TIME_MICROSECONDS,
                res_ptr.get(), res_size, fmt), IPX_ERR_ARG);
            EXPECT_EQ(ipx_datetime2str_be(data_ptr.get(), i, FDS_ET_DATE_TIME_NANOSECONDS,
                res_ptr.get(), res_size, fmt), IPX_ERR_ARG);
        }
    }
}

void
datetime2str_invalid_type(enum fds_iemgr_element_type type)
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
            IPX_ERR_ARG);
        EXPECT_EQ(ipx_datetime2str_be(data_ptr.get(), BYTES_8, type, res_ptr.get(), res_size, fmt),
            IPX_ERR_ARG);
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
    std::vector<enum fds_iemgr_element_type> type_vec = { // Does not include valid datetime types)
        FDS_ET_OCTET_ARRAY, FDS_ET_UNSIGNED_8, FDS_ET_UNSIGNED_16, FDS_ET_UNSIGNED_32,
        FDS_ET_UNSIGNED_64, FDS_ET_SIGNED_8, FDS_ET_SIGNED_16, FDS_ET_SIGNED_32,
        FDS_ET_SIGNED_64, FDS_ET_FLOAT_32, FDS_ET_FLOAT_64, FDS_ET_BOOLEAN, FDS_ET_MAC_ADDRESS,
        FDS_ET_STRING, FDS_ET_IPV4_ADDRESS, FDS_ET_IPV6_ADDRESS, FDS_ET_BASIC_LIST,
        FDS_ET_SUB_TEMPLATE_LIST, FDS_ET_SUB_TEMPLATE_MULTILIST
    };

    for (fds_iemgr_element_type type : type_vec) {
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
    ASSERT_EQ(ipx_set_mac(data_ptr.get(), data_size, input_arr), IPX_OK);

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

/*
 * Test invalid size of input fields and insufficient size of output buffer
 */
TEST(ConverterToStrings, mac2strInvalidInput)
{
    const size_t mac_size = 6;
    uint8_t mac_data[mac_size];
    mac2strNormal_str2mac("12:23:34:45:56:67", mac_data);

    // Invalid size of the output buffer
    for (size_t i = 0; i < IPX_CONVERT_STRLEN_MAC; ++i) {
        SCOPED_TRACE("i = " + std::to_string(i));
        std::unique_ptr<char[]> res_ptr{new char[i]};
        EXPECT_EQ(ipx_mac2str(mac_data, mac_size, res_ptr.get(), i), IPX_ERR_BUFFER);
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
        EXPECT_EQ(ipx_mac2str(data_ptr.get(), i, res_ptr.get(), res_size), IPX_ERR_ARG);
    }
}

// -----------------------------------------------------------------------------

/*
 * Test IPv4/IPv6 to string converter
 */
void
ip2strNormal_check(std::string addr_str)
{
    SCOPED_TRACE("Test IP: " + addr_str);

    // Try to convert IP address into binary form
    struct in6_addr addr_bin;
    size_t addr_size;
    int af_family;

    if (inet_pton(AF_INET, addr_str.c_str(), &addr_bin) == 1) {
        addr_size = 4;  // IPv4
        af_family = AF_INET;
    } else if (inet_pton(AF_INET6, addr_str.c_str(), &addr_bin) == 1) {
        addr_size = 16; // IPv6
        af_family = AF_INET6;
    } else {
        FAIL() << "Failed to convert '" + addr_str + "' into binary form";
    }

    // Store IP to a field
    size_t data_size = addr_size;
    std::unique_ptr<uint8_t[]> data_ptr{new uint8_t[data_size]};
    ASSERT_EQ(ipx_set_ip(data_ptr.get(), data_size, &addr_bin), IPX_OK);

    // Convert it back into string
    size_t res_size = IPX_CONVERT_STRLEN_IP;
    std::unique_ptr<char[]> res_ptr{new char[res_size]};

    int ret_code;
    ret_code = ipx_ip2str(data_ptr.get(), data_size, res_ptr.get(), res_size);
    ASSERT_GT(ret_code, 0);
    EXPECT_EQ(strlen(res_ptr.get()), static_cast<size_t>(ret_code));

    // Try to convert it back into binary form
    struct in6_addr addr_new;
    ASSERT_EQ(inet_pton(af_family, res_ptr.get(), &addr_new), 1);

    // Compare binary forms
    EXPECT_EQ(memcmp(&addr_bin, &addr_new, addr_size), 0);
}


TEST(ConverterToStrings, ip2strNormal)
{
    // IPv4 addresses
    ip2strNormal_check("0.0.0.0");
    ip2strNormal_check("255.255.255.255");

    ip2strNormal_check("10.0.0.0");
    ip2strNormal_check("176.16.0.0");
    ip2strNormal_check("192.168.0.0");

    ip2strNormal_check("127.0.0.1");
    ip2strNormal_check("1.2.3.4");
    ip2strNormal_check("123.234.123.234");
    ip2strNormal_check("147.229.9.43");

    // IPv6 addresses
    ip2strNormal_check("0:0:0:0:0:0:0:0");
    ip2strNormal_check("00FF:FF00:00FF:FF00:00FF:FF00:00FF:FF00");
    ip2strNormal_check("FFFF:FFFF:FFFF:FFFF:FFFF:FFFF:FFFF:FFFF");

    ip2strNormal_check("::");
    ip2strNormal_check("::1");
    ip2strNormal_check("fc00::");
    ip2strNormal_check("::ffff:0:0");
    ip2strNormal_check("64:ff9b::");
    ip2strNormal_check("2002::");
    ip2strNormal_check("2001:20::");

    ip2strNormal_check("2001:718:1:101::144:228");
    ip2strNormal_check("2001:db8:0:0:1:0:0:1");
    ip2strNormal_check("2001:db8:a0b:12f0::1");
    ip2strNormal_check("2a00:1450:4014:80c::200e");
    ip2strNormal_check("2a03:2880:f106:83:face:b00c:0:25de");
}

void
ip2strInvalid_small_buffer(std::string addr_str)
{
    // Try to convert IP address into binary form
    struct in6_addr addr_bin;
    size_t addr_size;

    if (inet_pton(AF_INET, addr_str.c_str(), &addr_bin) == 1) {
        addr_size = 4;  // IPv4
    } else if (inet_pton(AF_INET6, addr_str.c_str(), &addr_bin) == 1) {
        addr_size = 16; // IPv6
    } else {
        FAIL() << "Failed to convert '" + addr_str + "' into binary form";
    }

    // Store IP to a field
    size_t data_size = addr_size;
    std::unique_ptr<uint8_t[]> data_ptr{new uint8_t[data_size]};
    ASSERT_EQ(ipx_set_ip(data_ptr.get(), data_size, &addr_bin), IPX_OK);

    // Convert it back into string
    size_t res_size = IPX_CONVERT_STRLEN_IP;
    std::unique_ptr<char[]> res_ptr{new char[res_size]};
    int ret_code;
    ret_code = ipx_ip2str(data_ptr.get(), data_size, res_ptr.get(), res_size);
    ASSERT_GT(ret_code, 0);

    // Now we know required size of the buffer... try smaller buffers
    size_t max_len = static_cast<size_t>(ret_code) + 1; // +1 == '\0'
    for (size_t i = 0; i < max_len; ++i) {
        std::unique_ptr<char[]> tmp_ptr{new char[i]};
        EXPECT_EQ(ipx_ip2str(data_ptr.get(), data_size, tmp_ptr.get(), i), IPX_ERR_BUFFER);
    }
}

/*
 * Test invalid size of input fields and insufficient size of output buffer
 */
TEST(ConverterToStrings, ip2strInvalid)
{
    // Try invalid field size
    size_t res_size = IPX_CONVERT_STRLEN_IP;
    std::unique_ptr<char[]> res_ptr{new char[res_size]};

    for (size_t i = 0; i < 32U; ++i) {
        if (i == 4U || i == 16U) {
            // Skip valid field sizes
            continue;
        }

        SCOPED_TRACE("Size: " + std::to_string(i));
        std::unique_ptr<uint8_t[]> data_ptr{new uint8_t[i]};
        EXPECT_EQ(ipx_ip2str(data_ptr.get(), i, res_ptr.get(), res_size), IPX_ERR_ARG);
    }

    // Try insufficient size of output buffer
    ip2strInvalid_small_buffer("0.0.0.0");
    ip2strInvalid_small_buffer("255.255.255.255");
    ip2strInvalid_small_buffer("147.229.9.43");

    ip2strInvalid_small_buffer("0:0:0:0:0:0:0:0");
    ip2strInvalid_small_buffer("00FF:FF00:00FF:FF00:00FF:FF00:00FF:FF00");
    ip2strInvalid_small_buffer("FFFF:FFFF:FFFF:FFFF:FFFF:FFFF:FFFF:FFFF");
    ip2strInvalid_small_buffer("::");
    ip2strInvalid_small_buffer("2a03:2880:f106:83:face:b00c:0:25de");
}

// -----------------------------------------------------------------------------

/*
 * Test octet array to string converter
 */
void
octet_array2strNormal_check(const uint8_t octet_data[], size_t octet_size)
{
    // Calculate expected result
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (size_t i = 0; i < octet_size; ++i) {
        ss << std::setw(2) << (unsigned int) octet_data[i];
    }
    std::string exp_str{ss.str()};

    // Convert to string
    size_t res_size = (2 * octet_size) + 1; // Based on documentation of conversion function
    std::unique_ptr<char[]> res_ptr{new char[res_size]};

    int ret_code = ipx_octet_array2str(octet_data, octet_size, res_ptr.get(), res_size);
    ASSERT_GE(ret_code, 0);
    EXPECT_EQ(strlen(res_ptr.get()), static_cast<size_t>(ret_code));

    // Compare
    EXPECT_STRCASEEQ(exp_str.c_str(), res_ptr.get());
}

TEST(ConverterToStrings, octet_array2strNormal)
{
    octet_array2strNormal_check(nullptr, 0);
    const uint8_t oa1[] = {0x00};
    octet_array2strNormal_check(oa1, sizeof(oa1));
    const uint8_t oa2[] = {0xFF};
    octet_array2strNormal_check(oa2, sizeof(oa2));
    const uint8_t oa3[] = {0xFA, 0xCE, 0xB0, 0x0C};
    octet_array2strNormal_check(oa3, sizeof(oa3));

    // Try all symbols
    constexpr size_t oa_full_size = 256;
    uint8_t oa_full[oa_full_size];
    for (size_t i = 0; i < oa_full_size; ++i) {
        oa_full[i] = i;
    }
    octet_array2strNormal_check(oa_full, oa_full_size);
}

/*
 * Test insufficient output buffer size
 */
TEST(ConverterToStrings, octet_array2strInvalid)
{
    constexpr size_t oa_size = 32;
    const uint8_t oa_data[oa_size] = {0};

    const size_t required_size = (2 * oa_size) + 1;
    for (size_t i = 0; i < required_size; ++i) {
        std::unique_ptr<char[]> res_ptr{new char[i]};
        EXPECT_EQ(ipx_octet_array2str(oa_data, oa_size, res_ptr.get(), i), IPX_ERR_BUFFER);
    }
}

// -----------------------------------------------------------------------------

/*
 * Test IPFIX UTF-8 string to string converter (only valid cases)
 */
TEST(ConverterToStrings, ipx_string2strNormal)
{
    /* Sentence "I can eat glass and it doesn't hurt me." in multiple languages and alphabets
     * See: http://www.columbia.edu/~fdc/utf8/
     */
    std::vector<std::string> str_list = {
        "I can eat glass and it doesn't hurt me.", // English
        "⠊⠀⠉⠁⠝⠀⠑⠁⠞⠀⠛⠇⠁⠎⠎⠀⠁⠝⠙⠀⠊⠞⠀⠙⠕⠑⠎⠝⠞⠀⠓⠥⠗⠞⠀⠍⠑", // English (Braille)
        "ЌЌЌ ЌЌЌЍ Ќ̈ЍЌЌ, ЌЌ ЌЌЍ ЍЌ ЌЌЌЌ ЌЍЌЌЌЌЌ", // Gothic
        "काचं शक्नोम्यत्तुम् । नोपहिनस्ति माम् ॥", // Sanskrit
        "Je peux manger du verre, ça ne me fait pas mal.", // French
        "Posso mangiare il vetro e non mi fa male.", // Italian
        "Pot să mănânc sticlă și ea nu mă rănește.", // Romanian
        "Dw i'n gallu bwyta gwydr, 'dyw e ddim yn gwneud dolur i mi.", // Welsh
        "Is féidir liom gloinne a ithe. Ní dhéanann sí dochar ar bith dom.", // Irish
        "ᛁᚳ᛫ᛗᚨᚷ᛫ᚷᛚᚨᛋ᛫ᛖᚩᛏᚪᚾ᛫ᚩᚾᛞ᛫ᚻᛁᛏ᛫ᚾᛖ᛫ᚻᛖᚪᚱᛗᛁᚪᚧ᛫ᛗᛖ᛬", // Anglo-Saxon (Runes)
        "Ic mæg glæs eotan ond hit ne hearmiað me.", // Anglo-Saxon (Latin)
        " ᛖᚴ ᚷᛖᛏ ᛖᛏᛁ ᚧ ᚷᛚᛖᚱ ᛘᚾ ᚦᛖᛋᛋ ᚨᚧ ᚡᛖ ᚱᚧᚨ ᛋᚨᚱ", // Old Norse (Runes)
        "Eg kan eta glas utan å skada meg.", // Norwegian (Nynorsk)
        "Jeg kan spise glas, det gør ikke ondt på mig.", // Danish
        "Æ ka æe glass uhen at det go mæ naue.", // Sønderjysk
        "Ich kann Glas essen, ohne mir zu schaden.", // German
        "Meg tudom enni az üveget, nem lesz tőle bajom.", // Hungarian
        "Мон ярсан суликадо, ды зыян эйстэнзэ а ули.", // Erzian
        "Aš galiu valgyti stiklą ir jis manęs nežeidžia.", // Lithuanian
        "Es varu ēst stiklu, tas man nekaitē.", // Litvian
        "Mohu jíst sklo, neublíží mi.", // Czech
        "Môžem jesť sklo. Nezraní ma.", // Slovak
        "Mogę jeść szkło i mi nie szkodzi.", // Polish
        "Можам да јадам стакло, а не ме штета.", // Macedonian
        "Я могу есть стекло, оно мне не вредит.", // Russian
        "Я магу есці шкло, яно мне не шкодзіць.", // Belarusian
        "Мога да ям стъкло, то не ми вреди.", // Bulgarian
        "მინას ვჭამ და არა მტკივა.", // Georgian
        "Կրնամ ապակի ուտել և ինծի անհանգիստ չըներ։", // Armenian
        "جام ييه بلورم بڭا ضررى طوقونمز", // Turkish (Ottoman)
        "আমি কাঁচ খেতে পারি, তাতে আমার কোনো ক্ষতি হয় না।", // Bengali
        "मी काच खाऊ शकतो, मला ते दुखत नाही.", // Marathi
        "मैं काँच खा सकता हूँ और मुझे उससे कोई चोट नहीं पहुंचती.", // Hindi
        "நான் கண்ணாடி சாப்பிடுவேன், அதனால் எனக்கு ஒரு கேடும் வராது.", // Tamil
        "ن می توانم بدونِ احساس درد شيشه بخورم", // Persian
        "أنا قادر على أكل الزجاج و هذا لا يؤلمني. ", // Arabic
        "אני יכול לאכול זכוכית וזה לא מזיק לי.", // Hebrew
        "Tôi có thể ăn thủy tinh mà không hại gì.", // Vietnamese
        "我能吞下玻璃而不伤身体。", // Chinese
        "我能吞下玻璃而不傷身體。", // Chinese (traditional)
        "私はガラスを食べられます。それは私を傷つけません。", // Japan
        "나는 유리를 먹을 수 있어요. 그래도 아프지 않아요", // Korean
        // Test also random 4 byte characters
        "𠜎𠜱𠝹𠱓𠱸𠲖𠳏𠳕𠴕𠵼𠵿𠸎𠸏𠹷𠺝𠺢",
        "\xF0\xA0\x9C\x8E \xF0\xA0\x9C\xB1 \xF0\xA0\x9D\xB9 \xF0\xA0\xB1\xB8", // 𠜎 𠜱 𠝹 𠱸
        "\xF0\xA0\x9C\x8E\xF0\xA0\x9C\xB1\xF0\xA0\x9D\xB9\xF0\xA0\xB1\xB8" // 𠜎𠜱𠝹𠱸
    };

    for (const std::string &str : str_list) {
        SCOPED_TRACE("String: " + str);
        // Prepare input buffer
        const size_t data_size = str.size();
        std::unique_ptr<uint8_t[]> data_ptr{new uint8_t[data_size]};

        // Store string to the buffer (without '\0')
        ASSERT_EQ(ipx_set_string(data_ptr.get(), data_size, str.c_str()), IPX_OK);

        // Try to read the value
        const size_t res_size = (4 * data_size) + 1; // Max. size based on the documentation
        std::unique_ptr<char[]> res_ptr{new char[res_size]};
        int ret_code = ipx_string2str(data_ptr.get(), data_size, res_ptr.get(), res_size);
        ASSERT_GE(ret_code, 0);
        EXPECT_EQ(strlen(res_ptr.get()), static_cast<size_t>(ret_code));

        // Compare
        EXPECT_EQ(str, res_ptr.get());
    }
}

/*
 * Test IPFIX UTF-8 string to string converter (escape sequences)
 */
TEST(ConverterToStrings, ipx_string2strEscapeChar)
{
    // First is input, second is expected output
    using byte_arr = std::array<unsigned char, 14>;
    using my_pair = std::pair<byte_arr, std::string>;

    std::vector<my_pair> pair_list = {
        // Only control C0 characters (beginning)
        my_pair({0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xA, 0xB, 0xC, 0xD},
            "\\x00\\x01\\x02\\x03\\x04\\x05\\x06\\a\\b\\t\\n\\v\\f\\r"),
        // Only control C0 characters (ending)
        my_pair({0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x7F},
            "\\x13\\x14\\x15\\x16\\x17\\x18\\x19\\x1A\\x1B\\x1C\\x1D\\x1E\\x1F\\x7F"),
        // C1 control characters
        my_pair({0x80, 0x81, 0x82, 0x83, 0x88, 0x90, 0x93, 0x95, 0x98, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F},
            "\\x80\\x81\\x82\\x83\\x88\\x90\\x93\\x95\\x98\\x9B\\x9C\\x9D\\x9E\\x9F"),
        // Combination of C0 characters and normal characters
        my_pair({'a', '\\', 0x1F, 0x20, ' ', 0x7F, '\t', 'd', '?', '_', '1', '[', '+', '.'},
            "a\\\\x1F  \\x7F\\td?_1[+."),
        // Null byte and other characters
        my_pair({'0', 'a', 'b', '\0', 'c', 'd', 'e', '\0', 'f', 'g', '\0', '9', '\0', '\0'},
            "0ab\\x00cde\\x00fg\\x009\\x00\\x00"),
        my_pair({'"', 'w', 'o', 'r', 'l', 'd', '\'', '\\', 'n', '\'', 'i', 'n', '\\', '"'},
            "\"world'\\n'in\\\""),

        // Boundaries
        // First possible sequence of a certain length (1 - 4 bytes length)
        my_pair({0x00, ' ', 0xC0, 0x80, ' ', 0xE0, 0x80, 0x80, ' ', 0xF0, 0x80, 0x80, 0x80, ' '},
            "\\x00 \xC0\x80 \xE0\x80\x80 \xF0\x80\x80\x80 "),
        // Last possible sequence of a certain length (1 - 4 bytes length)
        my_pair({0x7F, ' ', 0xDF, 0xBF, ' ', 0xEF, 0xBF, 0xBF, ' ', 0xF7, 0xBF, 0xBF, 0xBF, ' '},
            "\\x7F \xDF\xBF \xEF\xBF\xBF \xF7\xBF\xBF\xBF "),
    };

    for (const auto &pair : pair_list) {
        const byte_arr &arr_in = pair.first;
        const std::string &str_out = pair.second;
        SCOPED_TRACE("String: " + str_out);

        // Store to a field
        const size_t data_size = arr_in.size();
        std::unique_ptr<uint8_t[]> data_ptr{new uint8_t[data_size]};
        const char *input = reinterpret_cast<const char *>(arr_in.data());
        ASSERT_EQ(ipx_set_string(data_ptr.get(), data_size, input), IPX_OK);

        // Try to convert the value
        const size_t res_size = (4 * data_size) + 1; // Max. size based on the documentation
        std::unique_ptr<char[]> res_ptr{new char[res_size]};
        int ret_code = ipx_string2str(data_ptr.get(), data_size, res_ptr.get(), res_size);
        ASSERT_GE(ret_code, 0);
        EXPECT_EQ(strlen(res_ptr.get()), static_cast<size_t>(ret_code));

        EXPECT_STREQ(str_out.c_str(), res_ptr.get());
    }
}

/*
 * Test IPFIX UTF-8 string to string converter (escape sequences)
 */
TEST(ConverterToStrings, ipx_string2strInvalidChar)
{
    struct test_data {
        const char  *buffer_data;
        const size_t buffer_size;
        const char  *expectation;
    };

    #define MY_INPUT(str) str, (sizeof(str) - 1)
    std::vector<struct test_data> input_list = {
        /* First and last continuation byte 0x80 (1000 0000), 0xBF (1011 1111) i.e. missing leading
         * byte that determines length. In this case 0x80 is C1 control character -> just escape  */
        {MY_INPUT("\x80"), "\\x80"},
        {MY_INPUT("\xBF"), "\uFFFD"},
        // 1 - 3 random continuation bytes (10xx xxxx) without the leading bytes
        {MY_INPUT("\xAA"), "\uFFFD"},
        {MY_INPUT("\xBB\xBB"), "\uFFFD\uFFFD"},
        {MY_INPUT("\xB1\xB2\xB3"), "\uFFFD\uFFFD\uFFFD"},
        // Random lonely start characters (110x xxxx..., 1110 xxxx..., 1111 0xxx...)
        {MY_INPUT("\xC0"), "\uFFFD"},
        {MY_INPUT("\xC5"), "\uFFFD"},
        {MY_INPUT("\xE0"), "\uFFFD"},
        {MY_INPUT("\xE5"), "\uFFFD"},
        {MY_INPUT("\xF0"), "\uFFFD"},
        {MY_INPUT("\xF5"), "\uFFFD"},
        // Sequences with last continuation byte missing
        {MY_INPUT("\xC0"), "\uFFFD"}, // 2 bytes
        {MY_INPUT("\xDF"), "\uFFFD"},
        {MY_INPUT("\xE0\x80"), "\uFFFD\\x80"}, // 3 bytes (0x80 is C1 control character)
        {MY_INPUT("\xEF\xBF"), "\uFFFD\uFFFD"},
        {MY_INPUT("\xF0\x80\x80"), "\uFFFD\\x80\\x80"}, // 4 bytes
        {MY_INPUT("\xF7\xBF\xBF"), "\uFFFD\uFFFD\uFFFD"},
        // Impossible bytes (not valid in UTF8) - 0xF0 - 0xFF
        {MY_INPUT("\xF0"), "\uFFFD"},
        {MY_INPUT("\xF1"), "\uFFFD"},
        {MY_INPUT("\xF2"), "\uFFFD"},
        {MY_INPUT("\xF3"), "\uFFFD"},
        {MY_INPUT("\xF4"), "\uFFFD"},
        {MY_INPUT("\xF5"), "\uFFFD"},
        {MY_INPUT("\xF6"), "\uFFFD"},
        {MY_INPUT("\xF7"), "\uFFFD"},
        {MY_INPUT("\xF8"), "\uFFFD"},
        {MY_INPUT("\xF9"), "\uFFFD"},
        {MY_INPUT("\xFA"), "\uFFFD"},
        {MY_INPUT("\xFB"), "\uFFFD"},
        {MY_INPUT("\xFC"), "\uFFFD"},
        {MY_INPUT("\xFD"), "\uFFFD"},
        {MY_INPUT("\xFE"), "\uFFFD"},
        {MY_INPUT("\xFF"), "\uFFFD"},
        // Sequences with last invalid continuation byte
        /*

         // TODO: solve
        {MY_INPUT("\xC0\x00"), "\uFFFD\\x00"},
        {MY_INPUT("\xC0\xEF"), "\uFFFD\uFFFD"},
        {MY_INPUT("\xE0\x80\xC0"), "\uFFFD\uFFFD\uFFFD"},
        {MY_INPUT("\xEF\xBF\xCF"), "\uFFFD\uFFFD\uFFFD"},

         */

    };
    #undef MY_INPUT

    for (const auto &input_struct: input_list) {
        SCOPED_TRACE(std::string("Expected string: ") + input_struct.expectation);

        // Store to a field
        const size_t data_size = input_struct.buffer_size;
        std::unique_ptr<uint8_t[]> data_ptr{new uint8_t[data_size]};
        const char *input_data = reinterpret_cast<const char *>(input_struct.buffer_data);
        ASSERT_EQ(ipx_set_string(data_ptr.get(), data_size, input_data), IPX_OK);

        // Try to convert the value
        const size_t res_size = (4 * data_size) + 1; // Max. size based on the documentation
        std::unique_ptr<char[]> res_ptr{new char[res_size]};
        int ret_code = ipx_string2str(data_ptr.get(), data_size, res_ptr.get(), res_size);
        ASSERT_GE(ret_code, 0);
        EXPECT_EQ(strlen(res_ptr.get()), static_cast<size_t>(ret_code));

        EXPECT_STREQ(input_struct.expectation, res_ptr.get());
    }
}


/**
 * @}
 */

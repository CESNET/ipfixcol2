/**
 * \file tests/unit_tests/core/converters/datetime.cpp
 * \author Lukas Hutak <xhutak01@stud.fit.vutbr.cz>
 * \brief Converters tests
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
 * \defgroup ipx_converters_datetime_be_test Data conversion tests of big endian
 *   datetime functions
 *
 * \note In many cases, test functions use dynamically allocated variables,
 *   because it is useful for valgrind memory check (accessing an array out
 *   of bounds, etc.)
 * @{
 */

/*
 * WARNING:
 *   Test does NOT include use-cases when time wraps around data type because
 *   this behavior of converters is not defined yet.
 */

#include <gtest/gtest.h>
#include <string>

extern "C" {
	#include <ipfixcol2/converters.h>
}

#define BYTES_4 (4U)
#define BYTES_8 (8U)

int main(int argc, char **argv)
{
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}

/**
 * \brief Test fixture for Date and time tests
 */
class ConverterDateTime : public ::testing::Test {
protected:
	static const uint64_t EPOCH_DIFF = 2208988800UL; // Unix epoch - NTP epoch [in seconds]
	static const uint32_t USEC_MASK = 0xFFFFF800; // To set last 11 bits of a fraction to zeros.

	static const size_t mem_size = 16;
	const uint8_t mem_const[mem_size] =
			{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
	uint8_t mem[mem_size];

	// Auxiliary test functions (defined together with appropriate test cases)
	uint32_t
	fraction2nanosec(uint32_t frac);
	uint32_t
	nanosec2fraction(uint32_t nsec);

	void
	SetInvalidSizeLP_test(enum fds_iemgr_element_type type, size_t except);
	void
	SetInvalidSizeHP_test(enum fds_iemgr_element_type type, size_t except);

	void
	GetInvalidSizeLP_test(enum fds_iemgr_element_type type, size_t except);
	void
	GetInvalidSizeHP_test(enum fds_iemgr_element_type type, size_t except);

	void
	DatetimeSetTestLowPrecision(uint64_t in_sec, uint64_t in_nsec);
	void
	DatetimeSetTestHighPrecision(uint64_t in_sec, uint64_t in_nsec);

	void
	DatetimeGetTestLowPrecision(uint64_t in_sec, uint64_t in_nsec);
	void
	DatetimeGetTestHighPrecision(uint64_t in_sec, uint64_t in_nsec);

public:
	/** Create variables for tests */
	virtual void
	SetUp() {

	}

	/** Destroy variables for the tests */
	virtual void
	TearDown() {

	}

};

uint32_t
ConverterDateTime::fraction2nanosec(uint32_t frac)
{
	uint64_t res = frac;
	res *= 1000000000ULL; // 1e9
	res >>= 32;
	return res;
}
uint32_t
ConverterDateTime::nanosec2fraction(uint32_t nsec)
{
	uint64_t res = nsec;
	res <<= 32;
	res /= 1000000000ULL; // 1e9
	return res;
}

/*
 * Check that the function works only when the correct combinations of type and
 * memory size are used
 */
void
ConverterDateTime::SetInvalidSizeLP_test(enum fds_iemgr_element_type type, size_t except)
{
	const uint64_t timestamp = 1499668301123ULL;
	for (size_t i = 0; i < mem_size; ++i) {
		SCOPED_TRACE("Memory size: " + std::to_string(i) + " byte(s).");
		if (i == except) {
			// Correct value
			continue;
		}

		memcpy(mem, mem_const, mem_size);
		EXPECT_EQ(ipx_set_datetime_lp_be(mem, i, type, timestamp), IPX_ERR_ARG);
		EXPECT_EQ(memcmp(mem, mem_const, mem_size), 0);
	}
}

void
ConverterDateTime::SetInvalidSizeHP_test(enum fds_iemgr_element_type type, size_t except)
{
	const struct timespec timestamp = {1499668301123ULL, 123456789U};
	for (size_t i = 0; i < mem_size; ++i) {
		SCOPED_TRACE("Memory size: " + std::to_string(i) + " byte(s).");
		if (i == except) {
			// Correct value
			continue;
		}

		memcpy(mem, mem_const, mem_size);
		EXPECT_EQ(ipx_set_datetime_hp_be(mem, i, type, timestamp), IPX_ERR_ARG);
		EXPECT_EQ(memcmp(mem, mem_const, mem_size), 0);
	}
}

TEST_F(ConverterDateTime, SetInvalidSizeLowPrecision)
{
	// Low precision API
	{
		SCOPED_TRACE("Element type: seconds");
		SetInvalidSizeLP_test(FDS_ET_DATE_TIME_SECONDS,	BYTES_4);
	}
	{
		SCOPED_TRACE("Element type: milliseconds");
		SetInvalidSizeLP_test(FDS_ET_DATE_TIME_MILLISECONDS, BYTES_8);
	}
	{
		SCOPED_TRACE("Element type: microseconds");
		SetInvalidSizeLP_test(FDS_ET_DATE_TIME_MICROSECONDS, BYTES_8);
	}
	{
		SCOPED_TRACE("Element type: nanoseconds");
		SetInvalidSizeLP_test(FDS_ET_DATE_TIME_NANOSECONDS, BYTES_8);
	}
}

TEST_F(ConverterDateTime, SetInvalidSizeHighPrecision)
{
	// High precision API
	{
		SCOPED_TRACE("Element type: seconds");
		SetInvalidSizeHP_test(FDS_ET_DATE_TIME_SECONDS, BYTES_4);
	}
	{
		SCOPED_TRACE("Element type: milliseconds");
		SetInvalidSizeHP_test(FDS_ET_DATE_TIME_MILLISECONDS, BYTES_8);
	}
	{
		SCOPED_TRACE("Element type: microseconds");
		SetInvalidSizeHP_test(FDS_ET_DATE_TIME_MICROSECONDS, BYTES_8);
	}
	{
		SCOPED_TRACE("Element type: nanoseconds");
		SetInvalidSizeHP_test(FDS_ET_DATE_TIME_NANOSECONDS, BYTES_8);
	}
}

/*
 * Check min. and max. valid timestamps for data each type
 */
TEST_F(ConverterDateTime, SetMinMaxLowPrecision)
{
	// NOTE: All constant values below are in milliseconds unless otherwise stated.
	// Seconds: 32 bit unsigned integer since 1.1.1970
	const uint64_t sec_min = 0; // 1 January 1970 00:00 (UTC)
	const uint64_t sec_max = UINT32_MAX * 1000U; // 7 February 2106 6:28:15 (UTC)

	// Milliseconds: 64 bit unsigned integer since 1.1.1970
	const uint64_t msec_min = 0; // 1 January 1970 00:00 (UTC)
	const uint64_t msec_max = std::numeric_limits<uint64_t>::max();

	// Microseconds: 64bit NTP timestamp since 1.1.1900
	const uint64_t ntp_era_end_as_unix = 2085978495UL *1000U; // 7 February 2036 6:28:15
	const uint64_t unix_epoch_as_ntp = EPOCH_DIFF * 1000U;
	const uint64_t usec_min = 0; // API uses Unix timestamp i.e. 1.1.1970
	const uint64_t usec_max = ntp_era_end_as_unix + 999;

	// Nanoseconds: 64bit NTP timestamp since 1.1.1900
	const uint64_t nsec_min = 0; // API uses Unix timestamp i.e. 1.1.1970
	const uint64_t nsec_max = ntp_era_end_as_unix + 999;

	enum fds_iemgr_element_type type;
	uint32_t fraction;

	// Seconds
	type = FDS_ET_DATE_TIME_SECONDS;
	EXPECT_EQ(ipx_set_datetime_lp_be(mem, BYTES_4, type, sec_min), IPX_OK);
	EXPECT_EQ(*(uint32_t *) mem, htonl(sec_min / 1000U));
	EXPECT_EQ(ipx_set_datetime_lp_be(mem, BYTES_4, type, sec_max), IPX_OK);
	EXPECT_EQ(*(uint32_t *) mem, htonl(sec_max / 1000U));

	// Milliseconds
	type = FDS_ET_DATE_TIME_MILLISECONDS;
	EXPECT_EQ(ipx_set_datetime_lp_be(mem, BYTES_8, type, msec_min), IPX_OK);
	EXPECT_EQ(*(uint64_t *) mem, htobe64(msec_min));
	EXPECT_EQ(ipx_set_datetime_lp_be(mem, BYTES_8, type, msec_max), IPX_OK);
	EXPECT_EQ(*(uint64_t *) mem, htobe64(msec_max));

	// Microseconds
	type = FDS_ET_DATE_TIME_MICROSECONDS;
	EXPECT_EQ(ipx_set_datetime_lp_be(mem, BYTES_8, type, usec_min), IPX_OK);
	EXPECT_EQ(*(uint32_t *) &mem[0], htonl(unix_epoch_as_ntp / 1000U)); // Seconds
	EXPECT_EQ(*(uint32_t *) &mem[4], 0U); // Fraction

	EXPECT_EQ(ipx_set_datetime_lp_be(mem, BYTES_8, type, usec_max), IPX_OK);
	EXPECT_EQ(*(uint32_t *) &mem[0], htonl(UINT32_MAX)); // Seconds
	fraction = nanosec2fraction(999000000U);
	fraction &= 0xFFFFF800;
	EXPECT_EQ(*(uint32_t *) &mem[4], htonl(fraction)); // Fraction

	// Nanoseconds
	type = FDS_ET_DATE_TIME_NANOSECONDS;
	EXPECT_EQ(ipx_set_datetime_lp_be(mem, BYTES_8, type, nsec_min), IPX_OK);
	EXPECT_EQ(*(uint32_t *) &mem[0], htonl(unix_epoch_as_ntp / 1000U)); // Seconds
	EXPECT_EQ(*(uint32_t *) &mem[4], 0U); // Fraction

	EXPECT_EQ(ipx_set_datetime_lp_be(mem, BYTES_8, type, nsec_max), IPX_OK);
	EXPECT_EQ(*(uint32_t *) &mem[0], htonl(UINT32_MAX)); // Seconds
	fraction = nanosec2fraction(999000000U);
	EXPECT_EQ(*(uint32_t *) &mem[4], htonl(fraction)); // Fraction
}

/*
 * Check min. and max. valid timestamps for data each type
 */
TEST_F(ConverterDateTime, SetMinMaxHighPrecision)
{
	const long nano_max = 999999999L;

	// Seconds: 32 bit unsigned integer since 1.1.1970
	const timespec sec_min = {0, 0}; // 1 January 1970 00:00 (UTC)
	const timespec sec_max = {UINT32_MAX, nano_max};

	// Milliseconds: 64 bit unsigned integer since 1.1.1970
	const timespec msec_min = {0, 0}; // 1 January 1970 00:00 (UTC)
	const timespec msec_max = {(std::numeric_limits<uint64_t>::max() / 1000) - 1, nano_max};

	// Microseconds: 64bit NTP timestamp since 1.1.1900
	const uint64_t ntp_era_end_as_unix = 2085978495UL; // 7 February 2036 6:28:15
	const uint64_t unix_epoch_as_ntp = EPOCH_DIFF;
	const timespec usec_min = {0, 0}; // API uses Unix timestamp i.e. 1.1.1970
	const timespec usec_max = {ntp_era_end_as_unix, nano_max};

	// Nanoseconds: 64bit NTP timestamp since 1.1.1900
	const timespec nsec_min = {0, 0}; // API uses Unix timestamp i.e. 1.1.1970
	const timespec nsec_max = {ntp_era_end_as_unix, nano_max};

	enum fds_iemgr_element_type type;
	uint32_t fraction;

	// Seconds
	type = FDS_ET_DATE_TIME_SECONDS;
	EXPECT_EQ(ipx_set_datetime_hp_be(mem, BYTES_4, type, sec_min), IPX_OK);
	EXPECT_EQ(*(uint32_t *) mem, htonl(sec_min.tv_sec));
	EXPECT_EQ(ipx_set_datetime_hp_be(mem, BYTES_4, type, sec_max), IPX_OK);
	EXPECT_EQ(*(uint32_t *) mem, htonl(sec_max.tv_sec));

	// Milliseconds
	uint64_t result;
	type = FDS_ET_DATE_TIME_MILLISECONDS;
	EXPECT_EQ(ipx_set_datetime_hp_be(mem, BYTES_8, type, msec_min), IPX_OK);
	result = (msec_min.tv_sec * 1000U) + (msec_min.tv_nsec / 1000000U);
	EXPECT_EQ(*(uint64_t *) mem, htobe64(result));
	EXPECT_EQ(ipx_set_datetime_hp_be(mem, BYTES_8, type, msec_max), IPX_OK);
	result = (msec_max.tv_sec * 1000U) + (msec_max.tv_nsec / 1000000U);
	EXPECT_EQ(*(uint64_t *) mem, htobe64(result));

	// Microseconds
	type = FDS_ET_DATE_TIME_MICROSECONDS;
	EXPECT_EQ(ipx_set_datetime_hp_be(mem, BYTES_8, type, usec_min), IPX_OK);
	EXPECT_EQ(*(uint32_t *) &mem[0], htonl(unix_epoch_as_ntp)); // Seconds
	EXPECT_EQ(*(uint32_t *) &mem[4], 0U); // Fraction

	EXPECT_EQ(ipx_set_datetime_hp_be(mem, BYTES_8, type, usec_max), IPX_OK);
	EXPECT_EQ(*(uint32_t *) &mem[0], htonl(UINT32_MAX)); // Seconds
	fraction = nanosec2fraction(usec_max.tv_nsec);
	fraction &= USEC_MASK;
	EXPECT_EQ(*(uint32_t *) &mem[4], htonl(fraction)); // Fraction

	// Nanoseconds
	type = FDS_ET_DATE_TIME_NANOSECONDS;
	EXPECT_EQ(ipx_set_datetime_hp_be(mem, BYTES_8, type, nsec_min), IPX_OK);
	EXPECT_EQ(*(uint32_t *) &mem[0], htonl(unix_epoch_as_ntp)); // Seconds
	EXPECT_EQ(*(uint32_t *) &mem[4], 0U); // Fraction

	EXPECT_EQ(ipx_set_datetime_hp_be(mem, BYTES_8, type, nsec_max), IPX_OK);
	EXPECT_EQ(*(uint32_t *) &mem[0], htonl(UINT32_MAX)); // Seconds
	fraction = nanosec2fraction(nsec_max.tv_nsec);
	EXPECT_EQ(*(uint32_t *) &mem[4], htonl(fraction)); // Fraction
}

/*
 * Invalid data types
 */
TEST_F(ConverterDateTime, SetInvalidDataTypeLowPrecision)
{
	for (int i = 0; i < FDS_ET_UNASSIGNED; ++i) {
		SCOPED_TRACE("Type ID: " + std::to_string(i));
		switch (i) {
		case FDS_ET_DATE_TIME_SECONDS:
		case FDS_ET_DATE_TIME_MILLISECONDS:
		case FDS_ET_DATE_TIME_MICROSECONDS:
		case FDS_ET_DATE_TIME_NANOSECONDS:
			// Skip valid types
			continue;
		default:
			// Try all invalid types
			break;
		}

		enum fds_iemgr_element_type type = static_cast<enum fds_iemgr_element_type>(i);

		// Check that the return code is correct and the memory is not changed
		memcpy(mem, mem_const, mem_size);
		EXPECT_EQ(ipx_set_datetime_lp_be(mem, BYTES_4, type, 0), IPX_ERR_ARG);
		EXPECT_EQ(memcmp(mem, mem_const, mem_size), 0);
		memcpy(mem, mem_const, mem_size);
		EXPECT_EQ(ipx_set_datetime_lp_be(mem, BYTES_8, type, 0), IPX_ERR_ARG);
		EXPECT_EQ(memcmp(mem, mem_const, mem_size), 0);

		// Check also that memory is not used at all - can cause SEGFAULT
		EXPECT_EQ(ipx_set_datetime_lp_be(NULL, BYTES_4, type, 0), IPX_ERR_ARG);
		EXPECT_EQ(ipx_set_datetime_lp_be(NULL, BYTES_8, type, 0), IPX_ERR_ARG);
	}
}

TEST_F(ConverterDateTime, SetInvalidDataTypeHighPrecision)
{
	const struct timespec ts = {0, 0};

	for (int i = 0; i < FDS_ET_UNASSIGNED; ++i) {
		SCOPED_TRACE("Type ID: " + std::to_string(i));
		switch (i) {
		case FDS_ET_DATE_TIME_SECONDS:
		case FDS_ET_DATE_TIME_MILLISECONDS:
		case FDS_ET_DATE_TIME_MICROSECONDS:
		case FDS_ET_DATE_TIME_NANOSECONDS:
			// Skip valid types
			continue;
		default:
			// Try all invalid types
			break;
		}

		enum fds_iemgr_element_type type = static_cast<enum fds_iemgr_element_type>(i);

		// Check that the return code is correct and the memory is not changed
		memcpy(mem, mem_const, mem_size);
		EXPECT_EQ(ipx_set_datetime_hp_be(mem, BYTES_4, type, ts), IPX_ERR_ARG);
		EXPECT_EQ(memcmp(mem, mem_const, mem_size), 0);
		memcpy(mem, mem_const, mem_size);
		EXPECT_EQ(ipx_set_datetime_hp_be(mem, BYTES_8, type, ts), IPX_ERR_ARG);
		EXPECT_EQ(memcmp(mem, mem_const, mem_size), 0);

		// Check also that memory is not used at all - can cause SEGFAULT
		EXPECT_EQ(ipx_set_datetime_hp_be(NULL, BYTES_4, type, ts), IPX_ERR_ARG);
		EXPECT_EQ(ipx_set_datetime_hp_be(NULL, BYTES_8, type, ts), IPX_ERR_ARG);
	}
}

/*
 * Test "random" dates in the current era
 */
void
ConverterDateTime::DatetimeSetTestLowPrecision(uint64_t in_sec, uint64_t in_nsec)
{
	// Calculate inputs
	const uint64_t input_lp = (in_sec * 1000) + (in_nsec / 1000000);   // [ms]
	// Low level API can handle only milliseconds precision
	in_nsec /= 1000000;
	in_nsec *= 1000000;

	// Calculate expected results of low precision API
	const uint32_t res_sec = htonl(in_sec);
	const uint64_t res_msec = htobe64(input_lp);
	const uint32_t aux_frac = nanosec2fraction(in_nsec);
	const uint32_t res_usec[2] = {htonl(in_sec + EPOCH_DIFF), htonl(aux_frac & USEC_MASK)};
	const uint32_t res_nsec[2] = {htonl(in_sec + EPOCH_DIFF), htonl(aux_frac)};

	// Test seconds, milliseconds, etc.
	enum fds_iemgr_element_type type;
	type = FDS_ET_DATE_TIME_SECONDS;
	EXPECT_EQ(ipx_set_datetime_lp_be(mem, BYTES_4, type, input_lp), IPX_OK);
	EXPECT_EQ(*(uint32_t *) mem, res_sec);

	type = FDS_ET_DATE_TIME_MILLISECONDS;
	EXPECT_EQ(ipx_set_datetime_lp_be(mem, BYTES_8, type, input_lp), IPX_OK);
	EXPECT_EQ(*(uint64_t *) mem, res_msec);

	type = FDS_ET_DATE_TIME_MICROSECONDS;
	EXPECT_EQ(ipx_set_datetime_lp_be(mem, BYTES_8, type, input_lp), IPX_OK);
	EXPECT_EQ(*(uint64_t *) mem, *(uint64_t *) res_usec);

	type = FDS_ET_DATE_TIME_NANOSECONDS;
	EXPECT_EQ(ipx_set_datetime_lp_be(mem, BYTES_8, type, input_lp), IPX_OK);
	EXPECT_EQ(*(uint64_t *) mem, *(uint64_t *) res_nsec);
}

void
ConverterDateTime::DatetimeSetTestHighPrecision(uint64_t in_sec, uint64_t in_nsec)
{
	// Calculate inputs
	const struct timespec input_hp = {
			static_cast<time_t>(in_sec),
			static_cast<long>(in_nsec)
	}; // [s, ns]

	// Calculate expected results of low precision API
	const uint32_t res_sec = htonl(in_sec);
	const uint64_t res_msec = htobe64((in_sec * 1000) + (in_nsec / 1000000));
	const uint32_t aux_frac = nanosec2fraction(in_nsec);
	const uint32_t res_usec[2] = {htonl(in_sec + EPOCH_DIFF), htonl(aux_frac & USEC_MASK)};
	const uint32_t res_nsec[2] = {htonl(in_sec + EPOCH_DIFF), htonl(aux_frac)};

	// Test seconds, milliseconds, etc.
	enum fds_iemgr_element_type type;
	type = FDS_ET_DATE_TIME_SECONDS;
	EXPECT_EQ(ipx_set_datetime_hp_be(mem, BYTES_4, type, input_hp), IPX_OK);
	EXPECT_EQ(*(uint32_t *) mem, res_sec);

	type = FDS_ET_DATE_TIME_MILLISECONDS;
	EXPECT_EQ(ipx_set_datetime_hp_be(mem, BYTES_8, type, input_hp), IPX_OK);
	EXPECT_EQ(*(uint64_t *) mem, res_msec);

	type = FDS_ET_DATE_TIME_MICROSECONDS;
	EXPECT_EQ(ipx_set_datetime_hp_be(mem, BYTES_8, type, input_hp), IPX_OK);
	EXPECT_EQ(*(uint64_t *) mem, *(uint64_t *) res_usec);

	type = FDS_ET_DATE_TIME_NANOSECONDS;
	EXPECT_EQ(ipx_set_datetime_hp_be(mem, BYTES_8, type, input_hp), IPX_OK);
	EXPECT_EQ(*(uint64_t *) mem, *(uint64_t *) res_nsec);
}

TEST_F(ConverterDateTime, SetRandomValue)
{
	{
		SCOPED_TRACE("Timestamp test: \"29 Nov 1973 21:33:09.987654321 (UTC)\"");
		const uint64_t sec = 123456789;
		const uint64_t nsec = 987654321;
		DatetimeSetTestLowPrecision(sec, nsec);
		DatetimeSetTestHighPrecision(sec, nsec);
	}
	{
		SCOPED_TRACE("Timestamp test: \"11 Jul 2017 11:59:23.123456789 (UTC)\"");
		const uint64_t sec = 1499774363;
		const uint64_t nsec = 123456789;
		DatetimeSetTestLowPrecision(sec, nsec);
		DatetimeSetTestHighPrecision(sec, nsec);
	}
	{
		SCOPED_TRACE("Timestamp test: \"14 Dec 2035 20:04:24.280048921 (UTC)\"");
		const uint64_t sec = 2081275464;
		const uint64_t nsec = 280048921;
		DatetimeSetTestLowPrecision(sec, nsec);
		DatetimeSetTestHighPrecision(sec, nsec);
	}
}

/*
 * Check that the function works only when the correct combinations of type and
 * memory size are used
 */
void
ConverterDateTime::GetInvalidSizeLP_test(enum fds_iemgr_element_type type, size_t except)
{
	const uint64_t timestamp_const = 9876543210ULL;
	uint64_t timestamp_out;

	for (size_t i = 0; i < mem_size; ++i) {
		SCOPED_TRACE("Memory size: " + std::to_string(i) + " byte(s).");
		if (i == except) {
			// Correct value
			continue;
		}

		timestamp_out = timestamp_const;
		EXPECT_EQ(ipx_get_datetime_lp_be(mem_const, i, type, &timestamp_out), IPX_ERR_ARG);
		EXPECT_EQ(timestamp_out, timestamp_const);

		// Should not touch the memory, therefore the NULL pointer could cause SEGFAULT
		timestamp_out = timestamp_const;
		EXPECT_EQ(ipx_get_datetime_lp_be(NULL, i, type, &timestamp_out), IPX_ERR_ARG);
		EXPECT_EQ(timestamp_out, timestamp_const);
	}
}

void
ConverterDateTime::GetInvalidSizeHP_test(enum fds_iemgr_element_type type, size_t except)
{
	const struct timespec timestamp_const = {1499668301123ULL, 123456789U};
	struct timespec timestamp_out;

	for (size_t i = 0; i < mem_size; ++i) {
		SCOPED_TRACE("Memory size: " + std::to_string(i) + " byte(s).");
		if (i == except) {
			// Correct value
			continue;
		}

		timestamp_out = timestamp_const;
		EXPECT_EQ(ipx_get_datetime_hp_be(mem_const, i, type, &timestamp_out), IPX_ERR_ARG);
		EXPECT_EQ(timestamp_out.tv_sec, timestamp_const.tv_sec);
		EXPECT_EQ(timestamp_out.tv_nsec, timestamp_const.tv_nsec);

		// Should not touch the memory, therefore the NULL pointer could cause SEGFAULT
		timestamp_out = timestamp_const;
		EXPECT_EQ(ipx_get_datetime_hp_be(NULL, i, type, &timestamp_out), IPX_ERR_ARG);
		EXPECT_EQ(timestamp_out.tv_sec, timestamp_const.tv_sec);
		EXPECT_EQ(timestamp_out.tv_nsec, timestamp_const.tv_nsec);
	}
}

TEST_F(ConverterDateTime, GetInvalidSizeLowPrecision)
{
	// Low precision API
	{
		SCOPED_TRACE("Element type: seconds");
		GetInvalidSizeLP_test(FDS_ET_DATE_TIME_SECONDS,	BYTES_4);
	}
	{
		SCOPED_TRACE("Element type: milliseconds");
		GetInvalidSizeLP_test(FDS_ET_DATE_TIME_MILLISECONDS, BYTES_8);
	}
	{
		SCOPED_TRACE("Element type: microseconds");
		GetInvalidSizeLP_test(FDS_ET_DATE_TIME_MICROSECONDS, BYTES_8);
	}
	{
		SCOPED_TRACE("Element type: nanoseconds");
		GetInvalidSizeLP_test(FDS_ET_DATE_TIME_NANOSECONDS, BYTES_8);
	}
}

TEST_F(ConverterDateTime, GetInvalidSizeHighPrecision)
{
	// High precision API
	{
		SCOPED_TRACE("Element type: seconds");
		GetInvalidSizeHP_test(FDS_ET_DATE_TIME_SECONDS, BYTES_4);
	}
	{
		SCOPED_TRACE("Element type: milliseconds");
		GetInvalidSizeHP_test(FDS_ET_DATE_TIME_MILLISECONDS, BYTES_8);
	}
	{
		SCOPED_TRACE("Element type: microseconds");
		GetInvalidSizeHP_test(FDS_ET_DATE_TIME_MICROSECONDS, BYTES_8);
	}
	{
		SCOPED_TRACE("Element type: nanoseconds");
		GetInvalidSizeHP_test(FDS_ET_DATE_TIME_NANOSECONDS, BYTES_8);
	}
}

/*
 * Get the minimum and maximum value that can be stored into each data type
 */
TEST_F(ConverterDateTime, GetMinMaxLowPrecision)
{
	// NOTE: All constant values below are in milliseconds unless otherwise stated.
	// Seconds: 32 bit unsigned integer since 1.1.1970
	const uint64_t sec_min = 0; // 1 January 1970 00:00 (UTC)
	// 7 February 2106 6:28:15 (UTC)
	const uint64_t sec_max = static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()) * 1000U;

	// Milliseconds: 64 bit unsigned integer since 1.1.1970
	const uint64_t msec_min = 0; // 1 January 1970 00:00 (UTC)
	const uint64_t msec_max = std::numeric_limits<uint64_t>::max();

	// Microseconds: 64bit NTP timestamp since 1.1.1900
	const uint64_t ntp_era_end_as_unix = 2085978495UL * 1000U; // 7 February 2036 6:28:15
	const uint64_t usec_min = 0; // API uses Unix timestamp i.e. 1.1.1970
	const uint64_t usec_max = ntp_era_end_as_unix + 999;

	// Nanoseconds: 64bit NTP timestamp since 1.1.1900
	const uint64_t nsec_min = 0; // API uses Unix timestamp i.e. 1.1.1970
	const uint64_t nsec_max = ntp_era_end_as_unix + 999;

	enum fds_iemgr_element_type type;
	uint64_t result;

	// Seconds
	type = FDS_ET_DATE_TIME_SECONDS;
	EXPECT_EQ(ipx_set_datetime_lp_be(mem, BYTES_4, type, sec_min), IPX_OK);
	EXPECT_EQ(ipx_get_datetime_lp_be(mem, BYTES_4, type, &result), IPX_OK);
	EXPECT_EQ(result, sec_min);
	EXPECT_EQ(ipx_set_datetime_lp_be(mem, BYTES_4, type, sec_max), IPX_OK);
	EXPECT_EQ(ipx_get_datetime_lp_be(mem, BYTES_4, type, &result), IPX_OK);
	// Milliseconds are lost!
	EXPECT_EQ(result, (sec_max / 1000) * 1000);

	// Milliseconds
	type = FDS_ET_DATE_TIME_MILLISECONDS;
	EXPECT_EQ(ipx_set_datetime_lp_be(mem, BYTES_8, type, msec_min), IPX_OK);
	EXPECT_EQ(ipx_get_datetime_lp_be(mem, BYTES_8, type, &result), IPX_OK);
	EXPECT_EQ(result, msec_min);
	EXPECT_EQ(ipx_set_datetime_lp_be(mem, BYTES_8, type, msec_max), IPX_OK);
	EXPECT_EQ(ipx_get_datetime_lp_be(mem, BYTES_8, type, &result), IPX_OK);
	EXPECT_EQ(result, msec_max);

	// Microseconds
	type = FDS_ET_DATE_TIME_MICROSECONDS;
	EXPECT_EQ(ipx_set_datetime_lp_be(mem, BYTES_8, type, usec_min), IPX_OK);
	EXPECT_EQ(ipx_get_datetime_lp_be(mem, BYTES_8, type, &result), IPX_OK);
	EXPECT_EQ(result, usec_min);
	EXPECT_EQ(ipx_set_datetime_lp_be(mem, BYTES_8, type, usec_max), IPX_OK);
	EXPECT_EQ(ipx_get_datetime_lp_be(mem, BYTES_8, type, &result), IPX_OK);
	// Conversion can cause rounding - therefore we use the acceptable error bound
	EXPECT_NEAR(result, usec_max, 1);

	// Nanoseconds
	type = FDS_ET_DATE_TIME_NANOSECONDS;
	EXPECT_EQ(ipx_set_datetime_lp_be(mem, BYTES_8, type, nsec_min), IPX_OK);
	EXPECT_EQ(ipx_get_datetime_lp_be(mem, BYTES_8, type, &result), IPX_OK);
	EXPECT_EQ(result, nsec_min);
	EXPECT_EQ(ipx_set_datetime_lp_be(mem, BYTES_8, type, nsec_max), IPX_OK);
	EXPECT_EQ(ipx_get_datetime_lp_be(mem, BYTES_8, type, &result), IPX_OK);
	// Conversion can cause rounding - therefore we use the acceptable error bound
	EXPECT_NEAR(result, nsec_max, 1);
}

TEST_F(ConverterDateTime, GetMinMaxHighPrecision)
{
	const long nano_max = 999999999L;

	// Seconds: 32 bit unsigned integer since 1.1.1970
	const timespec sec_min = {0, 0}; // 1 January 1970 00:00 (UTC)
	const timespec sec_max = {UINT32_MAX, nano_max};

	// Milliseconds: 64 bit unsigned integer since 1.1.1970
	const timespec msec_min = {0, 0}; // 1 January 1970 00:00 (UTC)
	const timespec msec_max = {(std::numeric_limits<uint64_t>::max() / 1000) - 1, nano_max};

	// Microseconds: 64bit NTP timestamp since 1.1.1900
	const uint64_t ntp_era_end_as_unix = 2085978495UL; // 7 February 2036 6:28:15
	const timespec usec_min = {0, 0}; // API uses Unix timestamp i.e. 1.1.1970
	const timespec usec_max = {ntp_era_end_as_unix, nano_max};

	// Nanoseconds: 64bit NTP timestamp since 1.1.1900
	const timespec nsec_min = {0, 0}; // API uses Unix timestamp i.e. 1.1.1970
	const timespec nsec_max = {ntp_era_end_as_unix, nano_max};

	enum fds_iemgr_element_type type;
	struct timespec result;

	// Seconds
	type = FDS_ET_DATE_TIME_SECONDS;
	EXPECT_EQ(ipx_set_datetime_hp_be(mem, BYTES_4, type, sec_min), IPX_OK);
	EXPECT_EQ(ipx_get_datetime_hp_be(mem, BYTES_4, type, &result), IPX_OK);
	EXPECT_EQ(result.tv_sec, sec_min.tv_sec);
	EXPECT_EQ(result.tv_nsec, sec_min.tv_nsec);
	EXPECT_EQ(ipx_set_datetime_hp_be(mem, BYTES_4, type, sec_max), IPX_OK);
	EXPECT_EQ(ipx_get_datetime_hp_be(mem, BYTES_4, type, &result), IPX_OK);
	EXPECT_EQ(result.tv_sec, sec_max.tv_sec);
	EXPECT_EQ(result.tv_nsec, 0); // Fraction is lost!

	// Milliseconds
	type = FDS_ET_DATE_TIME_MILLISECONDS;
	EXPECT_EQ(ipx_set_datetime_hp_be(mem, BYTES_8, type, msec_min), IPX_OK);
	EXPECT_EQ(ipx_get_datetime_hp_be(mem, BYTES_8, type, &result), IPX_OK);
	EXPECT_EQ(result.tv_sec, msec_min.tv_sec);
	EXPECT_EQ(result.tv_nsec, msec_min.tv_nsec);
	EXPECT_EQ(ipx_set_datetime_hp_be(mem, BYTES_8, type, msec_max), IPX_OK);
	EXPECT_EQ(ipx_get_datetime_hp_be(mem, BYTES_8, type, &result), IPX_OK);
	// Fraction is partly lost
	EXPECT_EQ(result.tv_sec, msec_max.tv_sec);
	EXPECT_NEAR(result.tv_nsec, msec_max.tv_nsec, 1000000);

	// Microseconds
	type = FDS_ET_DATE_TIME_MICROSECONDS;
	EXPECT_EQ(ipx_set_datetime_hp_be(mem, BYTES_8, type, usec_min), IPX_OK);
	EXPECT_EQ(ipx_get_datetime_hp_be(mem, BYTES_8, type, &result), IPX_OK);
	EXPECT_EQ(result.tv_sec, usec_min.tv_sec);
	EXPECT_EQ(result.tv_nsec, usec_min.tv_nsec);
	EXPECT_EQ(ipx_set_datetime_hp_be(mem, BYTES_8, type, usec_max), IPX_OK);
	EXPECT_EQ(ipx_get_datetime_hp_be(mem, BYTES_8, type, &result), IPX_OK);
	// Fraction is partly lost
	EXPECT_EQ(result.tv_sec, usec_max.tv_sec);
	EXPECT_NEAR(result.tv_nsec, usec_max.tv_nsec, 1000);

	// Nanoseconds
	type = FDS_ET_DATE_TIME_NANOSECONDS;
	EXPECT_EQ(ipx_set_datetime_hp_be(mem, BYTES_8, type, nsec_min), IPX_OK);
	EXPECT_EQ(ipx_get_datetime_hp_be(mem, BYTES_8, type, &result), IPX_OK);
	EXPECT_EQ(result.tv_sec, nsec_min.tv_sec);
	EXPECT_EQ(result.tv_nsec, nsec_min.tv_nsec);
	EXPECT_EQ(ipx_set_datetime_hp_be(mem, BYTES_8, type, nsec_max), IPX_OK);
	EXPECT_EQ(ipx_get_datetime_hp_be(mem, BYTES_8, type, &result), IPX_OK);
	// Conversion can cause rounding - therefore we use the acceptable error bound
	EXPECT_EQ(result.tv_sec, nsec_max.tv_sec);
	EXPECT_NEAR(result.tv_nsec, nsec_max.tv_nsec, 1);
}

/*
 * Get random dates from the current era
 */
void
ConverterDateTime::DatetimeGetTestLowPrecision(uint64_t in_sec, uint64_t in_nsec)
{
	// Calculate inputs
	uint64_t result;
	const uint64_t input_lp = (in_sec * 1000) + (in_nsec / 1000000);   // [ms]

	// Test seconds, milliseconds, etc.
	enum fds_iemgr_element_type type;
	type = FDS_ET_DATE_TIME_SECONDS;
	EXPECT_EQ(ipx_set_datetime_lp_be(mem, BYTES_4, type, input_lp), IPX_OK);
	EXPECT_EQ(ipx_get_datetime_lp_be(mem, BYTES_4, type, &result), IPX_OK);
	EXPECT_EQ(in_sec * 1000, result);

	type = FDS_ET_DATE_TIME_MILLISECONDS;
	EXPECT_EQ(ipx_set_datetime_lp_be(mem, BYTES_8, type, input_lp), IPX_OK);
	EXPECT_EQ(ipx_get_datetime_lp_be(mem, BYTES_8, type, &result), IPX_OK);
	EXPECT_NEAR(input_lp, result, 1);

	type = FDS_ET_DATE_TIME_MICROSECONDS;
	EXPECT_EQ(ipx_set_datetime_lp_be(mem, BYTES_8, type, input_lp), IPX_OK);
	EXPECT_EQ(ipx_get_datetime_lp_be(mem, BYTES_8, type, &result), IPX_OK);
	EXPECT_NEAR(input_lp, result, 1);

	type = FDS_ET_DATE_TIME_NANOSECONDS;
	EXPECT_EQ(ipx_set_datetime_lp_be(mem, BYTES_8, type, input_lp), IPX_OK);
	EXPECT_EQ(ipx_get_datetime_lp_be(mem, BYTES_8, type, &result), IPX_OK);
	EXPECT_NEAR(input_lp, result, 1);
}

void
ConverterDateTime::DatetimeGetTestHighPrecision(uint64_t in_sec, uint64_t in_nsec)
{
	// Calculate inputs
	struct timespec result;
	const struct timespec input_hp = {static_cast<time_t>(in_sec), static_cast<long>(in_nsec)};

	// Test seconds, milliseconds, etc.
	enum fds_iemgr_element_type type;
	type = FDS_ET_DATE_TIME_SECONDS;
	EXPECT_EQ(ipx_set_datetime_hp_be(mem, BYTES_4, type, input_hp), IPX_OK);
	EXPECT_EQ(ipx_get_datetime_hp_be(mem, BYTES_4, type, &result), IPX_OK);
	EXPECT_EQ(input_hp.tv_sec, result.tv_sec);
	EXPECT_EQ(input_hp.tv_nsec, 0); // Fraction is lost!

	type = FDS_ET_DATE_TIME_MILLISECONDS;
	EXPECT_EQ(ipx_set_datetime_hp_be(mem, BYTES_8, type, input_hp), IPX_OK);
	EXPECT_EQ(ipx_get_datetime_hp_be(mem, BYTES_8, type, &result), IPX_OK);
	EXPECT_EQ(input_hp.tv_sec, result.tv_sec);
	EXPECT_NEAR(input_hp.tv_nsec, result.tv_nsec, 1000000);

	type = FDS_ET_DATE_TIME_MICROSECONDS;
	EXPECT_EQ(ipx_set_datetime_hp_be(mem, BYTES_8, type, input_hp), IPX_OK);
	EXPECT_EQ(ipx_get_datetime_hp_be(mem, BYTES_8, type, &result), IPX_OK);
	EXPECT_EQ(input_hp.tv_sec, result.tv_sec);
	EXPECT_NEAR(input_hp.tv_nsec, result.tv_nsec, 1000);

	type = FDS_ET_DATE_TIME_NANOSECONDS;
	EXPECT_EQ(ipx_set_datetime_hp_be(mem, BYTES_8, type, input_hp), IPX_OK);
	EXPECT_EQ(ipx_get_datetime_hp_be(mem, BYTES_8, type, &result), IPX_OK);
	EXPECT_EQ(input_hp.tv_sec, result.tv_sec);
	EXPECT_NEAR(input_hp.tv_nsec, result.tv_nsec, 1);
}

TEST_F(ConverterDateTime, GetRandomValue)
{
	{
		SCOPED_TRACE("Timestamp test: \"29 Nov 1973 21:33:09.987654321 (UTC)\"");
		const uint64_t sec = 123456789;
		const uint64_t nsec = 987654321;
		DatetimeGetTestLowPrecision(sec, nsec);
		DatetimeSetTestHighPrecision(sec, nsec);
	}
	{
		SCOPED_TRACE("Timestamp test: \"11 Jul 2017 11:59:23.123456789 (UTC)\"");
		const uint64_t sec = 1499774363;
		const uint64_t nsec = 123456789;
		DatetimeGetTestLowPrecision(sec, nsec);
		DatetimeSetTestHighPrecision(sec, nsec);
	}
	{
		SCOPED_TRACE("Timestamp test: \"14 Dec 2035 20:04:24.280048921 (UTC)\"");
		const uint64_t sec = 2081275464;
		const uint64_t nsec = 280048921;
		DatetimeGetTestLowPrecision(sec, nsec);
		DatetimeSetTestHighPrecision(sec, nsec);
	}
}

/*
 * Invalid data types
 */
TEST_F(ConverterDateTime, GetInvalidDataTypeLowPrecision)
{
	for (int i = 0; i < FDS_ET_UNASSIGNED; ++i) {
		SCOPED_TRACE("Type ID: " + std::to_string(i));
		switch (i) {
		case FDS_ET_DATE_TIME_SECONDS:
		case FDS_ET_DATE_TIME_MILLISECONDS:
		case FDS_ET_DATE_TIME_MICROSECONDS:
		case FDS_ET_DATE_TIME_NANOSECONDS:
			// Skip valid types
			continue;
		default:
			// Try all invalid types
			break;
		}

		enum fds_iemgr_element_type type = static_cast<enum fds_iemgr_element_type>(i);
		uint64_t result;
		memcpy(mem, mem_const, mem_size);

		// Check that the return code is correct
		EXPECT_EQ(ipx_get_datetime_lp_be(mem, BYTES_4, type, &result), IPX_ERR_ARG);
		EXPECT_EQ(ipx_get_datetime_lp_be(mem, BYTES_8, type, &result), IPX_ERR_ARG);

		// Check also that memory is not used at all - can cause SEGFAULT
		EXPECT_EQ(ipx_get_datetime_lp_be(NULL, BYTES_4, type, &result), IPX_ERR_ARG);
		EXPECT_EQ(ipx_get_datetime_lp_be(NULL, BYTES_8, type, &result), IPX_ERR_ARG);
	}
}

TEST_F(ConverterDateTime, GetInvalidDataTypeHighPrecision)
{
	for (int i = 0; i < FDS_ET_UNASSIGNED; ++i) {
		SCOPED_TRACE("Type ID: " + std::to_string(i));
		switch (i) {
		case FDS_ET_DATE_TIME_SECONDS:
		case FDS_ET_DATE_TIME_MILLISECONDS:
		case FDS_ET_DATE_TIME_MICROSECONDS:
		case FDS_ET_DATE_TIME_NANOSECONDS:
			// Skip valid types
			continue;
		default:
			// Try all invalid types
			break;
		}

		enum fds_iemgr_element_type type = static_cast<enum fds_iemgr_element_type>(i);
		struct timespec result;
		memcpy(mem, mem_const, mem_size);

		// Check that the return code is correct
		EXPECT_EQ(ipx_get_datetime_hp_be(mem, BYTES_4, type, &result), IPX_ERR_ARG);
		EXPECT_EQ(ipx_get_datetime_hp_be(mem, BYTES_8, type, &result), IPX_ERR_ARG);

		// Check also that memory is not used at all - can cause SEGFAULT
		EXPECT_EQ(ipx_get_datetime_hp_be(NULL, BYTES_4, type, &result), IPX_ERR_ARG);
		EXPECT_EQ(ipx_get_datetime_hp_be(NULL, BYTES_8, type, &result), IPX_ERR_ARG);
	}
}

/**
 * @}
 */

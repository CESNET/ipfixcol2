#include <gtest/gtest.h>

extern "C" {
	#include <ipfixcol2.h>
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

// Test setting maximum possible value for 1,2,4,8 bytes values
TEST(Convertors, setUintMax) {
	// SetUp
	const uint64_t max_val = UINT64_MAX;
	uint8_t  u8;
	uint16_t u16;
	uint32_t u32;
	uint64_t u64;
	int ret_val;

	// Tests
	EXPECT_EQ(ipx_set_uint(&u8,  sizeof(u8),  max_val), IPX_CONVERT_ERR_TRUNC);
	EXPECT_EQ(u8,  UINT8_MAX);

	EXPECT_EQ(ipx_set_uint(&u16, sizeof(u16), max_val), IPX_CONVERT_ERR_TRUNC);
	EXPECT_EQ(u16, UINT16_MAX);

	EXPECT_EQ(ipx_set_uint(&u32, sizeof(u32), max_val), IPX_CONVERT_ERR_TRUNC);
	EXPECT_EQ(u32, UINT32_MAX);

	EXPECT_EQ(ipx_set_uint(&u64, sizeof(u64), max_val), 0);
	EXPECT_EQ(u64, UINT64_MAX);
}


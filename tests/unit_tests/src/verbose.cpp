#include <gtest/gtest.h>

extern "C" {
	#include <ipfixcol2.h>
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

TEST(Verbosity, Print) {
	ipx_verbosity_set_level(IPX_VERBOSITY_INFO);
	enum IPX_VERBOSITY_LEVEL level = ipx_verbosity_get_level();
	EXPECT_EQ(level, IPX_VERBOSITY_INFO);
}

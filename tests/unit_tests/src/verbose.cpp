#include <gtest/gtest.h>

extern "C" {
	#include <ipfixcol2.h>
}

TEST(Verbosity, Print) {
	ipx_verbosity_set_level(IPX_VERBOSITY_INFO);
	enum IPX_VERBOSITY_LEVEL level = ipx_verbosity_get_level();
	EXPECT_EQ(level, IPX_VERBOSITY_INFO);
}

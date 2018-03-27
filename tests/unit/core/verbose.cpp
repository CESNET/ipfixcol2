#include <gtest/gtest.h>
#include <core/verbose.h>

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

TEST(Verbosity, global_set_and_get)
{
    ipx_verb_level_set(IPX_VERB_INFO);
    enum ipx_verb_level level = ipx_verb_level_get();
    EXPECT_EQ(level, IPX_VERB_INFO);
}

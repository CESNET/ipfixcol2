/**
 * \author Michal Režňák
 * \date   8.9.17
 */
#include <gtest/gtest.h>
#include <ipfixcol2.h>
#include <ipfixcol2/common.h>

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

TEST(Create_and_destroy, valid) {
    auto tmpl_udp = ipx_tmpl_create(50, 50, IPX_SESSION_TYPE_UDP);
    auto tmpl_tcp =  ipx_tmpl_create(50, 50, IPX_SESSION_TYPE_TCP);
    auto tmpl_sctp =  ipx_tmpl_create(50, 50, IPX_SESSION_TYPE_SCTP);
    EXPECT_NE(tmpl_udp, nullptr);
    EXPECT_NE(tmpl_tcp, nullptr);
    EXPECT_NE(tmpl_sctp, nullptr);
    EXPECT_NO_THROW(ipx_tmpl_destroy(tmpl_udp));
    EXPECT_NO_THROW(ipx_tmpl_destroy(tmpl_tcp));
    EXPECT_NO_THROW(ipx_tmpl_destroy(tmpl_sctp));
}

TEST(Create, invalid) {
    auto tmpl = ipx_tmpl_create(50, 50, static_cast<IPX_SESSION_TYPE>(999));
    EXPECT_EQ(tmpl, nullptr);
}

TEST(Iemgr, valid) {
    auto tmpl = ipx_tmpl_create(50, 50, IPX_SESSION_TYPE_UDP);

    EXPECT_EQ(ipx_tmpl_iemgr_load(tmpl, NULL), IPX_OK);

    fds_iemgr_t *iemgr = fds_iemgr_create();
    EXPECT_EQ(ipx_tmpl_iemgr_load(tmpl, iemgr), IPX_OK);
    ipx_tmpl_destroy(tmpl);
    fds_iemgr_destroy(iemgr);
}

TEST(Set, valid) {
    auto tmpl = ipx_tmpl_create(50, 50, IPX_SESSION_TYPE_UDP);
    EXPECT_NO_THROW(ipx_tmpl_set(tmpl, 42, 42));
    ipx_tmpl_destroy(tmpl);
}

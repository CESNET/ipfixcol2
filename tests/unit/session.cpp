//
// Created by lukashutak on 05/03/18.
//

#include <gtest/gtest.h>
#include <ipfixcol2.h>

extern "C" {
#include <ipfixcol2.h>
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

static void
net_tester(fds_session_type stype, uint8_t af_family, const std::string &src_ip,
    const std::string &dst_ip, uint16_t src_port, uint16_t dst_port, uint16_t lt_data,
    uint16_t lt_opts)
{
    SCOPED_TRACE("Address: " + src_ip + ":" + std::to_string(src_port) + " -> "
        + dst_ip + ":" + std::to_string(dst_port));

    // Setup
    ipx_session_net net_src;
    net_src.l3_proto = af_family;
    ASSERT_EQ(inet_pton(af_family, src_ip.c_str(), &net_src.addr_src), 1);
    ASSERT_EQ(inet_pton(af_family, dst_ip.c_str(), &net_src.addr_dst), 1);
    net_src.port_src = src_port;
    net_src.port_dst = dst_port;

    // Test
    struct ipx_session *session;
    switch (stype) {
    case FDS_SESSION_TCP:  session = ipx_session_new_tcp(&net_src); break;
    case FDS_SESSION_UDP:  session = ipx_session_new_udp(&net_src, lt_data, lt_opts); break;
    case FDS_SESSION_SCTP: session = ipx_session_new_sctp(&net_src); break;
    default: FAIL() << "Unsupported session type!";
    }

    ASSERT_NE(session, nullptr);
    EXPECT_EQ(session->type, stype);
    if (session->type == FDS_SESSION_UDP) {
        EXPECT_EQ(session->udp.lifetime.tmplts, lt_data);
        EXPECT_EQ(session->udp.lifetime.opts_tmplts, lt_opts);
    }

    const ipx_session_net *net;
    switch (session->type) {
    case FDS_SESSION_TCP:  net = &session->tcp.net; break;
    case FDS_SESSION_UDP:  net = &session->udp.net; break;
    case FDS_SESSION_SCTP: net = &session->sctp.net; break;
    default: FAIL() << "Unsupported session type!";
    }

    EXPECT_EQ(net->l3_proto, net_src.l3_proto);
    EXPECT_EQ(net->port_src, net_src.port_src);
    EXPECT_EQ(net->port_dst, net_src.port_dst);

    if (session->tcp.net.l3_proto == AF_INET) {
        const size_t addr_size = sizeof(net->addr_src.ipv4);
        EXPECT_EQ(memcmp(&net->addr_src.ipv4, &net_src.addr_src.ipv4, addr_size), 0);
        EXPECT_EQ(memcmp(&net->addr_dst.ipv4, &net_src.addr_dst.ipv4, addr_size), 0);
    } else if (session->tcp.net.l3_proto == AF_INET6) {
        const size_t addr_size = sizeof(net->addr_src.ipv6);
        EXPECT_EQ(memcmp(&net->addr_src.ipv6, &net_src.addr_src.ipv6, addr_size), 0);
        EXPECT_EQ(memcmp(&net->addr_dst.ipv6, &net_src.addr_dst.ipv6, addr_size), 0);
    }  else {
        FAIL() << "Unknown IP address type!";
    }

    std::string exp_ident = src_ip + ":" + std::to_string(src_port);
    EXPECT_EQ(session->ident, exp_ident);

    // TearDown
    ipx_session_destroy(session);
}

TEST(tcp, valid)
{
    // IPv4
    net_tester(FDS_SESSION_TCP, AF_INET, "127.0.0.1",   "127.0.0.1",   65000, 4739,  0, 0);
    net_tester(FDS_SESSION_TCP, AF_INET, "169.254.0.1", "169.254.0.2", 12345, 23456, 0, 0);
    net_tester(FDS_SESSION_TCP, AF_INET, "8.8.8.8",     "9.9.9.9",     20,    50,    0, 0);

    // IPv6
    net_tester(FDS_SESSION_TCP, AF_INET6, "::1", "fe80::6bae", 123, 456,  0, 0);
    net_tester(FDS_SESSION_TCP, AF_INET6, "2001:aaaa:bbbb:cccc:dddd:eeee:ffff:abcd", "::1",
        4700, 4739, 0, 0);
}

TEST(tcp, invalid)
{
    // Prepare working pattern
    struct ipx_session_net net4;
    net4.l3_proto = AF_INET;
    ASSERT_EQ(inet_pton(AF_INET, "1.2.3.4", &net4.addr_src), 1);
    ASSERT_EQ(inet_pton(AF_INET, "4.3.2.1", &net4.addr_dst), 1);
    net4.port_src = 12345;
    net4.port_dst = 54321;

    // Invalid AF family
    struct ipx_session_net net4_family = net4;
    net4_family.l3_proto = 0;
    ASSERT_EQ(ipx_session_new_tcp(&net4_family), nullptr);
}

TEST(udp, valid)
{
    // IPv4
    net_tester(FDS_SESSION_UDP, AF_INET, "127.0.0.1",   "127.0.0.1",   65000, 4739,  60, 0);
    net_tester(FDS_SESSION_UDP, AF_INET, "169.254.0.1", "169.254.0.2", 12345, 23456, 0, 10);
    net_tester(FDS_SESSION_UDP, AF_INET, "8.8.8.8",     "9.9.9.9",     20,    50,    65535, 65535);

    // IPv6
    net_tester(FDS_SESSION_UDP, AF_INET6, "::1", "fe80::6bae", 123, 456,  60, 60);
    net_tester(FDS_SESSION_UDP, AF_INET6, "2001:aaaa:bbbb:cccc:dddd:eeee:ffff:abcd", "::1",
        4700, 4739, 3600, 3600);
}

TEST(udp, invalid)
{
    // Prepare working pattern
    struct ipx_session_net net6;
    net6.l3_proto = AF_INET6;
    ASSERT_EQ(inet_pton(AF_INET6, "aaaa::ffff", &net6.addr_src), 1);
    ASSERT_EQ(inet_pton(AF_INET6, "ffff::aaaa", &net6.addr_dst), 1);
    net6.port_src = 12345;
    net6.port_dst = 54321;

    // Invalid AF family
    struct ipx_session_net net6_family = net6;
    net6_family.l3_proto = 0;
    ASSERT_EQ(ipx_session_new_udp(&net6_family, 60, 60), nullptr);
}

TEST(sctp, valid)
{
    // IPv4
    net_tester(FDS_SESSION_SCTP, AF_INET, "127.0.0.1",   "127.0.0.1",   65000, 4739,  0, 0);
    net_tester(FDS_SESSION_SCTP, AF_INET, "169.254.0.1", "169.254.0.2", 12345, 23456, 0, 0);
    net_tester(FDS_SESSION_SCTP, AF_INET, "8.8.8.8",     "9.9.9.9",     20,    50,    0, 0);

    // IPv6
    net_tester(FDS_SESSION_SCTP, AF_INET6, "::1", "fe80::6bae", 123, 456,  0, 0);
    net_tester(FDS_SESSION_SCTP, AF_INET6, "2001:aaaa:bbbb:cccc:dddd:eeee:ffff:abcd", "::1",
        4700, 4739, 0, 0);
}

TEST(sctp, invalid)
{
    // Prepare working pattern
    struct ipx_session_net net6;
    net6.l3_proto = AF_INET6;
    ASSERT_EQ(inet_pton(AF_INET6, "1010:2020:3030:4040:5050:6060:7070:8080", &net6.addr_src), 1);
    ASSERT_EQ(inet_pton(AF_INET6, "8080:7070:6060:5050:4040:3030:2020:1010", &net6.addr_dst), 1);
    net6.port_src = 12345;
    net6.port_dst = 54321;

    // Invalid AF family
    struct ipx_session_net net6_family = net6;
    net6_family.l3_proto = 0;
    ASSERT_EQ(ipx_session_new_sctp(&net6_family), nullptr);
}

TEST(file, valid)
{
    const char *s1_full = "/tmp/file/file.201803060853";
    const char *s1_ident = "file.201803060853";
    struct ipx_session *s1 = ipx_session_new_file(s1_full);
    ASSERT_NE(s1, nullptr);
    EXPECT_EQ(s1->type, FDS_SESSION_FILE);
    EXPECT_STREQ(s1->file.file_path, s1_full);
    EXPECT_STREQ(s1->ident, s1_ident);
    ipx_session_destroy(s1);

    const char *s2_full = "data/0011";
    const char *s2_ident = "0011";
    struct ipx_session *s2 = ipx_session_new_file(s2_full);
    ASSERT_NE(s2, nullptr);
    EXPECT_EQ(s2->type, FDS_SESSION_FILE);
    EXPECT_STREQ(s2->file.file_path, s2_full);
    EXPECT_STREQ(s2->ident, s2_ident);
    ipx_session_destroy(s2);

    const char *s3_full = "/";
    const char *s3_ident = "/";
    struct ipx_session *s3 = ipx_session_new_file(s3_full);
    ASSERT_NE(s3, nullptr);
    EXPECT_EQ(s3->type, FDS_SESSION_FILE);
    EXPECT_STREQ(s3->file.file_path, s3_full);
    EXPECT_STREQ(s3->ident, s3_ident);
    ipx_session_destroy(s3);
}

TEST(file, invalid)
{
    const char *s1_full = "";
    struct ipx_session *s1 = ipx_session_new_file(s1_full);
    EXPECT_EQ(s1, nullptr);
}
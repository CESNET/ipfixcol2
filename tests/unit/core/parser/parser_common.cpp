#include <gtest/gtest.h>
#include <MsgGen.h>
#include <ipfixcol2/session.h>

extern "C" {
    #include <core/context.h>
    #include <core/parser.h>
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

// Create main class for parameterized test
class Common : public ::testing::TestWithParam<enum fds_session_type> {
protected:
    using ctx_uniq = std::unique_ptr<ipx_ctx_t, decltype(&ipx_ctx_destroy)>;
    using parser_uniq = std::unique_ptr<ipx_parser_t, decltype(&ipx_parser_destroy)>;
    using iemgr_uniq = std::unique_ptr<fds_iemgr_t, decltype(&fds_iemgr_destroy)>;
    using session_uniq = std::unique_ptr<struct ipx_session, decltype(&ipx_session_destroy)>;

    static const ipx_verb_level DEF_VERB = IPX_VERB_DEBUG;
    fds_iemgr_t *iemgr;
    ipx_parser_t *parser;
    ipx_session *session;
    ipx_ctx_t *ctx;

    // Setup
    void SetUp() override {
        // Prepare typical Network configuration
        ipx_session_net net_cfg;
        net_cfg.l3_proto = AF_INET;
        net_cfg.port_src = 60000;
        net_cfg.port_dst = 4739; // Typical collector port
        ASSERT_EQ(inet_pton(AF_INET, "192.168.0.2", &net_cfg.addr_src.ipv4), 1);
        ASSERT_EQ(inet_pton(AF_INET, "192.168.0.1", &net_cfg.addr_dst.ipv4), 1);

        // Create a parser, a session and an IE manager
        std::string ctx_name = "Testing context";
        std::string parser_name = ctx_name + " (parser)";

        parser_uniq  parser_wrap(ipx_parser_create(parser_name.c_str(), DEF_VERB), &ipx_parser_destroy);
        iemgr_uniq   iemgr_wrap(fds_iemgr_create(), &fds_iemgr_destroy);
        ctx_uniq     ctx_wrap(ipx_ctx_create(ctx_name.c_str(), nullptr), &ipx_ctx_destroy);
        session_uniq session_wrap(NULL, &ipx_session_destroy);

        switch (GetParam()) {
        case FDS_SESSION_TCP:
            session_wrap.reset(ipx_session_new_tcp(&net_cfg));
            break;
        case FDS_SESSION_UDP:
            session_wrap.reset(ipx_session_new_udp(&net_cfg, 0, 0));
            break;
        case FDS_SESSION_SCTP:
            session_wrap.reset(ipx_session_new_sctp(&net_cfg));
            break;
        case FDS_SESSION_FILE:
            session_wrap.reset(ipx_session_new_file("fake_file.data"));
            break;
        default:
            throw std::runtime_error("Unknown session type!");
        }

        // Parse IE elements
        if (fds_iemgr_read_file(iemgr_wrap.get(), "data/iana_part.xml", false) != FDS_OK) {
            std::string err_msg = fds_iemgr_last_err(iemgr_wrap.get());
            throw std::runtime_error("Failed to load IEs: " + err_msg);
        }

        parser = parser_wrap.release();
        iemgr = iemgr_wrap.release();
        session = session_wrap.release();
        ctx = ctx_wrap.release();
    }

    // Tear Down
    void TearDown() override {
        // Cleanup
        ipx_session_destroy(session);
        ipx_parser_destroy(parser);
        fds_iemgr_destroy(iemgr);
        ipx_ctx_destroy(ctx);
    }
};

// Define parameters of parametrized test
INSTANTIATE_TEST_CASE_P(Parser, Common, ::testing::Values(FDS_SESSION_UDP,
    FDS_SESSION_TCP, FDS_SESSION_SCTP, FDS_SESSION_FILE));

// Just create and destroy parser
TEST_P(Common, createAndDestroy)
{
    // Nothing to do...
    ASSERT_NE(parser, nullptr);
}

// Just defined source of IE and destroy the parser
TEST_P(Common, enableIEMgr)
{
    ipx_msg_garbage *garbage;
    ASSERT_EQ(ipx_parser_ie_source(parser, iemgr, &garbage), IPX_OK);
    if (garbage) {
        ipx_msg_garbage_destroy(garbage);
    }
}

// One source, one message with one template and one record
TEST_P(Common, simple)
{
    const uint16_t tmplt_id = 256;
    ipfix_trec trec(tmplt_id);
    trec.add_field(8, 4);  // SRC IPv4 address
    trec.add_field(12, 4); // DSC IPv4 address
    trec.add_field(1, 4);  // bytes
    trec.add_field(2, 4);  // packets

    ipfix_set set_tmplts(2);
    set_tmplts.add_rec(trec);

    const std::string src_addr = "127.0.0.1";
    const std::string dst_addr = "127.0.0.2";
    uint64_t bytes = 12345;
    uint64_t pkts  = 14;

    ipfix_drec drec;
    drec.append_ip(src_addr);
    drec.append_ip(dst_addr);
    drec.append_uint(bytes, 4);
    drec.append_uint(pkts, 4);

    ipfix_set set_data(tmplt_id);
    set_data.add_rec(drec);

    ipfix_msg msg;
    msg.add_set(set_tmplts);
    msg.add_set(set_data);

    uint32_t odid = 1;
    struct ipx_msg_ctx msg_ctx = {session, odid, 0};
    uint16_t msg_size = msg.size();
    uint8_t *msg_data = reinterpret_cast<uint8_t *>(msg.release());
    ipx_msg_ipfix_t *ipfix_msg = ipx_msg_ipfix_create(ctx, &msg_ctx, msg_data, msg_size);
    ASSERT_NE(ipfix_msg, nullptr);

    ipx_msg_garbage *garbage;
    ASSERT_EQ(ipx_parser_process(parser, &ipfix_msg, &garbage), IPX_OK);

    EXPECT_EQ(ipx_msg_ipfix_get_drec_cnt(ipfix_msg), 1);
    ipx_ipfix_record *rec = ipx_msg_ipfix_get_drec(ipfix_msg, 0);
    ASSERT_NE(rec, nullptr);

    fds_drec_field field;
    ASSERT_GE(fds_drec_find(&rec->rec, 0, 1, &field), 0); // bytes

    uint64_t value;
    ASSERT_EQ(fds_get_uint_be(field.data, field.size, &value), FDS_OK);
    EXPECT_EQ(value, bytes);

    ipx_msg_ipfix_destroy(ipfix_msg);
}


// Max message (65000 records in one message)...
#include <arpa/inet.h>
#include <gtest/gtest.h>
#include <memory>

#include <ipfixcol2.h>
#include <libfds/ipfix_parsers.h>

extern "C" {
#include <core/netflow2ipfix/netflow2ipfix.h>
#include <core/netflow2ipfix/netflow_structs.h>
#include <core/context.h>
}

// Number of fields in NetFlow v5 Template (18x standard field + 2x padding + 2x sampling field)
constexpr size_t NF5_FIELD_CNT = 22U;

class ThrowListener : public testing::EmptyTestEventListener {
    void OnTestPartResult(const testing::TestPartResult& result) override {
        if (result.type() == testing::TestPartResult::kFatalFailure) {
            throw testing::AssertionException(result);
        }
    }
};

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::UnitTest::GetInstance()->listeners().Append(new ThrowListener);
    return RUN_ALL_TESTS();
}

// Base TestCase fixture
class MsgBase : public ::testing::Test {
protected:
    /// Before each Test case
    void SetUp() override
    {
        context_create();
        session_create();
    }

    /// After each Test case
    void TearDown() override
    {
    }

    // Create fake plugin context (i.e. without function callbacks)
    void
    context_create()
    {
        const ::testing::TestInfo* const test_info =
            ::testing::UnitTest::GetInstance()->current_test_info();
        m_ctx.reset(ipx_ctx_create(test_info->name(), nullptr));
        ASSERT_NE(m_ctx, nullptr);
    }
    // Create a Transport Session
    void
    session_create()
    {
        ipx_session_net net_cfg;
        net_cfg.l3_proto = AF_INET;
        net_cfg.port_src = 60000;
        net_cfg.port_dst = 4739; // Typical collector port
        ASSERT_EQ(inet_pton(AF_INET, "192.168.0.2", &net_cfg.addr_src.ipv4), 1);
        ASSERT_EQ(inet_pton(AF_INET, "192.168.0.1", &net_cfg.addr_dst.ipv4), 1);
        m_session.reset(ipx_session_new_udp(&net_cfg, 0, 0));
        ASSERT_NE(m_session, nullptr);
    }
    /**
     * @brief Create the NetFlow v5 to IPFIX converter
     * @param[in] odid      Observation Domain ID of generated IPFIX Messages
     * @param[in] tmplt_ref Template Refresh interval (in seconds)
     * @param[in] verb      Verbosity level of the converter
     */
    void
    converter_create(uint32_t odid = 0, uint32_t tmplt_ref = 0, ipx_verb_level verb = IPX_VERB_NONE)
    {
        const ::testing::TestInfo* const test_info =
            ::testing::UnitTest::GetInstance()->current_test_info();
        std::string name = std::string(test_info->name()) + " (NFv5 -> IPFIX converter)";
        m_conv.reset(ipx_nf5_conv_init(name.c_str(), verb, tmplt_ref, odid));
        ASSERT_NE(m_conv, nullptr);
    }

    /// Default header data (can be modified by user)
    struct msg_data_hdr {
        uint16_t version        = IPX_NF5_VERSION;
        uint16_t count          = 0;
        uint32_t sys_uptime     = 10001U; // 10.001 seconds since boot
        uint32_t unix_sec       = 1562857357U; // 2019-07-11T15:02:37+00:00
        uint32_t unix_nsec      = 123456789U;
        uint32_t flow_seq       = 10U;
        uint8_t  engine_type    = 12;
        uint8_t  engine_id      = 13;
        uint16_t sampling_int   = 0;
    };

    /// Default record data (can be modified by user)
    struct msg_data_rec {
        uint32_t addr_src;
        uint32_t addr_dst;
        uint32_t nexthop;
        uint16_t snmp_input    = 165U;
        uint16_t snmp_output   = 166U;
        uint32_t delta_pkts    = 100U;
        uint32_t delta_octets  = 123456U;
        uint32_t ts_first      = 6501;
        uint32_t ts_last       = 9000;
        uint16_t port_src      = 65102;
        uint16_t port_dst      = 53;
        uint8_t  tcp_flags     = 18; // syn + ack
        uint8_t  proto         = 17;
        uint8_t  tos           = 224; // "Network control"
        uint16_t as_src        = 15169;
        uint16_t as_dst        = 13335;
        uint8_t  mask_src      = 0;
        uint8_t  mask_dst      = 0;

        msg_data_rec(const std::string ip_src = "8.8.8.8", const std::string ip_dst = "1.1.1.1",
            const std::string ip_next = "1.2.3.4")
        {
            EXPECT_EQ(inet_pton(AF_INET, ip_src.c_str(), &addr_src), 1);
            EXPECT_EQ(inet_pton(AF_INET, ip_dst.c_str(), &addr_dst), 1);
            EXPECT_EQ(inet_pton(AF_INET, ip_next.c_str(), &nexthop), 1);
        }
    };

    /**
     * @brief Create a empty NetFlow v5 Message
     * @param[out] msg_data Pointer to the new empty message
     * @param[out] msg_size Size of the message
     * @param[in]  data     Data to be filled into the header
     */
    void
    msg_create(uint8_t **msg_data, uint16_t *msg_size, const struct msg_data_hdr &data)
    {
        uint16_t new_size = sizeof(struct ipx_nf5_hdr);
        struct ipx_nf5_hdr *new_msg = (struct ipx_nf5_hdr *) calloc(1, new_size);
        ASSERT_NE(new_msg, nullptr);

        new_msg->version = htons(data.version);
        new_msg->count = htons(data.count);
        new_msg->sys_uptime = htonl(data.sys_uptime);
        new_msg->unix_sec = htonl(data.unix_sec);
        new_msg->unix_nsec = htonl(data.unix_nsec);
        new_msg->flow_seq = htonl(data.flow_seq);
        new_msg->engine_type = data.engine_type;
        new_msg->engine_id = data.engine_id;
        new_msg->sampling_interval = htons(data.sampling_int);

        *msg_data = reinterpret_cast<uint8_t *>(new_msg);
        *msg_size = new_size;
    }

    /**
     * @brief Add a Data Record to the message
     * @note Number of records is automatically increased
     * @param[in,out] msg_data Pointer to message to append
     * @param[in,out] msg_size Size of the message to append
     * @param[in]     data     Data to be filled into the record
     */
    void
    msg_rec_add(uint8_t **msg_data, uint16_t *msg_size, const struct msg_data_rec &data)
    {
        ASSERT_NE(msg_data, nullptr);
        ASSERT_NE(msg_size, nullptr);
        ASSERT_GE(*msg_size, sizeof(struct ipx_nf5_hdr));

        uint16_t rec_size = sizeof(struct ipx_nf5_rec);
        ASSERT_LE(*msg_size, UINT16_MAX - rec_size) << "Maximum message size has been reached";
        uint16_t new_size = *msg_size + rec_size;
        uint8_t *new_msg = (uint8_t *) realloc(*msg_data, new_size);
        ASSERT_NE(new_msg, nullptr);

        // Update the header
        auto new_hdr = reinterpret_cast<struct ipx_nf5_hdr *>(new_msg);
        new_hdr->count = htons(ntohs(new_hdr->count) + 1); // increment the record counter

        // Update the body
        auto new_rec = reinterpret_cast<struct ipx_nf5_rec *>(&new_msg[*msg_size]);
        new_rec->addr_src = data.addr_src;
        new_rec->addr_dst = data.addr_dst;
        new_rec->nexthop = data.nexthop;
        new_rec->snmp_input = htons(data.snmp_input);
        new_rec->snmp_output = htons(data.snmp_output);
        new_rec->delta_pkts = htonl(data.delta_pkts);
        new_rec->delta_octets = htonl(data.delta_octets);
        new_rec->ts_first = htonl(data.ts_first);
        new_rec->ts_last = htonl(data.ts_last);
        new_rec->port_src = htons(data.port_src);
        new_rec->port_dst = htons(data.port_dst);
        new_rec->tcp_flags = data.tcp_flags;
        new_rec->proto = data.proto;
        new_rec->tos = data.tos;
        new_rec->as_src = htons(data.as_src);
        new_rec->as_dst = htons(data.as_dst);
        new_rec->mask_src = data.mask_src;
        new_rec->mask_dst = data.mask_dst;
        new_rec->_pad1 = 0;
        new_rec->_pad2 = 0;

        *msg_data = reinterpret_cast<uint8_t *>(new_msg);
        *msg_size = new_size;
    }

    void
    prepare_msg(const struct ipx_msg_ctx *msg_ctx, uint8_t *msg_data, uint16_t msg_size)
    {
        m_msg.reset(ipx_msg_ipfix_create(m_ctx.get(), msg_ctx, msg_data, msg_size));
        ASSERT_NE(m_msg, nullptr);
    }

    /**
     * Compare converted IPFIX header with expected content
     * @param[in] ipx_hdr Converted IPFIX header to check
     * @param[in] exp_hdr Expected values
     * @param[in] odid    Expected ODID values
     * @note: Size of the IPFIX Message is not checked!
     */
    void
    cmp_header(const uint8_t *ipx_hdr, const msg_data_hdr &exp_hdr, uint32_t odid)
    {
        const auto *hdr = reinterpret_cast<const struct fds_ipfix_msg_hdr*>(ipx_hdr);
        EXPECT_EQ(ntohs(hdr->version), FDS_IPFIX_VERSION);
        EXPECT_EQ(ntohl(hdr->export_time), exp_hdr.unix_sec);
        EXPECT_EQ(ntohl(hdr->seq_num), exp_hdr.flow_seq);
        EXPECT_EQ(ntohl(hdr->odid), odid);
    }

    /**
     * @brief Compare converted IPFIX record with expected content
     * @param[in] ipx_rec  Converted IPFIX record to check
     * @param[in] orig_hdr Original NetFlow header parameters
     * @param[in] orig_rec Original NetFlow record parameters
     */
    void
    cmp_rec(struct fds_drec ipx_rec, const msg_data_hdr &orig_hdr, const msg_data_rec &orig_rec)
    {
        struct fds_drec_iter it;

        auto cmp_addr = [&it, &ipx_rec](uint16_t ipfix_id, uint32_t ipv4) {
            SCOPED_TRACE("ID: " + std::to_string(ipfix_id));

            fds_drec_iter_init(&it, &ipx_rec, 0);
            ASSERT_NE(fds_drec_iter_find(&it, 0, ipfix_id), FDS_EOC);
            ASSERT_EQ(it.field.size, 4U);
            ASSERT_EQ(*(uint32_t *) it.field.data, ipv4);
            EXPECT_EQ(fds_drec_iter_find(&it, 0, ipfix_id), FDS_EOC);
        };
        auto cmp_uint = [&it, &ipx_rec](uint16_t ipfix_id, uint64_t value) {
            SCOPED_TRACE("ID: " + std::to_string(ipfix_id));
            uint64_t tmp_value;

            fds_drec_iter_init(&it, &ipx_rec, 0);
            ASSERT_NE(fds_drec_iter_find(&it, 0, ipfix_id), FDS_EOC);
            ASSERT_LE(it.field.size, 8U);
            ASSERT_EQ(fds_get_uint_be(it.field.data, it.field.size, &tmp_value), FDS_OK);
            ASSERT_EQ(tmp_value, value);
            EXPECT_EQ(fds_drec_iter_find(&it, 0, ipfix_id), FDS_EOC);
        };
        auto cmp_time = [&it, &ipx_rec, &orig_hdr](uint16_t ipfix_id, uint32_t rec_time) {
            SCOPED_TRACE("ID: " + std::to_string(ipfix_id));
            uint64_t ipx_ts; // milliseconds since Epoch

            fds_drec_iter_init(&it, &ipx_rec, 0);
            ASSERT_NE(fds_drec_iter_find(&it, 0, ipfix_id), FDS_EOC);
            ASSERT_EQ(fds_get_datetime_lp_be(it.field.data, it.field.size,
                FDS_ET_DATE_TIME_MILLISECONDS, &ipx_ts) , FDS_OK);

            uint64_t sys_time = (orig_hdr.unix_sec * 1000ULL) + (orig_hdr.unix_nsec / 1000000ULL);
            uint32_t sys_diff = orig_hdr.sys_uptime - rec_time; // go back in time (in milliseconds)
            EXPECT_EQ(ipx_ts, sys_time - sys_diff);
            EXPECT_EQ(fds_drec_iter_find(&it, 0, ipfix_id), FDS_EOC);
        };

        // IP addresses
        cmp_addr(8, orig_rec.addr_src);
        cmp_addr(12, orig_rec.addr_dst);
        cmp_addr(15, orig_rec.nexthop);
        // SNMP
        cmp_uint(10, orig_rec.snmp_input);
        cmp_uint(14, orig_rec.snmp_output);
        // Deltas
        cmp_uint(1, orig_rec.delta_octets);
        cmp_uint(2, orig_rec.delta_pkts);
        // Timestamps
        cmp_time(152, orig_rec.ts_first);
        cmp_time(153, orig_rec.ts_last);
        // Ports
        cmp_uint(7, orig_rec.port_src);
        cmp_uint(11, orig_rec.port_dst);
        // TCP flags, protocol, TOS
        cmp_uint(6, orig_rec.tcp_flags);
        cmp_uint(4, orig_rec.proto);
        cmp_uint(5, orig_rec.tos);
        // AS
        cmp_uint(16, orig_rec.as_src);
        cmp_uint(17, orig_rec.as_dst);
        // Prefix mask
        cmp_uint(9, orig_rec.mask_src);
        cmp_uint(13, orig_rec.mask_dst);
        // Sampling (interval and algorithm)
        cmp_uint(34, orig_hdr.sampling_int & 0x3FFF);
        cmp_uint(35, orig_hdr.sampling_int >> 14);
    }

    /**
     * @brief Parse Template Set
     * @note Gets the next IPFIX Set and if it is a Template Set, parse the Template and return it
     * @param[in]  it_sets Initialized IPFIX Set iterator
     * @param[out] tmplt   Parsed Template
     */
    void
    parse_tset(struct fds_sets_iter &it_sets, struct fds_template **tmplt)
    {
        struct fds_tset_iter it_tset;
        ASSERT_EQ(fds_sets_iter_next(&it_sets), FDS_OK);
        ASSERT_EQ(ntohs(it_sets.set->flowset_id), FDS_IPFIX_SET_TMPLT);

        // Try to parse the Template
        fds_tset_iter_init(&it_tset, it_sets.set);
        ASSERT_EQ(fds_tset_iter_next(&it_tset), FDS_OK);
        EXPECT_EQ(it_tset.scope_cnt, 0);
        EXPECT_EQ(it_tset.field_cnt, NF5_FIELD_CNT);

        struct fds_template *tmplt_parsed = nullptr;
        uint16_t tmplt_size = it_tset.size;
        ASSERT_EQ(fds_template_parse(FDS_TYPE_TEMPLATE, it_tset.ptr.trec, &tmplt_size, &tmplt_parsed),
            FDS_OK);
        ASSERT_NE(tmplt_parsed, nullptr);
        EXPECT_EQ(tmplt_size, it_tset.size);

        // Expect no more Templates in the Set
        EXPECT_EQ(fds_tset_iter_next(&it_tset), FDS_EOC);

        *tmplt = tmplt_parsed;
    }

    // Transport Session
    std::unique_ptr<struct ipx_session, decltype(&ipx_session_destroy)>
        m_session = {nullptr, &ipx_session_destroy};
    // Plugin context (necessary for building IPFIX Messages)
    std::unique_ptr<ipx_ctx_t, decltype(&ipx_ctx_destroy)>
        m_ctx = {nullptr, &ipx_ctx_destroy};
    // NetFlow Message to convert / converted IPFIX Message
    std::unique_ptr<ipx_msg_ipfix_t, decltype(&ipx_msg_ipfix_destroy)>
        m_msg = {nullptr, &ipx_msg_ipfix_destroy};
    // NetFlow v5 to IPFIX converter
    std::unique_ptr<ipx_nf5_conv_t, decltype(&ipx_nf5_conv_destroy)>
        m_conv = {nullptr, &ipx_nf5_conv_destroy};
};

// Try to convert an empty NetFlow v5 message
TEST_F(MsgBase, emptyMessages)
{
    uint8_t *msg_data = nullptr;
    uint16_t msg_size = 0;
    struct fds_sets_iter it_sets;
    struct ipx_msg_ctx msg_ctx;

    // Create a converter
    const uint32_t VALUE_ODID = 10;
    const uint32_t VALUE_TMPLT_REF = 0; // no refresh
    converter_create(VALUE_ODID, VALUE_TMPLT_REF, IPX_VERB_DEBUG);

    // Create an empty message
    const struct msg_data_hdr HDR_DATA; // default header data
    msg_create(&msg_data, &msg_size, HDR_DATA);
    msg_ctx = {m_session.get(), VALUE_ODID, 0};
    prepare_msg(&msg_ctx, msg_data, msg_size);

    // Convert the message
    ASSERT_EQ(ipx_nf5_conv_process(m_conv.get(), m_msg.get()), IPX_OK);

    // Try to parse the IPFIX Message
    msg_data = ipx_msg_ipfix_get_packet(m_msg.get());
    auto msg_hdr = reinterpret_cast<fds_ipfix_msg_hdr *>(msg_data);

    // Check the header
    msg_size = ntohs(msg_hdr->length);
    ASSERT_GE(msg_size, FDS_IPFIX_MSG_HDR_LEN);
    cmp_header(msg_data, HDR_DATA, VALUE_ODID);

    // Check the body... this is the first message, therefore, the a Template should be added
    fds_sets_iter_init(&it_sets, msg_hdr);
    struct fds_template *tmplt_parsed = nullptr;
    parse_tset(it_sets, &tmplt_parsed);
    fds_template_destroy(tmplt_parsed);

    // No more Templates and Data Sets
    EXPECT_EQ(fds_sets_iter_next(&it_sets), FDS_EOC);

    // Try to parse the next empty NetFlow v5 message ------------------------
    msg_create(&msg_data, &msg_size, HDR_DATA);
    prepare_msg(&msg_ctx, msg_data, msg_size);

    // Convert the message
    ASSERT_EQ(ipx_nf5_conv_process(m_conv.get(), m_msg.get()), IPX_OK);

    msg_data = ipx_msg_ipfix_get_packet(m_msg.get());
    msg_hdr = reinterpret_cast<fds_ipfix_msg_hdr *>(msg_data);
    msg_size = ntohs(msg_hdr->length);

    // Check the header
    ASSERT_GE(msg_size, FDS_IPFIX_MSG_HDR_LEN);
    cmp_header(msg_data, HDR_DATA, VALUE_ODID);

    // Body should be empty
    EXPECT_EQ(msg_size, FDS_IPFIX_MSG_HDR_LEN);
    fds_sets_iter_init(&it_sets, msg_hdr);
    EXPECT_EQ(fds_sets_iter_next(&it_sets), FDS_EOC);
}

// Convert NetFlow message with a single record (no template refresh)
TEST_F(MsgBase, singleRecordPerMessage)
{
    uint8_t *msg_data = nullptr;
    uint16_t msg_size = 0;
    struct fds_sets_iter it_sets;
    struct fds_dset_iter it_dset;
    struct ipx_msg_ctx msg_ctx;

    // Create a converter
    const uint32_t VALUE_ODID = 12345678UL;
    const uint32_t VALUE_TMPLT_REF = 0; // no refresh
    converter_create(VALUE_ODID, VALUE_TMPLT_REF, IPX_VERB_DEBUG);

    // Create a message
    struct msg_data_hdr HDR_DATA; // default header data
    struct msg_data_rec REC_DATA; // default record data
    msg_create(&msg_data, &msg_size, HDR_DATA);
    msg_rec_add(&msg_data, &msg_size, REC_DATA);
    msg_ctx = {m_session.get(), VALUE_ODID, 0};
    prepare_msg(&msg_ctx, msg_data, msg_size);

    // Convert the message
    ASSERT_EQ(ipx_nf5_conv_process(m_conv.get(), m_msg.get()), IPX_OK);

    // Try to parse the IPFIX Message
    msg_data = ipx_msg_ipfix_get_packet(m_msg.get());
    auto msg_hdr = reinterpret_cast<fds_ipfix_msg_hdr *>(msg_data);

    // Check the header
    msg_size = ntohs(msg_hdr->length);
    ASSERT_GE(msg_size, FDS_IPFIX_MSG_HDR_LEN);
    cmp_header(msg_data, HDR_DATA, VALUE_ODID);

    // 1) Template Set
    fds_sets_iter_init(&it_sets, msg_hdr);
    struct fds_template *tmplt_parsed = nullptr;
    parse_tset(it_sets, &tmplt_parsed);
    std::unique_ptr<struct fds_template, decltype(&fds_template_destroy)>
        tmplt(tmplt_parsed, &fds_template_destroy);

    // 2) Expect Data Set
    ASSERT_EQ(fds_sets_iter_next(&it_sets), FDS_OK);
    uint16_t set_id = ntohs(it_sets.set->flowset_id);
    ASSERT_GE(set_id, FDS_IPFIX_SET_MIN_DSET);
    ASSERT_EQ(set_id, tmplt->id);
    fds_dset_iter_init(&it_dset, it_sets.set, tmplt.get());
    ASSERT_EQ(fds_dset_iter_next(&it_dset), FDS_OK);
    struct fds_drec drec = {it_dset.rec, it_dset.size, tmplt.get(), nullptr};
    cmp_rec(drec, HDR_DATA, REC_DATA);

    // No more Data Records and Sets
    EXPECT_EQ(fds_dset_iter_next(&it_dset), FDS_EOC);
    EXPECT_EQ(fds_sets_iter_next(&it_sets), FDS_EOC);

    // Try to parse the NetFlow v5 message (also with 1 record) ------------------------
    HDR_DATA.flow_seq++;
    HDR_DATA.sys_uptime += 1000;
    HDR_DATA.unix_sec += 1;
    REC_DATA.ts_first =  7880;
    REC_DATA.ts_last = 10000;

    msg_create(&msg_data, &msg_size, HDR_DATA);
    msg_rec_add(&msg_data, &msg_size, REC_DATA);
    prepare_msg(&msg_ctx, msg_data, msg_size);

    // Convert the message
    ASSERT_EQ(ipx_nf5_conv_process(m_conv.get(), m_msg.get()), IPX_OK);

    // Try to parse the IPFIX Message
    msg_data = ipx_msg_ipfix_get_packet(m_msg.get());
    msg_hdr = reinterpret_cast<fds_ipfix_msg_hdr *>(msg_data);
    msg_size = ntohs(msg_hdr->length);

    // Check the header
    ASSERT_GE(msg_size, FDS_IPFIX_MSG_HDR_LEN);
    cmp_header(msg_data, HDR_DATA, VALUE_ODID);

    // Expect Data Set
    fds_sets_iter_init(&it_sets, msg_hdr);
    ASSERT_EQ(fds_sets_iter_next(&it_sets), FDS_OK);
    set_id = ntohs(it_sets.set->flowset_id);
    ASSERT_GE(set_id, FDS_IPFIX_SET_MIN_DSET);
    ASSERT_EQ(set_id, tmplt->id);
    fds_dset_iter_init(&it_dset, it_sets.set, tmplt.get());
    ASSERT_EQ(fds_dset_iter_next(&it_dset), FDS_OK);
    drec = {it_dset.rec, it_dset.size, tmplt.get(), nullptr};
    cmp_rec(drec, HDR_DATA, REC_DATA);

    // No more Data Records and Sets
    EXPECT_EQ(fds_dset_iter_next(&it_dset), FDS_EOC);
    EXPECT_EQ(fds_sets_iter_next(&it_sets), FDS_EOC);
}

// Multiple messages with multiple records (template refresh enabled but without impact)
TEST_F(MsgBase, multipleRecordsPerMessage)
{
    uint8_t *msg_data = nullptr;
    uint16_t msg_size = 0;
    struct fds_sets_iter it_sets;
    struct fds_dset_iter it_dset;
    struct ipx_msg_ctx msg_ctx;

    // Create a converter
    const uint32_t VALUE_ODID = 0UL;
    const uint32_t VALUE_TMPLT_REF = 10; // every 10 seconds
    converter_create(VALUE_ODID, VALUE_TMPLT_REF, IPX_VERB_DEBUG);

    // Create a message
    struct msg_data_hdr HDR_DATA;
    HDR_DATA.flow_seq = 0;
    HDR_DATA.sampling_int = (0x2 << 14) | 0x64; // non-zero sampling info
    struct msg_data_rec REC_DATA_A; // default record data
    struct msg_data_rec REC_DATA_B;
    REC_DATA_B.proto = 6;
    REC_DATA_B.mask_src = 24;
    REC_DATA_B.mask_dst = 16;

    msg_create(&msg_data, &msg_size, HDR_DATA);
    for (int i = 0; i < 5; ++i) { // Add 10 records
        msg_rec_add(&msg_data, &msg_size, REC_DATA_A);
        msg_rec_add(&msg_data, &msg_size, REC_DATA_B);
    }
    msg_ctx = {m_session.get(), VALUE_ODID, 0};
    prepare_msg(&msg_ctx, msg_data, msg_size);

    // Convert the message
    ASSERT_EQ(ipx_nf5_conv_process(m_conv.get(), m_msg.get()), IPX_OK);

    // Try to parse the IPFIX Message
    msg_data = ipx_msg_ipfix_get_packet(m_msg.get());
    auto msg_hdr = reinterpret_cast<fds_ipfix_msg_hdr *>(msg_data);

    // Check the header
    msg_size = ntohs(msg_hdr->length);
    ASSERT_GE(msg_size, FDS_IPFIX_MSG_HDR_LEN);
    cmp_header(msg_data, HDR_DATA, VALUE_ODID);

    // 1) Template Set
    fds_sets_iter_init(&it_sets, msg_hdr);
    struct fds_template *tmplt_parsed = nullptr;
    parse_tset(it_sets, &tmplt_parsed);
    std::unique_ptr<struct fds_template, decltype(&fds_template_destroy)>
        tmplt(tmplt_parsed, &fds_template_destroy);

    // 2) Expect Data Set
    ASSERT_EQ(fds_sets_iter_next(&it_sets), FDS_OK);
    uint16_t set_id = ntohs(it_sets.set->flowset_id);
    ASSERT_GE(set_id, FDS_IPFIX_SET_MIN_DSET);
    ASSERT_EQ(set_id, tmplt->id);
    fds_dset_iter_init(&it_dset, it_sets.set, tmplt.get());
    for (int i = 0; i < 5; ++i) {
        struct fds_drec drec;
        ASSERT_EQ(fds_dset_iter_next(&it_dset), FDS_OK);
        drec = {it_dset.rec, it_dset.size, tmplt.get(), nullptr};
        cmp_rec(drec, HDR_DATA, REC_DATA_A);

        ASSERT_EQ(fds_dset_iter_next(&it_dset), FDS_OK);
        drec = {it_dset.rec, it_dset.size, tmplt.get(), nullptr};
        cmp_rec(drec, HDR_DATA, REC_DATA_B);
    }

    // No more Data Records and Sets
    EXPECT_EQ(fds_dset_iter_next(&it_dset), FDS_EOC);
    EXPECT_EQ(fds_sets_iter_next(&it_sets), FDS_EOC);

    // Try to parse the NetFlow v5 message (100 records) ------------------------
    struct msg_data_hdr HDR_DATA2;
    HDR_DATA2.flow_seq = 10;
    HDR_DATA2.unix_sec += 1;
    struct msg_data_rec REC_DATA2;

    msg_create(&msg_data, &msg_size, HDR_DATA2);
    for (int i = 0; i < 100; ++i) { // Add 100 records
        msg_rec_add(&msg_data, &msg_size, REC_DATA2);
    }
    msg_ctx = {m_session.get(), VALUE_ODID, 0};
    prepare_msg(&msg_ctx, msg_data, msg_size);

    // Convert the message
    ASSERT_EQ(ipx_nf5_conv_process(m_conv.get(), m_msg.get()), IPX_OK);

    // Try to parse the IPFIX Message
    msg_data = ipx_msg_ipfix_get_packet(m_msg.get());
    msg_hdr = reinterpret_cast<fds_ipfix_msg_hdr *>(msg_data);
    msg_size = ntohs(msg_hdr->length);

    // Check the header
    ASSERT_GE(msg_size, FDS_IPFIX_MSG_HDR_LEN);
    cmp_header(msg_data, HDR_DATA2, VALUE_ODID);

    // Expect Data Set
    fds_sets_iter_init(&it_sets, msg_hdr);
    ASSERT_EQ(fds_sets_iter_next(&it_sets), FDS_OK);
    set_id = ntohs(it_sets.set->flowset_id);
    ASSERT_GE(set_id, FDS_IPFIX_SET_MIN_DSET);
    ASSERT_EQ(set_id, tmplt->id);
    fds_dset_iter_init(&it_dset, it_sets.set, tmplt.get());
    for (int i = 0; i < 100; ++i) {
        ASSERT_EQ(fds_dset_iter_next(&it_dset), FDS_OK);
        struct fds_drec drec = {it_dset.rec, it_dset.size, tmplt.get(), nullptr};
        cmp_rec(drec, HDR_DATA2, REC_DATA2);
    }

    // No more Data Records and Sets
    EXPECT_EQ(fds_dset_iter_next(&it_dset), FDS_EOC);
    EXPECT_EQ(fds_sets_iter_next(&it_sets), FDS_EOC);
}

// Automatic Template refresh
TEST_F(MsgBase, templateRefresh)                    // - 4 packets (3. with the same timestamp as 2.)
{
    uint8_t *msg_data = nullptr;
    uint16_t msg_size = 0;
    struct fds_sets_iter it_sets;
    struct fds_dset_iter it_dset;
    struct ipx_msg_ctx msg_ctx;

    // Create a converter
    const uint32_t VALUE_ODID = 2135UL;
    const uint32_t VALUE_TMPLT_REF = 5; // every 5 seconds
    converter_create(VALUE_ODID, VALUE_TMPLT_REF, IPX_VERB_DEBUG);

    // Create a message
    struct msg_data_hdr hdr_data; // default header data
    hdr_data.flow_seq = 123456789UL;
    struct msg_data_rec rec_data; // default record data
    msg_create(&msg_data, &msg_size, hdr_data);
    msg_rec_add(&msg_data, &msg_size, rec_data);
    msg_ctx = {m_session.get(), VALUE_ODID, 0};
    prepare_msg(&msg_ctx, msg_data, msg_size);

    // Convert the message
    ASSERT_EQ(ipx_nf5_conv_process(m_conv.get(), m_msg.get()), IPX_OK);

    // Try to parse the IPFIX Message
    msg_data = ipx_msg_ipfix_get_packet(m_msg.get());
    auto msg_hdr = reinterpret_cast<fds_ipfix_msg_hdr *>(msg_data);

    // Check the header
    msg_size = ntohs(msg_hdr->length);
    ASSERT_GE(msg_size, FDS_IPFIX_MSG_HDR_LEN);
    cmp_header(msg_data, hdr_data, VALUE_ODID);

    // 1) Template Set
    fds_sets_iter_init(&it_sets, msg_hdr);
    struct fds_template *tmplt_parsed = nullptr;
    parse_tset(it_sets, &tmplt_parsed);
    std::unique_ptr<struct fds_template, decltype(&fds_template_destroy)>
        tmplt(tmplt_parsed, &fds_template_destroy);

    // 2) Expect Data Set
    ASSERT_EQ(fds_sets_iter_next(&it_sets), FDS_OK);
    uint16_t set_id = ntohs(it_sets.set->flowset_id);
    ASSERT_GE(set_id, FDS_IPFIX_SET_MIN_DSET);
    ASSERT_EQ(set_id, tmplt->id);
    fds_dset_iter_init(&it_dset, it_sets.set, tmplt.get());
    ASSERT_EQ(fds_dset_iter_next(&it_dset), FDS_OK);
    struct fds_drec drec = {it_dset.rec, it_dset.size, tmplt.get(), nullptr};
    cmp_rec(drec, hdr_data, rec_data);

    // No more Data Records and Sets
    EXPECT_EQ(fds_dset_iter_next(&it_dset), FDS_EOC);
    EXPECT_EQ(fds_sets_iter_next(&it_sets), FDS_EOC);

    // Try to parse another message (expect refreshed template) ----------------
    hdr_data.flow_seq++;
    hdr_data.unix_sec += 5;
    hdr_data.sys_uptime += 5000;

    msg_create(&msg_data, &msg_size, hdr_data);
    msg_rec_add(&msg_data, &msg_size, rec_data);
    prepare_msg(&msg_ctx, msg_data, msg_size);

    // Convert the message
    ASSERT_EQ(ipx_nf5_conv_process(m_conv.get(), m_msg.get()), IPX_OK);

    // Try to parse the IPFIX Message
    msg_data = ipx_msg_ipfix_get_packet(m_msg.get());
    msg_hdr = reinterpret_cast<fds_ipfix_msg_hdr *>(msg_data);
    msg_size = ntohs(msg_hdr->length);

    // Check the header
    ASSERT_GE(msg_size, FDS_IPFIX_MSG_HDR_LEN);
    cmp_header(msg_data, hdr_data, VALUE_ODID);

    // 1) Template Set
    fds_sets_iter_init(&it_sets, msg_hdr);
    parse_tset(it_sets, &tmplt_parsed);
    tmplt.reset(tmplt_parsed);

    // 2) Expect Data Set
    ASSERT_EQ(fds_sets_iter_next(&it_sets), FDS_OK);
    set_id = ntohs(it_sets.set->flowset_id);
    ASSERT_GE(set_id, FDS_IPFIX_SET_MIN_DSET);
    ASSERT_EQ(set_id, tmplt->id);
    fds_dset_iter_init(&it_dset, it_sets.set, tmplt.get());
    ASSERT_EQ(fds_dset_iter_next(&it_dset), FDS_OK);
    drec = {it_dset.rec, it_dset.size, tmplt.get(), nullptr};
    cmp_rec(drec, hdr_data, rec_data);

    // No more Data Records and Sets
    EXPECT_EQ(fds_dset_iter_next(&it_dset), FDS_EOC);
    EXPECT_EQ(fds_sets_iter_next(&it_sets), FDS_EOC);

    // Try to parse another message (expect no templates) ----------------------
    hdr_data.flow_seq++;
    msg_create(&msg_data, &msg_size, hdr_data);
    prepare_msg(&msg_ctx, msg_data, msg_size);  // empty

    // Convert the message
    ASSERT_EQ(ipx_nf5_conv_process(m_conv.get(), m_msg.get()), IPX_OK);

    // Try to parse the IPFIX Message
    msg_data = ipx_msg_ipfix_get_packet(m_msg.get());
    msg_hdr = reinterpret_cast<fds_ipfix_msg_hdr *>(msg_data);
    msg_size = ntohs(msg_hdr->length);

    // Check the header
    ASSERT_EQ(msg_size, FDS_IPFIX_MSG_HDR_LEN);
    cmp_header(msg_data, hdr_data, VALUE_ODID);

    // Try to parse another message (expect refreshed template) ----------------
    hdr_data.unix_sec += 6;
    hdr_data.sys_uptime += 6000;

    msg_create(&msg_data, &msg_size, hdr_data);
    prepare_msg(&msg_ctx, msg_data, msg_size);  // empty

    // Convert the message
    ASSERT_EQ(ipx_nf5_conv_process(m_conv.get(), m_msg.get()), IPX_OK);

    // Try to parse the IPFIX Message
    msg_data = ipx_msg_ipfix_get_packet(m_msg.get());
    msg_hdr = reinterpret_cast<fds_ipfix_msg_hdr *>(msg_data);
    msg_size = ntohs(msg_hdr->length);

    // Check the header
    ASSERT_GE(msg_size, FDS_IPFIX_MSG_HDR_LEN);
    cmp_header(msg_data, hdr_data, VALUE_ODID);
    // Check the template
    fds_sets_iter_init(&it_sets, msg_hdr);
    parse_tset(it_sets, &tmplt_parsed);
    tmplt.reset(tmplt_parsed);
    EXPECT_EQ(fds_sets_iter_next(&it_sets), FDS_EOC);
}

// Missing message (unexpected sequence number)
TEST_F(MsgBase, missingMessage)
{
    uint8_t *msg_data = nullptr;
    uint16_t msg_size = 0;
    struct fds_sets_iter it_sets;
    struct ipx_msg_ctx msg_ctx;

    // Create a converter
    const uint32_t VALUE_ODID = 9879UL;
    const uint32_t VALUE_TMPLT_REF = 5; // every 5 seconds
    converter_create(VALUE_ODID, VALUE_TMPLT_REF, IPX_VERB_DEBUG);

    // Create a message
    struct msg_data_hdr hdr_data; // default header data
    struct msg_data_rec rec_data; // default record data
    msg_create(&msg_data, &msg_size, hdr_data);
    msg_rec_add(&msg_data, &msg_size, rec_data); // Message with 1 flow record
    msg_ctx = {m_session.get(), VALUE_ODID, 0};
    prepare_msg(&msg_ctx, msg_data, msg_size);

    // Convert the message (just to learn the converter current sequence number)
    ASSERT_EQ(ipx_nf5_conv_process(m_conv.get(), m_msg.get()), IPX_OK);

    // Try to parse another message (unexpected sequence number) ---------------
    hdr_data.flow_seq += 2;  // 1 missing flow record
    msg_create(&msg_data, &msg_size, hdr_data);
    msg_rec_add(&msg_data, &msg_size, rec_data); // Message with 1 flow record again
    msg_ctx = {m_session.get(), VALUE_ODID, 0};
    prepare_msg(&msg_ctx, msg_data, msg_size);

    // Convert the message
    ASSERT_EQ(ipx_nf5_conv_process(m_conv.get(), m_msg.get()), IPX_OK);

    // Try to parse the IPFIX Message
    msg_data = ipx_msg_ipfix_get_packet(m_msg.get());
    auto msg_hdr = reinterpret_cast<fds_ipfix_msg_hdr *>(msg_data);
    msg_size = ntohs(msg_hdr->length);
    ASSERT_GE(msg_size, FDS_IPFIX_MSG_HDR_LEN);
    --hdr_data.flow_seq; // Modify expected seq. (IPFIX Message should ignore missing records)
    cmp_header(msg_data, hdr_data, VALUE_ODID);

    // Expect Data Set
    fds_sets_iter_init(&it_sets, msg_hdr);
    ASSERT_EQ(fds_sets_iter_next(&it_sets), FDS_OK);
    uint16_t set_id = ntohs(it_sets.set->flowset_id);
    ASSERT_GE(set_id, FDS_IPFIX_SET_MIN_DSET);

    // No more Data Records and Sets
    EXPECT_EQ(fds_sets_iter_next(&it_sets), FDS_EOC);
}

/* Reorder messages (unexpected sequence number)
 * It's expected that converted messages don't contain information about reordering
 */
TEST_F(MsgBase, reorderMessages)
{
    uint8_t *msg_data = nullptr;
    uint16_t msg_size = 0;
    struct fds_sets_iter it_sets;
    struct fds_dset_iter it_dset;
    struct ipx_msg_ctx msg_ctx;

    // Create a converter
    const uint32_t VALUE_ODID = 5679UL;
    const uint32_t VALUE_TMPLT_REF = 5; // every 5 seconds
    converter_create(VALUE_ODID, VALUE_TMPLT_REF, IPX_VERB_DEBUG);

    // Create a message
    struct msg_data_hdr hdr_data; // default header data
    struct msg_data_hdr ipx_hdr_exp = hdr_data;
    struct msg_data_rec rec_data; // default record data
    msg_create(&msg_data, &msg_size, hdr_data);
    msg_rec_add(&msg_data, &msg_size, rec_data); // Message with 1 flow record
    msg_ctx = {m_session.get(), VALUE_ODID, 0};
    prepare_msg(&msg_ctx, msg_data, msg_size);

    // Convert the message (just to learn the converter current sequence number)
    ASSERT_EQ(ipx_nf5_conv_process(m_conv.get(), m_msg.get()), IPX_OK);

    // Try to parse the IPFIX Message
    msg_data = ipx_msg_ipfix_get_packet(m_msg.get());
    auto msg_hdr = reinterpret_cast<fds_ipfix_msg_hdr *>(msg_data);
    msg_size = ntohs(msg_hdr->length);
    ASSERT_GE(msg_size, FDS_IPFIX_MSG_HDR_LEN);
    cmp_header(msg_data, ipx_hdr_exp, VALUE_ODID);

    // Expect Template Set
    fds_sets_iter_init(&it_sets, msg_hdr);
    struct fds_template *tmplt_parsed = nullptr;
    parse_tset(it_sets, &tmplt_parsed);
    std::unique_ptr<struct fds_template, decltype(&fds_template_destroy)>
        tmplt(tmplt_parsed, &fds_template_destroy);

    // Expect also Data Set
    ASSERT_EQ(fds_sets_iter_next(&it_sets), FDS_OK);
    uint16_t set_id = ntohs(it_sets.set->flowset_id);
    ASSERT_GE(set_id, FDS_IPFIX_SET_MIN_DSET);
    ASSERT_EQ(set_id, tmplt->id);
    fds_dset_iter_init(&it_dset, it_sets.set, tmplt.get());
    ASSERT_EQ(fds_dset_iter_next(&it_dset), FDS_OK);
    struct fds_drec drec = {it_dset.rec, it_dset.size, tmplt.get(), nullptr};
    cmp_rec(drec, hdr_data, rec_data);

    // No more Data Records and Sets
    EXPECT_EQ(fds_dset_iter_next(&it_dset), FDS_EOC);
    EXPECT_EQ(fds_sets_iter_next(&it_sets), FDS_EOC);

    // Try to parse another message (unexpected sequence number from the past) -----------------
    hdr_data.flow_seq -= 1;  // 1 record in history
    ipx_hdr_exp.flow_seq += 1; // IPFIX Message should ignore invalid sequence number
    // Slightly change the record
    rec_data.proto = 6;
    rec_data.delta_octets = 1201;
    rec_data.delta_pkts = 3;

    msg_create(&msg_data, &msg_size, hdr_data);
    msg_rec_add(&msg_data, &msg_size, rec_data); // Message with 1 flow record again
    msg_ctx = {m_session.get(), VALUE_ODID, 0};
    prepare_msg(&msg_ctx, msg_data, msg_size);

    // Convert the message
    ASSERT_EQ(ipx_nf5_conv_process(m_conv.get(), m_msg.get()), IPX_OK);

    // Try to parse the IPFIX Message
    msg_data = ipx_msg_ipfix_get_packet(m_msg.get());
    msg_hdr = reinterpret_cast<fds_ipfix_msg_hdr *>(msg_data);
    msg_size = ntohs(msg_hdr->length);
    ASSERT_GE(msg_size, FDS_IPFIX_MSG_HDR_LEN);
    cmp_header(msg_data, ipx_hdr_exp, VALUE_ODID);

    // Expect Data Set (only)
    fds_sets_iter_init(&it_sets, msg_hdr);
    ASSERT_EQ(fds_sets_iter_next(&it_sets), FDS_OK);
    set_id = ntohs(it_sets.set->flowset_id);
    ASSERT_GE(set_id, FDS_IPFIX_SET_MIN_DSET);
    ASSERT_EQ(set_id, tmplt->id);
    fds_dset_iter_init(&it_dset, it_sets.set, tmplt.get());
    ASSERT_EQ(fds_dset_iter_next(&it_dset), FDS_OK);
    drec = {it_dset.rec, it_dset.size, tmplt.get(), nullptr};
    cmp_rec(drec, hdr_data, rec_data);

    // No more Data Records and Sets
    EXPECT_EQ(fds_sets_iter_next(&it_sets), FDS_EOC);
    EXPECT_EQ(fds_sets_iter_next(&it_sets), FDS_EOC);

    // Try to parse another message (expected sequence number from the future) ---------------
    hdr_data.flow_seq += 2;  // back to the expected sequence number
    ipx_hdr_exp.flow_seq += 1; // IPFIX Message should ignore invalid sequence number

    msg_create(&msg_data, &msg_size, hdr_data);
    msg_rec_add(&msg_data, &msg_size, rec_data); // Message with 1 flow record again
    msg_ctx = {m_session.get(), VALUE_ODID, 0};
    prepare_msg(&msg_ctx, msg_data, msg_size);

    // Convert the message
    ASSERT_EQ(ipx_nf5_conv_process(m_conv.get(), m_msg.get()), IPX_OK);

    // Try to parse the IPFIX Message
    msg_data = ipx_msg_ipfix_get_packet(m_msg.get());
    msg_hdr = reinterpret_cast<fds_ipfix_msg_hdr *>(msg_data);
    msg_size = ntohs(msg_hdr->length);
    ASSERT_GE(msg_size, FDS_IPFIX_MSG_HDR_LEN);
    cmp_header(msg_data, ipx_hdr_exp, VALUE_ODID);

    // Expect Data Set (only)
    fds_sets_iter_init(&it_sets, msg_hdr);
    ASSERT_EQ(fds_sets_iter_next(&it_sets), FDS_OK);
    set_id = ntohs(it_sets.set->flowset_id);
    ASSERT_GE(set_id, FDS_IPFIX_SET_MIN_DSET);
    ASSERT_EQ(set_id, tmplt->id);
    fds_dset_iter_init(&it_dset, it_sets.set, tmplt.get());
    ASSERT_EQ(fds_dset_iter_next(&it_dset), FDS_OK);
    drec = {it_dset.rec, it_dset.size, tmplt.get(), nullptr};
    cmp_rec(drec, hdr_data, rec_data);

    // No more Data Records and Sets
    EXPECT_EQ(fds_sets_iter_next(&it_sets), FDS_EOC);
    EXPECT_EQ(fds_sets_iter_next(&it_sets), FDS_EOC);
}

// Try to convert a message with unexpected header version
TEST_F(MsgBase, invalidVersionNumber)
{
    uint8_t *msg_data = nullptr;
    uint16_t msg_size = 0;
    struct ipx_msg_ctx msg_ctx;

    // Create a converter
    const uint32_t VALUE_ODID = 12UL;
    const uint32_t VALUE_TMPLT_REF = 0; // every 5 seconds
    converter_create(VALUE_ODID, VALUE_TMPLT_REF, IPX_VERB_DEBUG);

    // Create a message
    struct msg_data_hdr hdr_data; // default header data
    hdr_data.version = 9; // invalid version
    struct msg_data_rec rec_data; // default record data
    msg_create(&msg_data, &msg_size, hdr_data);
    msg_rec_add(&msg_data, &msg_size, rec_data); // Message with 1 flow record
    msg_ctx = {m_session.get(), VALUE_ODID, 0};
    prepare_msg(&msg_ctx, msg_data, msg_size);

    // Create a backup copy
    std::unique_ptr<uint8_t[]> msg_cpy_data(new uint8_t[msg_size]);
    memcpy(msg_cpy_data.get(), msg_data, msg_size);
    uint16_t msg_cpy_size = msg_size;

    // Convert the message
    EXPECT_EQ(ipx_nf5_conv_process(m_conv.get(), m_msg.get()), IPX_ERR_FORMAT);

    // Check if the original message hasn't been changed
    msg_data = ipx_msg_ipfix_get_packet(m_msg.get());
    EXPECT_EQ(memcmp(msg_data, msg_cpy_data.get(), msg_cpy_size), 0);
}

// Try to convert a message with number of records doesn't matching information in the header
TEST_F(MsgBase, invalidNumberOfRecords)
{
    uint8_t *msg_data = nullptr;
    uint16_t msg_size = 0;
    struct ipx_msg_ctx msg_ctx;

    // Create a converter
    const uint32_t VALUE_ODID = 12UL;
    const uint32_t VALUE_TMPLT_REF = 0; // every 5 seconds
    converter_create(VALUE_ODID, VALUE_TMPLT_REF, IPX_VERB_DEBUG);

    // Create a message with less records than expected -----------------------
    struct msg_data_hdr hdr_data; // default header data
    struct msg_data_rec rec_data; // default record data
    msg_create(&msg_data, &msg_size, hdr_data);
    msg_rec_add(&msg_data, &msg_size, rec_data); // Message with 3 flow record
    msg_rec_add(&msg_data, &msg_size, rec_data);
    msg_rec_add(&msg_data, &msg_size, rec_data);
    // Change value of the counter
    auto nf5_data = reinterpret_cast<struct ipx_nf5_hdr *>(msg_data);
    nf5_data->count = htons(4);

    msg_ctx = {m_session.get(), VALUE_ODID, 0};
    prepare_msg(&msg_ctx, msg_data, msg_size);
    EXPECT_EQ(ipx_nf5_conv_process(m_conv.get(), m_msg.get()), IPX_ERR_FORMAT);

    // Create a message with more records than expected -----------------------
    msg_create(&msg_data, &msg_size, hdr_data);
    msg_rec_add(&msg_data, &msg_size, rec_data); // Message with 3 flow record
    msg_rec_add(&msg_data, &msg_size, rec_data);
    msg_rec_add(&msg_data, &msg_size, rec_data);
    // Change value of the counter
    nf5_data = reinterpret_cast<struct ipx_nf5_hdr *>(msg_data);
    nf5_data->count = htons(2);

    msg_ctx = {m_session.get(), VALUE_ODID, 0};
    prepare_msg(&msg_ctx, msg_data, msg_size);
    EXPECT_EQ(ipx_nf5_conv_process(m_conv.get(), m_msg.get()), IPX_ERR_FORMAT);
}

// Try to convert a message with unexpected end of its body or header
TEST_F(MsgBase, unexpectedEndOfMessge)
{
    uint8_t *msg_data = nullptr;
    uint16_t msg_size = 0;
    struct ipx_msg_ctx msg_ctx;

    // Create a converter
    const uint32_t VALUE_ODID = 12UL;
    const uint32_t VALUE_TMPLT_REF = 0; // every 5 seconds
    converter_create(VALUE_ODID, VALUE_TMPLT_REF, IPX_VERB_DEBUG);

    // Create a message
    struct msg_data_hdr hdr_data; // default header data
    struct msg_data_rec rec_data; // default record data
    msg_create(&msg_data, &msg_size, hdr_data);
    for (size_t i = 0; i < 30; ++i) {
        msg_rec_add(&msg_data, &msg_size, rec_data);
    }

    // Create a copy of the message
    std::unique_ptr<uint8_t[]> msg_cpy_data(new uint8_t[msg_size]);
    memcpy(msg_cpy_data.get(), msg_data, msg_size);
    uint16_t msg_cpy_size = msg_size;

    msg_ctx = {m_session.get(), VALUE_ODID, 0};
    prepare_msg(&msg_ctx, msg_data, msg_size);
    // Make sure that the message can be successfully converted
    ASSERT_EQ(ipx_nf5_conv_process(m_conv.get(), m_msg.get()), IPX_OK);

    // Now try to convert all possible messages with invalid size (less than expected)
    for (size_t i = 0; i < msg_cpy_size; ++i) {
        SCOPED_TRACE("i: " + std::to_string(i));
        msg_data = (uint8_t *) malloc(i * sizeof(uint8_t)); // malloc is used inside C API
        ASSERT_NE(msg_data, nullptr);
        memcpy(msg_data, msg_cpy_data.get(), i);
        msg_size = i;

        // Must always fail
        prepare_msg(&msg_ctx, msg_data, msg_size);
        ASSERT_EQ(ipx_nf5_conv_process(m_conv.get(), m_msg.get()), IPX_ERR_FORMAT);
    }
}

/* Try to convert very long NetFlow message to IPFIX
 * Size of NetFlow message is usually up to 1500 bytes (MTU), however, the converter should be
 * able to convert any unusual valid message. We expect that the converter adds extra data to
 * the converted IPFIX message (such as Template definitions, information about flow sampling, etc.)
 * that it's possible it should not be able to convert records. So let's try converter maximum
 * possible NetFlow message and expect failure.
 * Note: NetFlow 5 hdr: 24B, NetFlow 5 rec: 48B -> max 1364 records in a single NetFlow message.
 */
TEST_F(MsgBase, tooLongToConverter)
{
    uint8_t *msg_data = nullptr;
    uint16_t msg_size = 0;
    struct ipx_msg_ctx msg_ctx;

    // Create a converter
    const uint32_t VALUE_ODID = 1UL;
    const uint32_t VALUE_TMPLT_REF = 10; // every 5 seconds
    converter_create(VALUE_ODID, VALUE_TMPLT_REF, IPX_VERB_DEBUG);

    // Create a message
    struct msg_data_hdr hdr_data; // default header data
    struct msg_data_rec rec_data; // default record data
    msg_create(&msg_data, &msg_size, hdr_data);
    for (size_t i = 0; i < 1364; ++i) {
        msg_rec_add(&msg_data, &msg_size, rec_data);
    }

    msg_ctx = {m_session.get(), VALUE_ODID, 0};
    prepare_msg(&msg_ctx, msg_data, msg_size);
    EXPECT_EQ(ipx_nf5_conv_process(m_conv.get(), m_msg.get()), IPX_ERR_FORMAT);
}

#include <arpa/inet.h>
#include <gtest/gtest.h>
#include <memory>

#include <ipfixcol2.h>
#include <libfds/ipfix_parsers.h>
#include <MsgGen.h>
#include <libfds.h>

extern "C" {
#include <core/netflow2ipfix/netflow2ipfix.h>
#include <core/netflow2ipfix/netflow_structs.h>
#include <core/context.h>
}

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

class MsgBase : public ::testing::Test {
protected:
    /// Before each Test case
    void SetUp() override
    {
        context_create();
        session_create();
        converter_create(IPX_VERB_DEBUG);
    }

    /// After each Test case
    void TearDown() override
    {
    }

    /// Create fake plugin context (i.e. without function callbacks)
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
     * @param[in] verb  Verbosity level of the converter
     */
    void
    converter_create(ipx_verb_level verb = IPX_VERB_NONE)
    {
        const ::testing::TestInfo* const test_info =
            ::testing::UnitTest::GetInstance()->current_test_info();
        std::string name = std::string(test_info->name()) + " (NFv9 -> IPFIX converter)";
        m_conv.reset(ipx_nf9_conv_init(name.c_str(), verb));
        ASSERT_NE(m_conv, nullptr);
    }

    void
    prepare_msg(const struct ipx_msg_ctx *msg_ctx, uint8_t *msg_data, uint16_t msg_size)
    {
        m_msg.reset(ipx_msg_ipfix_create(m_ctx.get(), msg_ctx, msg_data, msg_size));
        ASSERT_NE(m_msg, nullptr);
    }

    std::unique_ptr<struct fds_template, decltype(&fds_template_destroy)>
    parse_template(const fds_tset_iter &it, enum fds_template_type type)
    {
        uint16_t tmplt_size = it.size;
        struct fds_template *tmplt_parsed = nullptr;
        if (fds_template_parse(type, it.ptr.trec, &tmplt_size, &tmplt_parsed)
                != FDS_OK) {
            throw std::runtime_error("Failed to parse a template!");
        }
        std::unique_ptr<struct fds_template, decltype(&fds_template_destroy)>
            tmplt(tmplt_parsed, &fds_template_destroy);
        return std::move(tmplt);

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
    std::unique_ptr<ipx_nf9_conv_t, decltype(&ipx_nf9_conv_destroy)>
        m_conv = {nullptr, &ipx_nf9_conv_destroy};
};

class Rec_base {
protected:
    Rec_base() = default;
    ~Rec_base() = default;

    std::unique_ptr<nf9_trec> m_trec = nullptr;
    std::unique_ptr<nf9_drec> m_drec = nullptr;
    uint16_t m_tid;           ///< Template ID
    uint16_t m_scope_cnt = 0; ///< Number of scope fields
    uint16_t m_ipx_dsize = 0; ///< Size of IPFIX Data Record

    // Data type of a field
    enum class item_type {
        UINT,
        INT,
        DOUBLE,
        IP,
        STR,
        TIME,
        OCTETS
    };

    // Content of a field
    union item_data {
        uint64_t val_uint;
        int64_t  val_int;
        double   val_double;
        uint8_t  val_ipv4[4];
        uint8_t  val_ipv6[16];
        char     val_str[64];
        uint8_t  val_octets[64];
    };

    // Description of a field
    struct item_info {
        uint16_t nf_id;
        uint16_t nf_len;
        uint16_t ipx_id;
        uint32_t ipx_en;
        uint16_t ipx_len;
        item_type type;
        item_data data;
    };

    static constexpr uint16_t SAME_LEN = 65535;
    static constexpr uint16_t SAME_ID = 65535;

    std::vector<item_info> m_items;

    void
    add_field_uint(uint16_t nf_id, uint16_t nf_len, uint64_t val, uint16_t ipx_id = SAME_ID,
        uint32_t ipx_en = 0, uint16_t ipx_len = SAME_LEN)
    {
        ipx_id = (ipx_id == SAME_ID) ? nf_id : ipx_id;
        ipx_len = (ipx_len == SAME_LEN) ? nf_len : ipx_len;

        item_info info = {nf_id, nf_len, ipx_id, ipx_en, ipx_len, item_type::UINT, {}};
        info.data.val_uint = val;
        m_items.push_back(info);
    }

    void
    add_field_int(uint16_t nf_id, uint16_t nf_len, int64_t val, uint16_t ipx_id = SAME_ID,
        uint32_t ipx_en = 0, uint16_t ipx_len = SAME_LEN)
    {
        ipx_id = (ipx_id == SAME_ID) ? nf_id : ipx_id;
        ipx_len = (ipx_len == SAME_LEN) ? nf_len : ipx_len;

        item_info info = {nf_id, nf_len, ipx_id, ipx_en, ipx_len, item_type::INT, {}};
        info.data.val_int = val;
        m_items.push_back(info);
    }

    void
    add_field_double(uint16_t nf_id, uint16_t nf_len, double val, uint16_t ipx_id = SAME_ID,
        uint32_t ipx_en = 0, uint16_t ipx_len = SAME_LEN)
    {
        ipx_id = (ipx_id == SAME_ID) ? nf_id : ipx_id;
        ipx_len = (ipx_len == SAME_LEN) ? nf_len : ipx_len;

        item_info info = {nf_id, nf_len, ipx_id, ipx_en, ipx_len, item_type::DOUBLE, {}};
        info.data.val_double = val;
        m_items.push_back(info);
    }

    void
    add_field_ip(uint16_t nf_id, uint16_t nf_len, const std::string &str, uint16_t ipx_id = SAME_ID,
        uint32_t ipx_en = 0, uint16_t ipx_len = SAME_LEN)
    {
        ipx_id = (ipx_id == SAME_ID) ? nf_id : ipx_id;
        ipx_len = (ipx_len == SAME_LEN) ? nf_len : ipx_len;

        item_info info = {nf_id, nf_len, ipx_id, ipx_en, ipx_len, item_type::IP, {}};
        if (inet_pton(AF_INET, str.c_str(), &info.data.val_ipv4) == 1
                || inet_pton(AF_INET6, str.c_str(), &info.data.val_ipv6) == 1) {
            m_items.push_back(info);
            return;
        }

        throw std::invalid_argument("Invalid IP address");
    }

    void
    add_field_string(uint16_t nf_id, uint16_t nf_len, const std::string &val,
        uint16_t ipx_id = SAME_ID, uint32_t ipx_en = 0, uint16_t ipx_len = SAME_LEN)
    {
        ipx_id = (ipx_id == SAME_ID) ? nf_id : ipx_id;
        ipx_len = (ipx_len == SAME_LEN) ? nf_len : ipx_len;

        item_info info = {nf_id, nf_len, ipx_id, ipx_en, ipx_len, item_type::STR, {}};
        const size_t max_size = sizeof(info.data.val_str);
        memset(info.data.val_str, 0, max_size);
        strncpy(info.data.val_str, val.c_str(), max_size - 1);
        m_items.push_back(info);
    }

    void
    add_field_time(uint16_t nf_id, uint16_t nf_len, uint64_t val, uint16_t ipx_id = SAME_ID,
        uint32_t ipx_en = 0, uint16_t ipx_len = SAME_LEN)
    {
        add_field_uint(nf_id, nf_len, val, ipx_id, ipx_en, ipx_len);
        m_items.back().type = item_type::TIME;
    }

    void
    add_field_octets(uint16_t nf_id, uint16_t nf_len, const uint8_t *val,
        uint16_t ipx_id = SAME_ID, uint32_t ipx_en = 0, uint16_t ipx_len = SAME_LEN)
    {
        ipx_id = (ipx_id == SAME_ID) ? nf_id : ipx_id;
        ipx_len = (ipx_len == SAME_LEN) ? nf_len : ipx_len;

        item_info info = {nf_id, nf_len, ipx_id, ipx_en, ipx_len, item_type::OCTETS, {}};
        const size_t max_octets = sizeof(info.data.val_octets);
        if (max_octets < nf_len) {
            throw std::invalid_argument("Too long octet array!");
        }
        memcpy(info.data.val_octets, val, nf_len);
        m_items.push_back(info);
    }

    void
    build(uint16_t tid, uint16_t scope_cnt = 0)
    {
        // Build NetFlow v9 Template Record
        m_tid = tid;
        if (scope_cnt == 0) {
            m_trec.reset(new nf9_trec(tid));
        } else {
            m_trec.reset(new nf9_trec(tid, scope_cnt));
            m_scope_cnt = scope_cnt;
        }

        for (const auto &item : m_items) {
            m_trec->add_field(item.nf_id, item.nf_len);
        }

        // Build NetFlow v9 Data Record
        m_drec.reset(new nf9_drec());
        m_ipx_dsize = 0;

        for (const auto &item : m_items) {
            m_ipx_dsize += item.ipx_len;

            switch(item.type) {
            case item_type::UINT:
                m_drec->append_uint(item.data.val_uint, item.nf_len);
                break;
            case item_type::INT:
                m_drec->append_int(item.data.val_int, item.nf_len);
                break;
            case item_type::DOUBLE:
                m_drec->append_float(item.data.val_double, item.nf_len);
                break;
            case item_type::IP:
                if (item.nf_len == 4U) {
                    m_drec->append_octets(item.data.val_ipv4, 4U);
                } else {
                    m_drec->append_octets(item.data.val_ipv4, 16U);
                }
                break;
            case item_type::TIME:
                m_drec->append_uint(item.data.val_uint, item.nf_len);
                break;
            case item_type::STR:
                m_drec->append_string(std::string(item.data.val_str), item.nf_len);
                break;
            case item_type::OCTETS:
                m_drec->append_octets(item.data.val_octets, item.nf_len);
                break;
            default:
                FAIL() << "Unimplemented type!";
            }
        }
    }

public:
    /**
     * @brief Compare with converted IPFIX Data Record
     * @param[in] rec         Converted IPFIX Data Record
     * @param[in] nf9_exp_sec Original NetFlow v9 Export timestamp (in seconds)
     * @param[in] nf9_uptime  Original NetFlow v9 SysUptime (in milliseconds)
     */
    void
    compare_data(struct fds_drec *rec, uint32_t nf9_exp_sec, uint32_t nf9_uptime)
    {
        // Size of the Data Record
        ASSERT_EQ(rec->size, m_ipx_dsize);
        struct fds_drec_iter iter;

        auto cmp_uint = [&iter](const struct item_info *info) {
            uint64_t tmp_value;
            ASSERT_EQ(iter.field.size, info->ipx_len);
            ASSERT_EQ(fds_get_uint_be(iter.field.data, iter.field.size, &tmp_value), FDS_OK);
            ASSERT_EQ(tmp_value, info->data.val_uint);
        };
        auto cmp_int = [&iter](const struct item_info *info) {
            int64_t tmp_value;
            ASSERT_EQ(iter.field.size, info->ipx_len);
            ASSERT_EQ(fds_get_int_be(iter.field.data, iter.field.size, &tmp_value), FDS_OK);
            ASSERT_EQ(tmp_value, info->data.val_int);
        };
        auto cmp_double = [&iter](const struct item_info *info) {
            double tmp_value;
            ASSERT_EQ(iter.field.size, info->ipx_len);
            ASSERT_EQ(fds_get_float_be(iter.field.data, iter.field.size, &tmp_value), FDS_OK);
            ASSERT_DOUBLE_EQ(tmp_value, info->data.val_double);
        };
        auto cmp_ip = [&iter](const struct item_info *info) {
            uint8_t tmp_value[16];
            ASSERT_EQ(iter.field.size, info->ipx_len);
            ASSERT_EQ(fds_get_ip(iter.field.data, iter.field.size, tmp_value), FDS_OK);
            ASSERT_EQ(memcmp(tmp_value, info->data.val_ipv4, iter.field.size), 0U);
        };
        auto cmp_str = [&iter](const struct item_info *info) {
            ASSERT_EQ(iter.field.size, info->ipx_len);
            ASSERT_EQ(memcmp(iter.field.data, info->data.val_str, iter.field.size), 0U);
        };
        auto cmp_time = [&iter, nf9_exp_sec, nf9_uptime](const struct item_info *info) {
            uint64_t ipx_ts; // milliseconds since Epoch
            ASSERT_EQ(iter.field.size, info->ipx_len);
            ASSERT_EQ(fds_get_datetime_lp_be(iter.field.data, iter.field.size,
                FDS_ET_DATE_TIME_MILLISECONDS, &ipx_ts) , FDS_OK);

            uint64_t sys_time = nf9_exp_sec * 1000ULL;
            uint64_t sys_uptime = nf9_uptime;
            uint64_t field_uptime = info->data.val_uint;
            uint64_t time_since_utc = sys_time - (sys_uptime - field_uptime);
            ASSERT_EQ(time_since_utc, ipx_ts);
        };
        auto cmp_octets = [&iter](const struct item_info *info) {
            ASSERT_EQ(iter.field.size, info->ipx_len);
            ASSERT_EQ(memcmp(iter.field.data, info->data.val_octets, iter.field.size), 0U);
        };

        fds_drec_iter_init(&iter, rec, FDS_DREC_PADDING_SHOW);
        for (size_t i = 0; i < m_items.size(); ++i) {
            SCOPED_TRACE("i: " + std::to_string(i));
            ASSERT_GE(fds_drec_iter_next(&iter), 0);

            const struct item_info *info = &m_items[i];
            switch (info->type) {
            case item_type::UINT:
                cmp_uint(info);
                break;
            case item_type::INT:
                cmp_int(info);
                break;
            case item_type::DOUBLE:
                cmp_double(info);
                break;
            case item_type::IP:
                cmp_ip(info);
                break;
            case item_type::STR:
                cmp_str(info);
                break;
            case item_type::TIME:
                cmp_time(info);
                break;
            case item_type::OCTETS:
                cmp_octets(info);
                break;
            default:
                FAIL() << "Unimplemented type!";
            }

        }

        EXPECT_EQ(fds_drec_iter_next(&iter), FDS_EOC);
    }

    /**
     * \brief Compare with converted IPFIX Template
     * \param[in] tmplt Converted and parsed IPFIX Template
     */
    void
    compare_template(struct fds_template *tmplt)
    {
        // Check the header
        ASSERT_EQ(tmplt->id, m_tid);
        ASSERT_EQ(tmplt->fields_cnt_total, m_items.size());
        ASSERT_EQ(tmplt->fields_cnt_scope, m_scope_cnt);

        for (size_t i = 0; i < m_items.size(); ++i) {
            SCOPED_TRACE("i: " + std::to_string(i));
            const struct fds_tfield *field = &tmplt->fields[i];
            const struct item_info *item = &m_items[i];
            EXPECT_EQ(field->en, item->ipx_en);
            EXPECT_EQ(field->id, item->ipx_id);
            EXPECT_EQ(field->length, item->ipx_len);
        }
    }

    const nf9_trec &
    get_nf9_template() {return *m_trec;};
    const nf9_drec &
    get_nf9_record() {return *m_drec;};
};

// -----------------------------------------------------------------------------

/// Basic flow record with combination of timestamp and non-timestamp data
class Rec_norm_basic : public Rec_base {
public:
    Rec_norm_basic(uint16_t tid)
    {
        // Define structure and content of the record
        add_field_uint(IPX_NF9_IE_IN_BYTES,       4, VAL_BYTES);
        add_field_uint(IPX_NF9_IE_IN_PKTS,        4, VAL_PKTS);
        add_field_ip(IPX_NF9_IE_IPV4_SRC_ADDR,    4, VAL_SRC_IP);
        add_field_ip(IPX_NF9_IE_IPV4_DST_ADDR,    4, VAL_DST_IP);
        add_field_time(IPX_NF9_IE_FIRST_SWITCHED, 4, VAL_TS_START, 152, 0, 8U); // new size and ID
        add_field_time(IPX_NF9_IE_LAST_SWITCHED,  4, VAL_TS_END, 153, 0, 8U); // new size and ID
        add_field_uint(IPX_NF9_IE_L4_SRC_PORT,    2, VAL_SRC_PORT);
        add_field_uint(IPX_NF9_IE_L4_DST_PORT,    2, VAL_DST_PORT);
        // Build Template and Data Record
        build(tid);
    }

    const uint32_t VAL_BYTES = 123456;
    const uint32_t VAL_PKTS = 254;
    const char *   VAL_SRC_IP = "127.0.0.1";
    const char *   VAL_DST_IP = "127.0.10.1";
    const uint32_t VAL_TS_START = 5000;
    const uint32_t VAL_TS_END = 7897;
    const uint16_t VAL_SRC_PORT = 60121;
    const uint16_t VAL_DST_PORT = 53;
};

/// Basic flow record with only non-timestamp data
class Rec_norm_nots : public Rec_base {
public:
    Rec_norm_nots(uint16_t tid)
    {
        add_field_uint(IPX_NF9_IE_IN_BYTES,       4, VAL_BYTES);
        add_field_uint(IPX_NF9_IE_IN_PKTS,        4, VAL_PKTS);
        add_field_ip(IPX_NF9_IE_IPV4_SRC_ADDR,    4, VAL_SRC_IP);
        add_field_ip(IPX_NF9_IE_IPV4_DST_ADDR,    4, VAL_DST_IP);
        add_field_uint(IPX_NF9_IE_L4_SRC_PORT,    2, VAL_SRC_PORT);
        add_field_uint(IPX_NF9_IE_L4_DST_PORT,    2, VAL_DST_PORT);
        // Build Template and Data Record
        build(tid);
    }

    const uint32_t VAL_BYTES = 100;
    const uint32_t VAL_PKTS = 2;
    const char *   VAL_SRC_IP = "255.255.0.1";
    const char *   VAL_DST_IP = "1.1.1.1";
    const uint16_t VAL_SRC_PORT = 5251;
    const uint16_t VAL_DST_PORT = 28297;
};

/// Basic flow record with only timestamp data
class Rec_norm_onlyts : public Rec_base {
public:
    Rec_norm_onlyts(uint16_t tid) {
        add_field_time(IPX_NF9_IE_FIRST_SWITCHED, 4, VAL_TS_START, 152, 0, 8U); // new size and ID
        add_field_time(IPX_NF9_IE_LAST_SWITCHED,  4, VAL_TS_END, 153, 0, 8U); // new size and ID
        // Build Template and Data Record
        build(tid);
    }

    const uint32_t VAL_TS_START = 873214UL;
    const uint32_t VAL_TS_END =   989772UL;
};

/// Basic flow record with multiple occurrences of the same fields
class Rec_norm_multi : public Rec_base {
public:
    Rec_norm_multi(uint16_t tid)
    {
        add_field_time(IPX_NF9_IE_FIRST_SWITCHED, 4, VAL_TS_START1, 152, 0, 8U); // new size and ID
        add_field_uint(IPX_NF9_IE_IN_BYTES,       4, VAL_BYTES);
        add_field_uint(IPX_NF9_IE_IN_PKTS,        4, VAL_PKTS);
        add_field_time(IPX_NF9_IE_LAST_SWITCHED,  4, VAL_TS_END1, 153, 0, 8U); // new size and ID
        add_field_ip(IPX_NF9_IE_IPV4_SRC_ADDR,    4, VAL_SRC_IP1);
        add_field_ip(IPX_NF9_IE_IPV4_DST_ADDR,    4, VAL_DST_IP1);
        add_field_uint(IPX_NF9_IE_L4_SRC_PORT,    2, VAL_SRC_PORT);
        add_field_uint(IPX_NF9_IE_PROTOCOL,       1, VAL_PROTO);
        add_field_time(IPX_NF9_IE_FIRST_SWITCHED, 4, VAL_TS_START2, 152, 0, 8U); // new size and ID
        add_field_ip(IPX_NF9_IE_IPV4_SRC_ADDR,    4, VAL_SRC_IP2);
        add_field_uint(IPX_NF9_IE_MPLS_LABEL_1,   3, VAL_MPLS_1);
        add_field_ip(IPX_NF9_IE_IPV4_DST_ADDR,    4, VAL_DST_IP2);
        add_field_time(IPX_NF9_IE_LAST_SWITCHED,  4, VAL_TS_END2, 153, 0, 8U); // new size and ID
        add_field_uint(IPX_NF9_IE_MPLS_LABEL_2,   3, VAL_MPLS_2);
        add_field_time(IPX_NF9_IE_FIRST_SWITCHED, 4, VAL_TS_START3, 152, 0, 8U); // new size and ID
        add_field_uint(IPX_NF9_IE_L4_DST_PORT,    2, VAL_DST_PORT);
        add_field_ip(IPX_NF9_IE_IPV6_DST_ADDR,   16, VAL_SRC_IP3);
        add_field_string(IPX_NF9_IE_APPLICATION_NAME, 32, std::string(VAL_APP_NAME));
        // Build Template and Data Record
        build(tid);
    }

    const uint32_t VAL_TS_START1 = 21;
    const uint32_t VAL_BYTES = 1234562892UL;
    const uint32_t VAL_PKTS = 19291821UL;
    const uint32_t VAL_TS_END1 = 90821;
    const char *   VAL_SRC_IP1 = "192.168.1.9";
    const char *   VAL_DST_IP1 = "192.168.2.1";
    const uint16_t VAL_SRC_PORT = 65000;
    const uint8_t  VAL_PROTO = 17;
    const uint32_t VAL_TS_START2 = 2002;
    const char *   VAL_SRC_IP2 = "10.10.10.20";
    const uint32_t VAL_MPLS_1 = 221;
    const char *   VAL_DST_IP2 = "10.20.30.40";
    const uint32_t VAL_TS_END2 = 29918;
    const uint32_t VAL_MPLS_2 = 222;
    const uint32_t VAL_TS_START3 = 10921;
    const uint16_t VAL_DST_PORT = 80;
    const char *   VAL_SRC_IP3 = "fe80::ffff:204.152.189.116";
    const char *   VAL_APP_NAME = "firefox";
};

/// Basic flow record with non-compatible field specifiers (ID > 127)
class Rec_norm_enterprise : public Rec_base {
public:
    Rec_norm_enterprise(uint16_t tid)
    {
        // Define structure and content of the record
        add_field_uint(IPX_NF9_IE_IN_BYTES,       4, VAL_BYTES);
        add_field_uint(IPX_NF9_IE_IN_PKTS,        4, VAL_PKTS);
        add_field_ip(IPX_NF9_IE_IPV4_SRC_ADDR,    4, VAL_SRC_IP);
        add_field_ip(IPX_NF9_IE_IPV4_DST_ADDR,    4, VAL_DST_IP);
        add_field_time(IPX_NF9_IE_FIRST_SWITCHED, 4, VAL_TS_START, 152, 0, 8U); // new size and ID
        add_field_uint(400,                       4, VAL_EN_NUM1, SAME_ID, EN_LOW);
        add_field_ip(40000,                       4, VAL_EN_IP,   7232,    EN_HIGH);
        add_field_time(IPX_NF9_IE_LAST_SWITCHED,  4, VAL_TS_END, 153, 0, 8U); // new size and ID
        add_field_uint(IPX_NF9_IE_L4_SRC_PORT,    2, VAL_SRC_PORT);
        add_field_uint(IPX_NF9_IE_L4_DST_PORT,    2, VAL_DST_PORT);
        add_field_int(8000,                       4, VAL_EN_NUM2, SAME_ID, EN_LOW);
        // Build Template and Data Record
        build(tid);
    }

    const uint32_t EN_LOW = 4294967294UL;
    const uint32_t EN_HIGH = 4294967295UL;

    const uint32_t VAL_BYTES = 123456221UL;
    const uint32_t VAL_PKTS = 252987324UL;
    const char *   VAL_SRC_IP = "8.8.8.8";
    const char *   VAL_DST_IP = "1.1.1.1";
    const uint32_t VAL_TS_START = 50000;
    const uint32_t VAL_TS_END = 78970;
    const uint16_t VAL_SRC_PORT = 60121;
    const uint16_t VAL_DST_PORT = 53;

    const uint32_t VAL_EN_NUM1 = 292182U;
    const int32_t  VAL_EN_NUM2 = 21;
    const char *   VAL_EN_IP = "224.255.0.0";
};

/// Options record with single scope field
class Rec_opts_simple : public Rec_base {
public:
    Rec_opts_simple(uint16_t tid)
    {
        add_field_uint(IPX_NF9_SCOPE_SYSTEM, 4, VAL_SCOPE_SYSTEM, 144U); // iana:exportingProcessId
        add_field_uint(IPX_NF9_IE_TOTAL_BYTES_EXP, 8, VAL_TOTAL_BYTES);
        add_field_uint(IPX_NF9_IE_TOTAL_PKTS_EXP,  8, VAL_TOTAL_PKTS);
        add_field_uint(IPX_NF9_IE_TOTAL_FLOWS_EXP, 4, VAL_TOTAL_FLOWS);
        // Build Template and Data Record
        build(tid, 1);
    }

    const uint32_t VAL_SCOPE_SYSTEM = 32;
    const uint64_t VAL_TOTAL_BYTES = 82202029183ULL;
    const uint64_t VAL_TOTAL_PKTS = 292823211ULL;
    const uint32_t VAL_TOTAL_FLOWS = 281124UL;
};

/// Options record with timestamps
class Rec_opts_timestamps : public Rec_base {
public:
    Rec_opts_timestamps(uint16_t tid)
    {
        add_field_uint(IPX_NF9_SCOPE_SYSTEM, 4, VAL_SCOPE_SYSTEM, 144U); // iana:exportingProcessId
        add_field_uint(IPX_NF9_IE_TOTAL_BYTES_EXP, 8, VAL_TOTAL_BYTES);
        add_field_uint(IPX_NF9_IE_TOTAL_PKTS_EXP,  8, VAL_TOTAL_PKTS);
        add_field_uint(IPX_NF9_IE_TOTAL_FLOWS_EXP, 4, VAL_TOTAL_FLOWS);
        add_field_time(IPX_NF9_IE_FIRST_SWITCHED, 4, VAL_TS_START, 152, 0, 8U); // new size and ID
        add_field_time(IPX_NF9_IE_LAST_SWITCHED,  4, VAL_TS_END, 153, 0, 8U); // new size and ID
        // Build Template and Data Record
        build(tid, 1);
    }

    const uint32_t VAL_SCOPE_SYSTEM = 32;
    const uint64_t VAL_TOTAL_BYTES = 82202029183ULL;
    const uint64_t VAL_TOTAL_PKTS = 292823211ULL;
    const uint32_t VAL_TOTAL_FLOWS = 281124UL;
    const uint32_t VAL_TS_START = 5000;
    const uint32_t VAL_TS_END = 7897;
};

/// Options record with single scope field and enterprise field specifiers
class Rec_opts_enterprise : public Rec_base {
public:
    Rec_opts_enterprise(uint16_t tid)
    {
        add_field_uint(IPX_NF9_SCOPE_TEMPLATE, 2, VAL_SCOPE_TID, 145U); // iana:templateId
        add_field_uint(400, 4, VAL_EN_UINT1, SAME_ID, EN_LOW);
        add_field_uint(43281, 8, VAL_EN_UINT2, 10513, EN_HIGH);
        // Build Template and Data Record
        build(tid, 1);
    }

    const uint32_t EN_LOW = 4294967294UL;
    const uint32_t EN_HIGH = 4294967295UL;

    const uint32_t VAL_SCOPE_TID = 256;
    const uint32_t VAL_EN_UINT1 = 2824;
    const uint64_t VAL_EN_UINT2 = 2811848212ULL;
};

/// Options record with multiple scope fields
class Rec_opts_multi : public Rec_base {
public:
    Rec_opts_multi(uint16_t tid)
    {
        add_field_uint(IPX_NF9_SCOPE_INTERFACE, 4, VAL_SCOPE_IFC, 10U);   // iana:ingressInterface
        add_field_uint(IPX_NF9_SCOPE_LINE_CARD, 4, VAL_SCOPE_CARD, 141U); // iana:lineCardId
        add_field_uint(IPX_NF9_IE_TOTAL_BYTES_EXP, 8, VAL_TOTAL_BYTES);
        add_field_uint(IPX_NF9_IE_TOTAL_PKTS_EXP,  8, VAL_TOTAL_PKTS);
        // Build Template and Data Record
        build(tid, 2);
    }

    const uint32_t VAL_SCOPE_IFC = 22;
    const uint32_t VAL_SCOPE_CARD = 23;
    const uint64_t VAL_TOTAL_BYTES = 82202029183ULL;
    const uint64_t VAL_TOTAL_PKTS = 292823211ULL;
};

/// Options record with unknown scope field type (ID > 5)
class Rec_opts_unknown : public Rec_base {
public:
    Rec_opts_unknown(uint16_t tid)
    {
        add_field_uint(10, 2, VAL_SCOPE); // non-standard scope field specifier
        add_field_uint(IPX_NF9_IE_FLOW_ACTIVE_TIMEOUT, 4, VAL_EXPORT_ACTIVE);
        add_field_uint(IPX_NF9_IE_FLOW_INACTIVE_TIMEOUT, 4, VAL_EXPORT_INACTIVE);
        // Build Template and Data Record
        build(tid, 1);
    }

    const uint16_t ID_SCOPE = 100;
    const uint16_t VAL_SCOPE = 20;
    const uint32_t VAL_EXPORT_ACTIVE = 300;
    const uint32_t VAL_EXPORT_INACTIVE = 30;
};

// -----------------------------------------------------------------------------

// Simple test that just create and destroy the converter
TEST_F(MsgBase, createAndDestroy)
{
    ASSERT_NE(m_conv, nullptr);
}

// Conversion of an empty NetFlow v9 message
TEST_F(MsgBase, emptyMessage)
{
    // Create a record and its context
    const uint32_t VALUE_EXPORT = 1562857357U; // 2019-07-11T15:02:37+00:00
    const uint32_t VALUE_ODID = 10;
    struct ipx_msg_ctx msg_ctx = {m_session.get(), VALUE_ODID, 0};

    nf9_msg nf9;
    nf9.set_odid(VALUE_ODID);
    nf9.set_seq(100);
    nf9.set_time_unix(VALUE_EXPORT);
    nf9.set_time_uptime(10001);  // 10.001 seconds since boot

    // Try to convert it
    uint16_t msg_size = nf9.size();
    uint8_t *msg_data = (uint8_t *) nf9.release();
    prepare_msg(&msg_ctx, msg_data, msg_size);
    ASSERT_EQ(ipx_nf9_conv_process(m_conv.get(), m_msg.get()), IPX_OK);

    // Check the IPFIX Message
    msg_data = ipx_msg_ipfix_get_packet(m_msg.get());
    const auto *ipfix_hdr = reinterpret_cast<const struct fds_ipfix_msg_hdr*>(msg_data);
    EXPECT_EQ(ntohs(ipfix_hdr->version), FDS_IPFIX_VERSION);
    EXPECT_EQ(ntohs(ipfix_hdr->length), FDS_IPFIX_MSG_HDR_LEN);
    EXPECT_EQ(ntohl(ipfix_hdr->odid), VALUE_ODID);
    EXPECT_EQ(ntohl(ipfix_hdr->export_time), VALUE_EXPORT);
    EXPECT_EQ(ntohl(ipfix_hdr->seq_num), 0);
}

// Try to convert NetFlow message with a single Template and single Data Record
TEST_F(MsgBase, oneTemplateOneDataRecord)
{
    // Create a record and its context
    const uint32_t VALUE_EXPORT = 1562857357U; // 2019-07-11T15:02:37+00:00
    const uint32_t VALUE_UPTIME = 10001;
    const uint32_t VALUE_ODID = 10;
    struct ipx_msg_ctx msg_ctx = {m_session.get(), VALUE_ODID, 0};

    // Try to different type of flow records
    uint16_t tid = IPX_NF9_SET_MIN_DSET;
    Rec_norm_basic r_basic(tid);
    Rec_norm_enterprise r_enterprise(tid);
    Rec_norm_multi r_multi(tid);
    Rec_norm_nots r_nots(tid);
    Rec_norm_onlyts r_onlyts(tid);

    // For each type of the flow record try to create a new NetFlow v9 message
    int i = 0;
    for (Rec_base *rec_ptr : {
            dynamic_cast<Rec_base *>(&r_basic),
            dynamic_cast<Rec_base *>(&r_enterprise),
            dynamic_cast<Rec_base *>(&r_multi),
            dynamic_cast<Rec_base *>(&r_nots),
            dynamic_cast<Rec_base *>(&r_onlyts)}) {
        SCOPED_TRACE("Record index: " + std::to_string(i++));

        // Try different combinations of Template FlowSet and Data FlowSet paddings
        const uint16_t pad_dset_max = rec_ptr->get_nf9_record().size();
        const uint16_t pad_tset_max = 8U; // i.e. Template with just one field
        for (uint16_t pad_tset = 0; pad_tset < pad_tset_max; pad_tset++) {
            SCOPED_TRACE("Template FlowSet padding: " + std::to_string(pad_tset));
            for (uint16_t pad_dset = 0; pad_dset < pad_dset_max; ++pad_dset) {
                SCOPED_TRACE("Data FlowSet padding: " + std::to_string(pad_dset));

                nf9_set nf9_tset(IPX_NF9_SET_TMPLT);
                nf9_tset.add_rec(rec_ptr->get_nf9_template());
                if (pad_tset) {
                    nf9_tset.add_padding(pad_tset);
                }
                nf9_set nf9_dset(tid);
                nf9_dset.add_rec(rec_ptr->get_nf9_record());
                if (pad_dset) {
                    nf9_dset.add_padding(pad_dset);
                }
                nf9_msg nf9;
                nf9.set_odid(VALUE_ODID);
                nf9.set_time_unix(VALUE_EXPORT);
                nf9.set_time_uptime(VALUE_UPTIME);
                nf9.add_set(nf9_tset);
                nf9.add_set(nf9_dset);

                // Create a new converter for each message
                converter_create(IPX_VERB_DEBUG);

                // Try to convert the message
                uint16_t msg_size = nf9.size();
                uint8_t *msg_data = (uint8_t *) nf9.release();
                prepare_msg(&msg_ctx, msg_data, msg_size);
                ASSERT_EQ(ipx_nf9_conv_process(m_conv.get(), m_msg.get()), IPX_OK);

                // Try to parse the message
                msg_data = ipx_msg_ipfix_get_packet(m_msg.get());
                auto *ipfix_hdr = reinterpret_cast<struct fds_ipfix_msg_hdr*>(msg_data);
                EXPECT_EQ(ntohs(ipfix_hdr->version), FDS_IPFIX_VERSION);
                EXPECT_GE(ntohs(ipfix_hdr->length), FDS_IPFIX_MSG_HDR_LEN);
                EXPECT_EQ(ntohl(ipfix_hdr->odid), VALUE_ODID);
                EXPECT_EQ(ntohl(ipfix_hdr->export_time), VALUE_EXPORT);
                EXPECT_EQ(ntohl(ipfix_hdr->seq_num), 0);

                // Try to parse the body
                struct fds_sets_iter it_set;
                fds_sets_iter_init(&it_set, ipfix_hdr);

                // Expect Template Set
                ASSERT_EQ(fds_sets_iter_next(&it_set), FDS_OK);
                ASSERT_EQ(ntohs(it_set.set->flowset_id), FDS_IPFIX_SET_TMPLT);
                fds_tset_iter it_tset;
                fds_tset_iter_init(&it_tset, it_set.set);
                ASSERT_EQ(fds_tset_iter_next(&it_tset), FDS_OK);
                // Parse the Template
                auto tmplt = parse_template(it_tset, FDS_TYPE_TEMPLATE);
                rec_ptr->compare_template(tmplt.get());
                // Expect no more Templates in the Set
                EXPECT_EQ(fds_tset_iter_next(&it_tset), FDS_EOC);

                // Expect Data Set
                ASSERT_EQ(fds_sets_iter_next(&it_set), FDS_OK);
                ASSERT_EQ(ntohs(it_set.set->flowset_id), tid);
                struct fds_dset_iter it_dset;
                fds_dset_iter_init(&it_dset, it_set.set, tmplt.get());
                ASSERT_EQ(fds_dset_iter_next(&it_dset), FDS_OK);

                struct fds_drec drec = {it_dset.rec, it_dset.size, tmplt.get(), nullptr};
                rec_ptr->compare_data(&drec, VALUE_EXPORT, VALUE_UPTIME);

                // No more records
                EXPECT_EQ(fds_dset_iter_next(&it_dset), FDS_EOC);
                EXPECT_EQ(fds_sets_iter_next(&it_set), FDS_EOC);
            }
        }
    }
}

// Try to convert NetFlow message with a single Options Template and single Data Record
TEST_F(MsgBase, oneOptionsTemplateOneDataRecord)
{
    // Create a record and its context
    const uint32_t VALUE_EXPORT = 1562857357U; // 2019-07-11T15:02:37+00:00
    const uint32_t VALUE_UPTIME = 983122U;
    const uint32_t VALUE_ODID = 32;
    struct ipx_msg_ctx msg_ctx = {m_session.get(), VALUE_ODID, 0};

    // Try to different type of options records
    uint16_t tid = IPX_NF9_SET_MIN_DSET;
    Rec_opts_simple r_simple(tid);
    Rec_opts_timestamps r_ts(tid);
    Rec_opts_enterprise r_enterprise(tid);
    Rec_opts_multi r_multi(tid);
    // Note: Rec_opts_unknown should be ignored during conversion, so it is not here...

    // For each type of the flow record try to create a new NetFlow v9 message
    int i = 0;
    for (Rec_base *rec_ptr : {
            dynamic_cast<Rec_base *>(&r_simple),
            dynamic_cast<Rec_base *>(&r_ts),
            dynamic_cast<Rec_base *>(&r_enterprise),
            dynamic_cast<Rec_base *>(&r_multi)}) {
        SCOPED_TRACE("Record index: " + std::to_string(i++));

        // Try different combinations of Template FlowSet and Data FlowSet paddings
        const uint16_t pad_dset_max = rec_ptr->get_nf9_record().size();
        const uint16_t pad_tset_max = 10U; // i.e. Options Template with just one field
        for (uint16_t pad_tset = 0; pad_tset < pad_tset_max; pad_tset++) {
            SCOPED_TRACE("Template FlowSet padding: " + std::to_string(pad_tset));
            for (uint16_t pad_dset = 0; pad_dset < pad_dset_max; ++pad_dset) {
                SCOPED_TRACE("Data FlowSet padding: " + std::to_string(pad_dset));

                nf9_set nf9_tset(IPX_NF9_SET_OPTS_TMPLT);
                nf9_tset.add_rec(rec_ptr->get_nf9_template());
                if (pad_tset) {
                    nf9_tset.add_padding(pad_tset);
                }
                nf9_set nf9_dset(tid);
                nf9_dset.add_rec(rec_ptr->get_nf9_record());
                if (pad_dset) {
                    nf9_dset.add_padding(pad_dset);
                }
                nf9_msg nf9;
                nf9.set_odid(VALUE_ODID);
                nf9.set_time_unix(VALUE_EXPORT);
                nf9.set_time_uptime(VALUE_UPTIME);
                nf9.add_set(nf9_tset);
                nf9.add_set(nf9_dset);

                // Create a new converter for each message
                converter_create(IPX_VERB_DEBUG);

                // Try to convert the message
                uint16_t msg_size = nf9.size();
                uint8_t *msg_data = (uint8_t *) nf9.release();
                prepare_msg(&msg_ctx, msg_data, msg_size);
                ASSERT_EQ(ipx_nf9_conv_process(m_conv.get(), m_msg.get()), IPX_OK);

                // Try to parse the message
                msg_data = ipx_msg_ipfix_get_packet(m_msg.get());
                auto *ipfix_hdr = reinterpret_cast<struct fds_ipfix_msg_hdr*>(msg_data);
                EXPECT_EQ(ntohs(ipfix_hdr->version), FDS_IPFIX_VERSION);
                EXPECT_GE(ntohs(ipfix_hdr->length), FDS_IPFIX_MSG_HDR_LEN);
                EXPECT_EQ(ntohl(ipfix_hdr->odid), VALUE_ODID);
                EXPECT_EQ(ntohl(ipfix_hdr->export_time), VALUE_EXPORT);
                EXPECT_EQ(ntohl(ipfix_hdr->seq_num), 0);

                // Try to parse the body
                struct fds_sets_iter it_set;
                fds_sets_iter_init(&it_set, ipfix_hdr);

                // Expect Options Template Set
                ASSERT_EQ(fds_sets_iter_next(&it_set), FDS_OK);
                ASSERT_EQ(ntohs(it_set.set->flowset_id), FDS_IPFIX_SET_OPTS_TMPLT);
                fds_tset_iter it_tset;
                fds_tset_iter_init(&it_tset, it_set.set);
                ASSERT_EQ(fds_tset_iter_next(&it_tset), FDS_OK);
                // Parse the Options Template
                auto tmplt = parse_template(it_tset, FDS_TYPE_TEMPLATE_OPTS);
                rec_ptr->compare_template(tmplt.get());
                // Expect no more Options Templates in the Set
                EXPECT_EQ(fds_tset_iter_next(&it_tset), FDS_EOC);

                // Expect Data Set
                ASSERT_EQ(fds_sets_iter_next(&it_set), FDS_OK);
                ASSERT_EQ(ntohs(it_set.set->flowset_id), tid);
                struct fds_dset_iter it_dset;
                fds_dset_iter_init(&it_dset, it_set.set, tmplt.get());
                ASSERT_EQ(fds_dset_iter_next(&it_dset), FDS_OK);

                struct fds_drec drec = {it_dset.rec, it_dset.size, tmplt.get(), nullptr};
                rec_ptr->compare_data(&drec, VALUE_EXPORT, VALUE_UPTIME);

                // No more records
                EXPECT_EQ(fds_dset_iter_next(&it_dset), FDS_EOC);
                EXPECT_EQ(fds_sets_iter_next(&it_set), FDS_EOC);
            }
        }
    }
}

// Try to convert NetFlow message with unsupported Options Template (unknown Scope ID)
TEST_F(MsgBase, unsupportedOptionsTemplateRecord)
{
    // Message context
    const uint32_t VALUE_EXPORT = 1562857357U; // 2019-07-11T15:02:37+00:00
    const uint32_t VALUE_UPTIME = 10001;
    const uint32_t VALUE_ODID = 0;
    struct ipx_msg_ctx msg_ctx = {m_session.get(), VALUE_ODID, 0};

    // Create the message (with one Options Template set and one Data Record)
    uint16_t tid = 256;
    Rec_opts_unknown rec_unknown(tid);

    nf9_set nf9_tset(IPX_NF9_SET_OPTS_TMPLT);
    nf9_tset.add_rec(rec_unknown.get_nf9_template());
    nf9_set nf9_dset(tid);
    nf9_dset.add_rec(rec_unknown.get_nf9_record());
    nf9_msg nf9;
    nf9.set_odid(VALUE_ODID);
    nf9.set_time_unix(VALUE_EXPORT);
    nf9.set_time_uptime(VALUE_UPTIME);
    nf9.add_set(nf9_tset);
    nf9.add_set(nf9_dset);

    // Try to convert the message
    uint16_t msg_size = nf9.size();
    uint8_t *msg_data = (uint8_t *) nf9.release();
    prepare_msg(&msg_ctx, msg_data, msg_size);
    ASSERT_EQ(ipx_nf9_conv_process(m_conv.get(), m_msg.get()), IPX_OK);

    // Try to parse the message
    msg_data = ipx_msg_ipfix_get_packet(m_msg.get());
    auto *ipfix_hdr = reinterpret_cast<struct fds_ipfix_msg_hdr*>(msg_data);
    EXPECT_EQ(ntohs(ipfix_hdr->version), FDS_IPFIX_VERSION);
    EXPECT_GE(ntohs(ipfix_hdr->length), FDS_IPFIX_MSG_HDR_LEN);
    EXPECT_EQ(ntohl(ipfix_hdr->odid), VALUE_ODID);
    EXPECT_EQ(ntohl(ipfix_hdr->export_time), VALUE_EXPORT);
    EXPECT_EQ(ntohl(ipfix_hdr->seq_num), 0);

    // Try to parse the body
    struct fds_sets_iter it_set;
    fds_sets_iter_init(&it_set, ipfix_hdr);

    /* Note: Because unsupported Options Templates and their Data Records should be
     * ignored, we assume that the body should be also empty
     */
    EXPECT_EQ(fds_sets_iter_next(&it_set), FDS_EOC);
}

/// Try to convert a NetFlow message with multiple Templates Sets (no Data Records)
TEST_F(MsgBase, onlyTemplatesInMsg)
{
    // Create a record and its context
    const uint32_t VALUE_EXPORT = 1562857357U; // 2019-07-11T15:02:37+00:00
    const uint32_t VALUE_UPTIME = 983122U;
    const uint32_t VALUE_ODID = 0;
    struct ipx_msg_ctx msg_ctx = {m_session.get(), VALUE_ODID, 0};

    // Create a message
    Rec_norm_basic      rec_norm_basic(256);
    Rec_norm_multi      rec_norm_multi(257);
    Rec_norm_nots       rec_norm_nots(400);
    Rec_norm_enterprise rec_norm_enterprise(62632);
    Rec_opts_simple     rec_opts_simple(2232);
    Rec_opts_timestamps rec_opts_timestamps(726);
    Rec_opts_enterprise rec_opts_enterprise(7236);
    Rec_opts_unknown    rec_opts_unknown(62392);

    nf9_set nf9_tset_norm1(IPX_NF9_SET_TMPLT);
    nf9_tset_norm1.add_rec(rec_norm_basic.get_nf9_template());
    nf9_tset_norm1.add_rec(rec_norm_multi.get_nf9_template());
    nf9_tset_norm1.add_rec(rec_norm_nots.get_nf9_template());
    nf9_set nf9_tset_opts(IPX_NF9_SET_OPTS_TMPLT);
    nf9_tset_opts.add_rec(rec_opts_simple.get_nf9_template());
    nf9_tset_opts.add_rec(rec_opts_timestamps.get_nf9_template());
    nf9_tset_opts.add_rec(rec_opts_unknown.get_nf9_template());
    nf9_tset_opts.add_rec(rec_opts_enterprise.get_nf9_template());
    nf9_set nf9_tset_norm2(IPX_NF9_SET_TMPLT);
    nf9_tset_norm2.add_rec(rec_norm_enterprise.get_nf9_template());
    nf9_msg nf9;
    nf9.set_seq(228321); // random number
    nf9.set_odid(VALUE_ODID);
    nf9.set_time_unix(VALUE_EXPORT);
    nf9.set_time_uptime(VALUE_UPTIME);
    nf9.add_set(nf9_tset_norm1);
    nf9.add_set(nf9_tset_opts);
    nf9.add_set(nf9_tset_norm2);

    // Try to convert the message
    uint16_t msg_size = nf9.size();
    uint8_t *msg_data = (uint8_t *) nf9.release();
    prepare_msg(&msg_ctx, msg_data, msg_size);
    ASSERT_EQ(ipx_nf9_conv_process(m_conv.get(), m_msg.get()), IPX_OK);

    // Try to parse the header
    msg_data = ipx_msg_ipfix_get_packet(m_msg.get());
    auto *ipfix_hdr = reinterpret_cast<struct fds_ipfix_msg_hdr*>(msg_data);
    EXPECT_EQ(ntohs(ipfix_hdr->version), FDS_IPFIX_VERSION);
    EXPECT_GE(ntohs(ipfix_hdr->length), FDS_IPFIX_MSG_HDR_LEN);
    EXPECT_EQ(ntohl(ipfix_hdr->odid), VALUE_ODID);
    EXPECT_EQ(ntohl(ipfix_hdr->export_time), VALUE_EXPORT);
    EXPECT_EQ(ntohl(ipfix_hdr->seq_num), 0);

    // Try to parse the body
    fds_tset_iter it_tset;
    struct fds_sets_iter it_set;
    fds_sets_iter_init(&it_set, ipfix_hdr);

    // Expect Template Set (parse and compare all templates)
    ASSERT_EQ(fds_sets_iter_next(&it_set), FDS_OK);
    ASSERT_EQ(ntohs(it_set.set->flowset_id), FDS_IPFIX_SET_TMPLT);
    fds_tset_iter_init(&it_tset, it_set.set);
    ASSERT_EQ(fds_tset_iter_next(&it_tset), FDS_OK);
    auto tmplt = parse_template(it_tset, FDS_TYPE_TEMPLATE);
    rec_norm_basic.compare_template(tmplt.get());
    ASSERT_EQ(fds_tset_iter_next(&it_tset), FDS_OK);
    tmplt = parse_template(it_tset, FDS_TYPE_TEMPLATE);
    rec_norm_multi.compare_template(tmplt.get());
    ASSERT_EQ(fds_tset_iter_next(&it_tset), FDS_OK);
    tmplt = parse_template(it_tset, FDS_TYPE_TEMPLATE);
    rec_norm_nots.compare_template(tmplt.get());
    EXPECT_EQ(fds_tset_iter_next(&it_tset), FDS_EOC);

    // Expect Options Template Set
    ASSERT_EQ(fds_sets_iter_next(&it_set), FDS_OK);
    ASSERT_EQ(ntohs(it_set.set->flowset_id), FDS_IPFIX_SET_OPTS_TMPLT);
    fds_tset_iter_init(&it_tset, it_set.set);
    ASSERT_EQ(fds_tset_iter_next(&it_tset), FDS_OK);
    tmplt = parse_template(it_tset, FDS_TYPE_TEMPLATE_OPTS);
    rec_opts_simple.compare_template(tmplt.get());
    ASSERT_EQ(fds_tset_iter_next(&it_tset), FDS_OK);
    tmplt = parse_template(it_tset, FDS_TYPE_TEMPLATE_OPTS);
    rec_opts_timestamps.compare_template(tmplt.get());
    // Note: "unknown" Options Template should be skipped by converter!
    ASSERT_EQ(fds_tset_iter_next(&it_tset), FDS_OK);
    tmplt = parse_template(it_tset, FDS_TYPE_TEMPLATE_OPTS);
    rec_opts_enterprise.compare_template(tmplt.get());
    EXPECT_EQ(fds_tset_iter_next(&it_tset), FDS_EOC);

    // Expect Template Set
    ASSERT_EQ(fds_sets_iter_next(&it_set), FDS_OK);
    ASSERT_EQ(ntohs(it_set.set->flowset_id), FDS_IPFIX_SET_TMPLT);
    fds_tset_iter_init(&it_tset, it_set.set);
    ASSERT_EQ(fds_tset_iter_next(&it_tset), FDS_OK);
    tmplt = parse_template(it_tset, FDS_TYPE_TEMPLATE);
    rec_norm_enterprise.compare_template(tmplt.get());
    EXPECT_EQ(fds_tset_iter_next(&it_tset), FDS_EOC);

    // No more Sets
    EXPECT_EQ(fds_sets_iter_next(&it_set), FDS_EOC);
}

/// Try to convert sequence of multiple NetFlow messages
TEST_F(MsgBase, simpleMessageSequence)
{
    // Message context
    const uint32_t VALUE_EXPORT = 1562857357U; // 2019-07-11T15:02:37+00:00
    const uint32_t VALUE_UPTIME = 7278632U;
    const uint32_t VALUE_ODID = 12345678UL;
    uint32_t VALUE_SEQ = 625372U;
    struct ipx_msg_ctx msg_ctx = {m_session.get(), VALUE_ODID, 0};

    // Create a simple NetFlow message -----------------------------------------
    const uint16_t tid_flow1 = 256;
    const uint16_t tid_flow2 = 257;
    const uint16_t tid_opts = 258;

    Rec_norm_basic      r_flow1(tid_flow1);
    Rec_norm_enterprise r_flow2(tid_flow2);
    Rec_opts_simple     r_opts(tid_opts);

    nf9_set nf9_1a_tset(IPX_NF9_SET_TMPLT);
    nf9_1a_tset.add_rec(r_flow1.get_nf9_template());
    nf9_set nf9_1b_dset(tid_flow1);
    nf9_1b_dset.add_rec(r_flow1.get_nf9_record());
    nf9_1b_dset.add_rec(r_flow1.get_nf9_record());
    nf9_msg nf9_1;
    nf9_1.set_odid(VALUE_ODID);
    nf9_1.set_time_unix(VALUE_EXPORT);
    nf9_1.set_time_uptime(VALUE_UPTIME);
    nf9_1.set_seq(VALUE_SEQ);
    nf9_1.add_set(nf9_1a_tset);
    nf9_1.add_set(nf9_1b_dset);

    // Try to convert the message
    uint16_t msg_size = nf9_1.size();
    uint8_t *msg_data = (uint8_t *) nf9_1.release();
    prepare_msg(&msg_ctx, msg_data, msg_size);
    ASSERT_EQ(ipx_nf9_conv_process(m_conv.get(), m_msg.get()), IPX_OK);

    // Try to parse the header
    msg_data = ipx_msg_ipfix_get_packet(m_msg.get());
    auto *ipfix_hdr = reinterpret_cast<struct fds_ipfix_msg_hdr*>(msg_data);
    EXPECT_EQ(ntohs(ipfix_hdr->version), FDS_IPFIX_VERSION);
    EXPECT_GE(ntohs(ipfix_hdr->length), FDS_IPFIX_MSG_HDR_LEN);
    EXPECT_EQ(ntohl(ipfix_hdr->odid), VALUE_ODID);
    EXPECT_EQ(ntohl(ipfix_hdr->export_time), VALUE_EXPORT);
    EXPECT_EQ(ntohl(ipfix_hdr->seq_num), 0);

    // Try to parse the body
    struct fds_sets_iter it_set;
    struct fds_tset_iter it_tset;
    struct fds_dset_iter it_dset;
    struct fds_drec drec;
    fds_sets_iter_init(&it_set, ipfix_hdr);

    // Expect Template Set
    ASSERT_EQ(fds_sets_iter_next(&it_set), FDS_OK);
    ASSERT_EQ(ntohs(it_set.set->flowset_id), FDS_IPFIX_SET_TMPLT);
    fds_tset_iter_init(&it_tset, it_set.set);
    ASSERT_EQ(fds_tset_iter_next(&it_tset), FDS_OK);
    auto tmplt_flow1 = parse_template(it_tset, FDS_TYPE_TEMPLATE);
    r_flow1.compare_template(tmplt_flow1.get());
    EXPECT_EQ(fds_tset_iter_next(&it_tset), FDS_EOC);

    // Expect Data Set
    ASSERT_EQ(fds_sets_iter_next(&it_set), FDS_OK);
    ASSERT_EQ(ntohs(it_set.set->flowset_id), tid_flow1);
    fds_dset_iter_init(&it_dset, it_set.set, tmplt_flow1.get());
    ASSERT_EQ(fds_dset_iter_next(&it_dset), FDS_OK);
    drec = {it_dset.rec, it_dset.size, tmplt_flow1.get(), nullptr};
    r_flow1.compare_data(&drec, VALUE_EXPORT, VALUE_UPTIME);
    ASSERT_EQ(fds_dset_iter_next(&it_dset), FDS_OK);
    drec = {it_dset.rec, it_dset.size, tmplt_flow1.get(), nullptr};
    r_flow1.compare_data(&drec, VALUE_EXPORT, VALUE_UPTIME);

    // No more records
    EXPECT_EQ(fds_dset_iter_next(&it_dset), FDS_EOC);
    EXPECT_EQ(fds_sets_iter_next(&it_set), FDS_EOC);

    // Create another message --------------------------------------------------
    nf9_set nf9_2a_dset(tid_flow1);
    nf9_2a_dset.add_rec(r_flow1.get_nf9_record());
    nf9_2a_dset.add_rec(r_flow1.get_nf9_record());
    nf9_set nf9_2b_tset(IPX_NF9_SET_OPTS_TMPLT);
    nf9_2b_tset.add_rec(r_opts.get_nf9_template());
    nf9_set nf9_2c_tset(IPX_NF9_SET_TMPLT);
    nf9_2c_tset.add_rec(r_flow2.get_nf9_template());
    nf9_set nf9_2d_dset(tid_flow2);
    nf9_2d_dset.add_rec(r_flow2.get_nf9_record());
    nf9_msg nf9_2;
    nf9_2.set_odid(VALUE_ODID);
    nf9_2.set_time_unix(VALUE_EXPORT + 100);
    nf9_2.set_time_uptime(VALUE_UPTIME + 100);
    nf9_2.set_seq(VALUE_SEQ + 1); // the previous message
    nf9_2.add_set(nf9_2a_dset);
    nf9_2.add_set(nf9_2b_tset);
    nf9_2.add_set(nf9_2c_tset);
    nf9_2.add_set(nf9_2d_dset);

    // Try to convert the message
    msg_size = nf9_2.size();
    msg_data = (uint8_t *) nf9_2.release();
    prepare_msg(&msg_ctx, msg_data, msg_size);
    ASSERT_EQ(ipx_nf9_conv_process(m_conv.get(), m_msg.get()), IPX_OK);

    // Try to parse the header
    msg_data = ipx_msg_ipfix_get_packet(m_msg.get());
    ipfix_hdr = reinterpret_cast<struct fds_ipfix_msg_hdr*>(msg_data);
    EXPECT_EQ(ntohs(ipfix_hdr->version), FDS_IPFIX_VERSION);
    EXPECT_GE(ntohs(ipfix_hdr->length), FDS_IPFIX_MSG_HDR_LEN);
    EXPECT_EQ(ntohl(ipfix_hdr->odid), VALUE_ODID);
    EXPECT_EQ(ntohl(ipfix_hdr->export_time), VALUE_EXPORT + 100);
    EXPECT_EQ(ntohl(ipfix_hdr->seq_num), 2); // 2 Data Records in the previous message

    // Try to parse the body
    fds_sets_iter_init(&it_set, ipfix_hdr);

    // Expect Data Set
    ASSERT_EQ(fds_sets_iter_next(&it_set), FDS_OK);
    ASSERT_EQ(ntohs(it_set.set->flowset_id), tid_flow1);
    fds_dset_iter_init(&it_dset, it_set.set, tmplt_flow1.get());
    ASSERT_EQ(fds_dset_iter_next(&it_dset), FDS_OK);
    drec = {it_dset.rec, it_dset.size, tmplt_flow1.get(), nullptr};
    r_flow1.compare_data(&drec, VALUE_EXPORT + 100, VALUE_UPTIME + 100);
    ASSERT_EQ(fds_dset_iter_next(&it_dset), FDS_OK);
    drec = {it_dset.rec, it_dset.size, tmplt_flow1.get(), nullptr};
    r_flow1.compare_data(&drec, VALUE_EXPORT + 100, VALUE_UPTIME + 100);
    EXPECT_EQ(fds_dset_iter_next(&it_dset), FDS_EOC);

    // Expect Options Template Set
    ASSERT_EQ(fds_sets_iter_next(&it_set), FDS_OK);
    ASSERT_EQ(ntohs(it_set.set->flowset_id), FDS_IPFIX_SET_OPTS_TMPLT);
    fds_tset_iter_init(&it_tset, it_set.set);
    ASSERT_EQ(fds_tset_iter_next(&it_tset), FDS_OK);
    auto tmplt_opts = parse_template(it_tset, FDS_TYPE_TEMPLATE_OPTS);
    r_opts.compare_template(tmplt_opts.get());
    EXPECT_EQ(fds_tset_iter_next(&it_tset), FDS_EOC);

    // Expect Template Set
    ASSERT_EQ(fds_sets_iter_next(&it_set), FDS_OK);
    ASSERT_EQ(ntohs(it_set.set->flowset_id), FDS_IPFIX_SET_TMPLT);
    fds_tset_iter_init(&it_tset, it_set.set);
    ASSERT_EQ(fds_tset_iter_next(&it_tset), FDS_OK);
    auto tmplt_flow2 = parse_template(it_tset, FDS_TYPE_TEMPLATE);
    r_flow2.compare_template(tmplt_flow2.get());
    EXPECT_EQ(fds_tset_iter_next(&it_tset), FDS_EOC);

    // Expect Data Set
    ASSERT_EQ(fds_sets_iter_next(&it_set), FDS_OK);
    ASSERT_EQ(ntohs(it_set.set->flowset_id), tid_flow2);
    fds_dset_iter_init(&it_dset, it_set.set, tmplt_flow2.get());
    ASSERT_EQ(fds_dset_iter_next(&it_dset), FDS_OK);
    drec = {it_dset.rec, it_dset.size, tmplt_flow2.get(), nullptr};
    r_flow2.compare_data(&drec, VALUE_EXPORT + 100, VALUE_UPTIME + 100);

    // No more records
    EXPECT_EQ(fds_dset_iter_next(&it_dset), FDS_EOC);
    EXPECT_EQ(fds_sets_iter_next(&it_set), FDS_EOC);

    // Create another message again --------------------------------------------
    nf9_set nf9_3a_dset(tid_flow1);
    nf9_3a_dset.add_rec(r_flow1.get_nf9_record());
    nf9_3a_dset.add_rec(r_flow1.get_nf9_record());
    nf9_set nf9_3b_dset(tid_opts);
    nf9_3b_dset.add_rec(r_opts.get_nf9_record());
    nf9_3b_dset.add_rec(r_opts.get_nf9_record());
    nf9_set nf9_3c_dset(tid_flow2);
    nf9_3c_dset.add_rec(r_flow2.get_nf9_record());
    nf9_3c_dset.add_rec(r_flow2.get_nf9_record());
    nf9_msg nf9_3;
    nf9_3.set_odid(VALUE_ODID);
    nf9_3.set_time_unix(VALUE_EXPORT + 200);
    nf9_3.set_time_uptime(VALUE_UPTIME + 200);
    nf9_3.set_seq(VALUE_SEQ + 2); // the previous 2 messages
    nf9_3.add_set(nf9_3a_dset);
    nf9_3.add_set(nf9_3b_dset);
    nf9_3.add_set(nf9_3c_dset);

    // Try to convert the message
    msg_size = nf9_3.size();
    msg_data = (uint8_t *) nf9_3.release();
    prepare_msg(&msg_ctx, msg_data, msg_size);
    ASSERT_EQ(ipx_nf9_conv_process(m_conv.get(), m_msg.get()), IPX_OK);

    // Try to parse the header
    msg_data = ipx_msg_ipfix_get_packet(m_msg.get());
    ipfix_hdr = reinterpret_cast<struct fds_ipfix_msg_hdr*>(msg_data);
    EXPECT_EQ(ntohs(ipfix_hdr->version), FDS_IPFIX_VERSION);
    EXPECT_GE(ntohs(ipfix_hdr->length), FDS_IPFIX_MSG_HDR_LEN);
    EXPECT_EQ(ntohl(ipfix_hdr->odid), VALUE_ODID);
    EXPECT_EQ(ntohl(ipfix_hdr->export_time), VALUE_EXPORT + 200);
    EXPECT_EQ(ntohl(ipfix_hdr->seq_num), 5); // 2 + 3 Data Records in the previous messages

    // Try to parse the body
    fds_sets_iter_init(&it_set, ipfix_hdr);

    // Expect Data Set
    ASSERT_EQ(fds_sets_iter_next(&it_set), FDS_OK);
    ASSERT_EQ(ntohs(it_set.set->flowset_id), tid_flow1);
    fds_dset_iter_init(&it_dset, it_set.set, tmplt_flow1.get());
    ASSERT_EQ(fds_dset_iter_next(&it_dset), FDS_OK);
    drec = {it_dset.rec, it_dset.size, tmplt_flow1.get(), nullptr};
    r_flow1.compare_data(&drec, VALUE_EXPORT + 200, VALUE_UPTIME + 200);
    ASSERT_EQ(fds_dset_iter_next(&it_dset), FDS_OK);
    drec = {it_dset.rec, it_dset.size, tmplt_flow1.get(), nullptr};
    r_flow1.compare_data(&drec, VALUE_EXPORT + 200, VALUE_UPTIME + 200);
    EXPECT_EQ(fds_dset_iter_next(&it_dset), FDS_EOC);

    // Expect Data Set
    ASSERT_EQ(fds_sets_iter_next(&it_set), FDS_OK);
    ASSERT_EQ(ntohs(it_set.set->flowset_id), tid_opts);
    fds_dset_iter_init(&it_dset, it_set.set, tmplt_opts.get());
    ASSERT_EQ(fds_dset_iter_next(&it_dset), FDS_OK);
    drec = {it_dset.rec, it_dset.size, tmplt_opts.get(), nullptr};
    r_opts.compare_data(&drec, VALUE_EXPORT + 200, VALUE_UPTIME + 200);
    ASSERT_EQ(fds_dset_iter_next(&it_dset), FDS_OK);
    drec = {it_dset.rec, it_dset.size, tmplt_opts.get(), nullptr};
    r_opts.compare_data(&drec, VALUE_EXPORT + 200, VALUE_UPTIME + 200);
    EXPECT_EQ(fds_dset_iter_next(&it_dset), FDS_EOC);

    // Expect Data Set
    ASSERT_EQ(fds_sets_iter_next(&it_set), FDS_OK);
    ASSERT_EQ(ntohs(it_set.set->flowset_id), tid_flow2);
    fds_dset_iter_init(&it_dset, it_set.set, tmplt_flow2.get());
    ASSERT_EQ(fds_dset_iter_next(&it_dset), FDS_OK);
    drec = {it_dset.rec, it_dset.size, tmplt_flow2.get(), nullptr};
    r_flow2.compare_data(&drec, VALUE_EXPORT + 200, VALUE_UPTIME + 200);
    ASSERT_EQ(fds_dset_iter_next(&it_dset), FDS_OK);
    drec = {it_dset.rec, it_dset.size, tmplt_flow2.get(), nullptr};
    r_flow2.compare_data(&drec, VALUE_EXPORT + 200, VALUE_UPTIME + 200);
    EXPECT_EQ(fds_dset_iter_next(&it_dset), FDS_EOC);

    // No more sets
    EXPECT_EQ(fds_sets_iter_next(&it_set), FDS_EOC);
}

/// Try to refresh a Template in the message (the same definition)
TEST_F(MsgBase, templateRefresh)
{
    // Message context
    const uint32_t VALUE_EXPORT = 1562857357U; // 2019-07-11T15:02:37+00:00
    const uint32_t VALUE_UPTIME = 7278632U;
    const uint32_t VALUE_ODID = UINT32_MAX;
    uint32_t VALUE_SEQ = UINT32_MAX;
    struct ipx_msg_ctx msg_ctx = {m_session.get(), VALUE_ODID, 0};

    // Create a simple NetFlow message -----------------------------------------
    const uint16_t tid_flow1 = 256;
    const uint16_t tid_flow2 = 257;
    const uint16_t tid_opts1 = 258;
    const uint16_t tid_opts2 = 259;

    Rec_norm_basic      r_flow1(tid_flow1);
    Rec_norm_enterprise r_flow2(tid_flow2);
    Rec_opts_simple     r_opts1(tid_opts1);
    Rec_opts_unknown    r_opts2(tid_opts2);

    nf9_set nf9_1a_tset(IPX_NF9_SET_TMPLT);
    nf9_1a_tset.add_rec(r_flow1.get_nf9_template());
    nf9_1a_tset.add_rec(r_flow2.get_nf9_template());
    nf9_set nf9_1b_tset(IPX_NF9_SET_OPTS_TMPLT);
    nf9_1b_tset.add_rec(r_opts1.get_nf9_template());
    nf9_1b_tset.add_rec(r_opts2.get_nf9_template()); // Unsupported template
    nf9_set nf9_1c_dset(tid_flow1);
    nf9_1c_dset.add_rec(r_flow1.get_nf9_record());
    nf9_msg nf9_1;
    nf9_1.set_odid(VALUE_ODID);
    nf9_1.set_time_unix(VALUE_EXPORT);
    nf9_1.set_time_uptime(VALUE_UPTIME);
    nf9_1.set_seq(VALUE_SEQ);
    nf9_1.add_set(nf9_1a_tset);
    nf9_1.add_set(nf9_1b_tset);
    nf9_1.add_set(nf9_1c_dset);

    // Try to convert the message
    uint16_t msg_size = nf9_1.size();
    uint8_t *msg_data = (uint8_t *) nf9_1.release();
    prepare_msg(&msg_ctx, msg_data, msg_size);
    ASSERT_EQ(ipx_nf9_conv_process(m_conv.get(), m_msg.get()), IPX_OK);

    // Try to parse the header
    msg_data = ipx_msg_ipfix_get_packet(m_msg.get());
    auto *ipfix_hdr = reinterpret_cast<struct fds_ipfix_msg_hdr*>(msg_data);
    EXPECT_EQ(ntohs(ipfix_hdr->version), FDS_IPFIX_VERSION);
    EXPECT_GE(ntohs(ipfix_hdr->length), FDS_IPFIX_MSG_HDR_LEN);
    EXPECT_EQ(ntohl(ipfix_hdr->odid), VALUE_ODID);
    EXPECT_EQ(ntohl(ipfix_hdr->export_time), VALUE_EXPORT);
    EXPECT_EQ(ntohl(ipfix_hdr->seq_num), 0);

    // Try to parse the body
    struct fds_sets_iter it_set;
    struct fds_tset_iter it_tset;
    struct fds_dset_iter it_dset;
    struct fds_drec drec;
    fds_sets_iter_init(&it_set, ipfix_hdr);

    // Expect Template Set
    ASSERT_EQ(fds_sets_iter_next(&it_set), FDS_OK);
    ASSERT_EQ(ntohs(it_set.set->flowset_id), FDS_IPFIX_SET_TMPLT);
    fds_tset_iter_init(&it_tset, it_set.set);
    ASSERT_EQ(fds_tset_iter_next(&it_tset), FDS_OK);
    auto tmplt_flow1 = parse_template(it_tset, FDS_TYPE_TEMPLATE);
    r_flow1.compare_template(tmplt_flow1.get());
    ASSERT_EQ(fds_tset_iter_next(&it_tset), FDS_OK);
    auto tmplt_flow2 = parse_template(it_tset, FDS_TYPE_TEMPLATE);
    r_flow2.compare_template(tmplt_flow2.get());
    EXPECT_EQ(fds_tset_iter_next(&it_tset), FDS_EOC);

    // Expect Options Template Set
    ASSERT_EQ(fds_sets_iter_next(&it_set), FDS_OK);
    ASSERT_EQ(ntohs(it_set.set->flowset_id), FDS_IPFIX_SET_OPTS_TMPLT);
    fds_tset_iter_init(&it_tset, it_set.set);
    ASSERT_EQ(fds_tset_iter_next(&it_tset), FDS_OK);
    auto tmplt_opts = parse_template(it_tset, FDS_TYPE_TEMPLATE_OPTS);
    r_opts1.compare_template(tmplt_opts.get());
    // Note: Unsupported template should not be converted
    EXPECT_EQ(fds_tset_iter_next(&it_tset), FDS_EOC);

    // Expect Data Set
    ASSERT_EQ(fds_sets_iter_next(&it_set), FDS_OK);
    ASSERT_EQ(ntohs(it_set.set->flowset_id), tid_flow1);
    fds_dset_iter_init(&it_dset, it_set.set, tmplt_flow1.get());
    ASSERT_EQ(fds_dset_iter_next(&it_dset), FDS_OK);
    drec = {it_dset.rec, it_dset.size, tmplt_flow1.get(), nullptr};
    r_flow1.compare_data(&drec, VALUE_EXPORT, VALUE_UPTIME);

    // No more records
    EXPECT_EQ(fds_dset_iter_next(&it_dset), FDS_EOC);
    EXPECT_EQ(fds_sets_iter_next(&it_set), FDS_EOC);

    // Create another message but refresh only few templates -------------------
    nf9_set nf9_2a_tset(IPX_NF9_SET_OPTS_TMPLT);
    nf9_2a_tset.add_rec(r_opts2.get_nf9_template()); // Unsupported template
    nf9_2a_tset.add_rec(r_opts1.get_nf9_template());
    nf9_set nf9_2b_tset(IPX_NF9_SET_TMPLT);
    nf9_2b_tset.add_rec(r_flow1.get_nf9_template());
    nf9_set nf9_2c_dset(tid_flow1);
    nf9_2c_dset.add_rec(r_flow1.get_nf9_record());
    nf9_set nf9_2d_dset(tid_opts1);
    nf9_2d_dset.add_rec(r_opts1.get_nf9_record());
    nf9_set nf9_2e_dset(tid_opts2);                  // Data FlowSet with unsupported record
    nf9_2e_dset.add_rec(r_opts2.get_nf9_record());
    nf9_set nf9_2f_dset(tid_flow2);
    nf9_2f_dset.add_rec(r_flow2.get_nf9_record());

    nf9_msg nf9_2;
    nf9_2.set_odid(VALUE_ODID);
    nf9_2.set_time_unix(VALUE_EXPORT);
    nf9_2.set_time_uptime(VALUE_UPTIME);
    nf9_2.set_seq(VALUE_SEQ + 1); // expect seq. number overflow
    nf9_2.add_set(nf9_2a_tset);
    nf9_2.add_set(nf9_2b_tset);
    nf9_2.add_set(nf9_2c_dset);
    nf9_2.add_set(nf9_2d_dset);
    nf9_2.add_set(nf9_2e_dset);
    nf9_2.add_set(nf9_2f_dset);

    // Try to convert the message
    msg_size = nf9_2.size();
    msg_data = (uint8_t *) nf9_2.release();
    prepare_msg(&msg_ctx, msg_data, msg_size);
    ASSERT_EQ(ipx_nf9_conv_process(m_conv.get(), m_msg.get()), IPX_OK);

    // Try to parse the header
    msg_data = ipx_msg_ipfix_get_packet(m_msg.get());
    ipfix_hdr = reinterpret_cast<struct fds_ipfix_msg_hdr*>(msg_data);
    EXPECT_EQ(ntohs(ipfix_hdr->version), FDS_IPFIX_VERSION);
    EXPECT_GE(ntohs(ipfix_hdr->length), FDS_IPFIX_MSG_HDR_LEN);
    EXPECT_EQ(ntohl(ipfix_hdr->odid), VALUE_ODID);
    EXPECT_EQ(ntohl(ipfix_hdr->export_time), VALUE_EXPORT);
    EXPECT_EQ(ntohl(ipfix_hdr->seq_num), 1); // 1 Data Record in the previous message

    // Try to parse the body
    fds_sets_iter_init(&it_set, ipfix_hdr);

    // Expect Options Template Set
    ASSERT_EQ(fds_sets_iter_next(&it_set), FDS_OK);
    ASSERT_EQ(ntohs(it_set.set->flowset_id), FDS_IPFIX_SET_OPTS_TMPLT);
    fds_tset_iter_init(&it_tset, it_set.set);
    ASSERT_EQ(fds_tset_iter_next(&it_tset), FDS_OK);
    tmplt_opts = parse_template(it_tset, FDS_TYPE_TEMPLATE_OPTS);
    // Note: Unsupported template should not be converted
    r_opts1.compare_template(tmplt_opts.get());
    EXPECT_EQ(fds_tset_iter_next(&it_tset), FDS_EOC);

    // Expect Template Set
    ASSERT_EQ(fds_sets_iter_next(&it_set), FDS_OK);
    ASSERT_EQ(ntohs(it_set.set->flowset_id), FDS_IPFIX_SET_TMPLT);
    fds_tset_iter_init(&it_tset, it_set.set);
    ASSERT_EQ(fds_tset_iter_next(&it_tset), FDS_OK);
    tmplt_flow1 = parse_template(it_tset, FDS_TYPE_TEMPLATE);
    r_flow1.compare_template(tmplt_flow1.get());
    EXPECT_EQ(fds_tset_iter_next(&it_tset), FDS_EOC);

    // Expect Data Set
    ASSERT_EQ(fds_sets_iter_next(&it_set), FDS_OK);
    ASSERT_EQ(ntohs(it_set.set->flowset_id), tid_flow1);
    fds_dset_iter_init(&it_dset, it_set.set, tmplt_flow1.get());
    ASSERT_EQ(fds_dset_iter_next(&it_dset), FDS_OK);
    drec = {it_dset.rec, it_dset.size, tmplt_flow1.get(), nullptr};
    r_flow1.compare_data(&drec, VALUE_EXPORT, VALUE_UPTIME);
    EXPECT_EQ(fds_dset_iter_next(&it_dset), FDS_EOC);

    // Expect Data Set
    ASSERT_EQ(fds_sets_iter_next(&it_set), FDS_OK);
    ASSERT_EQ(ntohs(it_set.set->flowset_id), tid_opts1);
    fds_dset_iter_init(&it_dset, it_set.set, tmplt_opts.get());
    ASSERT_EQ(fds_dset_iter_next(&it_dset), FDS_OK);
    drec = {it_dset.rec, it_dset.size, tmplt_opts.get(), nullptr};
    r_opts1.compare_data(&drec, VALUE_EXPORT, VALUE_UPTIME);
    EXPECT_EQ(fds_dset_iter_next(&it_dset), FDS_EOC);

    // Note: Data Set based on unsupported Template should be skipped

    // Expect Data Set
    ASSERT_EQ(fds_sets_iter_next(&it_set), FDS_OK);
    ASSERT_EQ(ntohs(it_set.set->flowset_id), tid_flow2);
    fds_dset_iter_init(&it_dset, it_set.set, tmplt_flow2.get());
    ASSERT_EQ(fds_dset_iter_next(&it_dset), FDS_OK);
    drec = {it_dset.rec, it_dset.size, tmplt_flow2.get(), nullptr};
    r_flow2.compare_data(&drec, VALUE_EXPORT, VALUE_UPTIME);
    EXPECT_EQ(fds_dset_iter_next(&it_dset), FDS_EOC);

    // No more sets
    EXPECT_EQ(fds_sets_iter_next(&it_set), FDS_EOC);
}

/// Try to change definition of a Template
TEST_F(MsgBase, templateRedefine)
{
    // Message context
    const uint32_t VALUE_EXPORT = 1562857357U; // 2019-07-11T15:02:37+00:00
    const uint32_t VALUE_UPTIME = 7278632U;
    const uint32_t VALUE_ODID = 12345678UL;
    uint32_t VALUE_SEQ = 625372U;
    struct ipx_msg_ctx msg_ctx = {m_session.get(), VALUE_ODID, 0};

    // Create a simple NetFlow message -----------------------------------------
    const uint16_t tid_flow1 = 256;
    const uint16_t tid_flow2 = 257;
    const uint16_t tid_opts = 258;

    Rec_norm_multi      r_flow1_a(tid_flow1);
    Rec_norm_enterprise r_flow2(tid_flow2);
    Rec_opts_timestamps r_opts_a(tid_opts);

    nf9_set nf9_1a_tset(IPX_NF9_SET_TMPLT);
    nf9_1a_tset.add_rec(r_flow1_a.get_nf9_template());
    nf9_1a_tset.add_rec(r_flow2.get_nf9_template());
    nf9_set nf9_1b_tset(IPX_NF9_SET_OPTS_TMPLT);
    nf9_1b_tset.add_rec(r_opts_a.get_nf9_template());
    nf9_set nf9_1c_dset(tid_flow1);
    nf9_1c_dset.add_rec(r_flow1_a.get_nf9_record());
    nf9_msg nf9_1;
    nf9_1.set_odid(VALUE_ODID);
    nf9_1.set_time_unix(VALUE_EXPORT);
    nf9_1.set_time_uptime(VALUE_UPTIME);
    nf9_1.set_seq(VALUE_SEQ);
    nf9_1.add_set(nf9_1a_tset);
    nf9_1.add_set(nf9_1b_tset);
    nf9_1.add_set(nf9_1c_dset);

    // Try to convert the message
    uint16_t msg_size = nf9_1.size();
    uint8_t *msg_data = (uint8_t *) nf9_1.release();
    prepare_msg(&msg_ctx, msg_data, msg_size);
    ASSERT_EQ(ipx_nf9_conv_process(m_conv.get(), m_msg.get()), IPX_OK);

    // Try to parse the header
    msg_data = ipx_msg_ipfix_get_packet(m_msg.get());
    auto *ipfix_hdr = reinterpret_cast<struct fds_ipfix_msg_hdr*>(msg_data);
    EXPECT_EQ(ntohs(ipfix_hdr->version), FDS_IPFIX_VERSION);
    EXPECT_GE(ntohs(ipfix_hdr->length), FDS_IPFIX_MSG_HDR_LEN);
    EXPECT_EQ(ntohl(ipfix_hdr->odid), VALUE_ODID);
    EXPECT_EQ(ntohl(ipfix_hdr->export_time), VALUE_EXPORT);
    EXPECT_EQ(ntohl(ipfix_hdr->seq_num), 0);

    // Try to parse the body
    struct fds_sets_iter it_set;
    struct fds_tset_iter it_tset;
    struct fds_dset_iter it_dset;
    struct fds_drec drec;
    fds_sets_iter_init(&it_set, ipfix_hdr);

    // Expect Template Set
    ASSERT_EQ(fds_sets_iter_next(&it_set), FDS_OK);
    ASSERT_EQ(ntohs(it_set.set->flowset_id), FDS_IPFIX_SET_TMPLT);
    fds_tset_iter_init(&it_tset, it_set.set);
    ASSERT_EQ(fds_tset_iter_next(&it_tset), FDS_OK);
    auto tmplt_flow1_a = parse_template(it_tset, FDS_TYPE_TEMPLATE);
    r_flow1_a.compare_template(tmplt_flow1_a.get());
    ASSERT_EQ(fds_tset_iter_next(&it_tset), FDS_OK);
    auto tmplt_flow2 = parse_template(it_tset, FDS_TYPE_TEMPLATE);
    r_flow2.compare_template(tmplt_flow2.get());
    EXPECT_EQ(fds_tset_iter_next(&it_tset), FDS_EOC);

    // Expect Options Template Set
    ASSERT_EQ(fds_sets_iter_next(&it_set), FDS_OK);
    ASSERT_EQ(ntohs(it_set.set->flowset_id), FDS_IPFIX_SET_OPTS_TMPLT);
    fds_tset_iter_init(&it_tset, it_set.set);
    ASSERT_EQ(fds_tset_iter_next(&it_tset), FDS_OK);
    auto tmplt_opts = parse_template(it_tset, FDS_TYPE_TEMPLATE_OPTS);
    r_opts_a.compare_template(tmplt_opts.get());
    EXPECT_EQ(fds_tset_iter_next(&it_tset), FDS_EOC);

    // Expect Data Set
    ASSERT_EQ(fds_sets_iter_next(&it_set), FDS_OK);
    ASSERT_EQ(ntohs(it_set.set->flowset_id), tid_flow1);
    fds_dset_iter_init(&it_dset, it_set.set, tmplt_flow1_a.get());
    ASSERT_EQ(fds_dset_iter_next(&it_dset), FDS_OK);
    drec = {it_dset.rec, it_dset.size, tmplt_flow1_a.get(), nullptr};
    r_flow1_a.compare_data(&drec, VALUE_EXPORT, VALUE_UPTIME);

    // No more records
    EXPECT_EQ(fds_dset_iter_next(&it_dset), FDS_EOC);
    EXPECT_EQ(fds_sets_iter_next(&it_set), FDS_EOC);

    // Create another message but with redefined templates ---------------------
    Rec_norm_basic  r_flow1_b(tid_flow1);
    Rec_norm_nots   r_flow3_b(tid_opts);

    nf9_set nf9_2a_tset(IPX_NF9_SET_TMPLT);
    nf9_2a_tset.add_rec(r_flow1_b.get_nf9_template());  // redefine the previous Template
    nf9_2a_tset.add_rec(r_flow3_b.get_nf9_template());    // redefine the previous Opts. Template
    nf9_set nf9_2b_dset(tid_flow1);
    nf9_2b_dset.add_rec(r_flow1_b.get_nf9_record());
    nf9_2b_dset.add_rec(r_flow1_b.get_nf9_record());
    nf9_set nf9_2c_dset(tid_opts); // now it is "normal template" (i.e. not Options)
    nf9_2c_dset.add_rec(r_flow3_b.get_nf9_record());
    nf9_set nf9_2d_dset(tid_flow2);
    nf9_2d_dset.add_rec(r_flow2.get_nf9_record());
    nf9_msg nf9_2;
    nf9_2.set_odid(VALUE_ODID);
    nf9_2.set_time_unix(VALUE_EXPORT);
    nf9_2.set_time_uptime(VALUE_UPTIME);
    nf9_2.set_seq(VALUE_SEQ + 1);
    nf9_2.add_set(nf9_2a_tset);
    nf9_2.add_set(nf9_2b_dset);
    nf9_2.add_set(nf9_2c_dset);
    nf9_2.add_set(nf9_2d_dset);

    // Try to convert the message
    msg_size = nf9_2.size();
    msg_data = (uint8_t *) nf9_2.release();
    prepare_msg(&msg_ctx, msg_data, msg_size);
    ASSERT_EQ(ipx_nf9_conv_process(m_conv.get(), m_msg.get()), IPX_OK);

    // Try to parse the header
    msg_data = ipx_msg_ipfix_get_packet(m_msg.get());
    ipfix_hdr = reinterpret_cast<struct fds_ipfix_msg_hdr*>(msg_data);
    EXPECT_EQ(ntohs(ipfix_hdr->version), FDS_IPFIX_VERSION);
    EXPECT_GE(ntohs(ipfix_hdr->length), FDS_IPFIX_MSG_HDR_LEN);
    EXPECT_EQ(ntohl(ipfix_hdr->odid), VALUE_ODID);
    EXPECT_EQ(ntohl(ipfix_hdr->export_time), VALUE_EXPORT);
    EXPECT_EQ(ntohl(ipfix_hdr->seq_num), 1); // 1 Data Record in the previous message

    // Try to parse the body
    fds_sets_iter_init(&it_set, ipfix_hdr);

    // Expect Template Set
    ASSERT_EQ(fds_sets_iter_next(&it_set), FDS_OK);
    ASSERT_EQ(ntohs(it_set.set->flowset_id), FDS_IPFIX_SET_TMPLT);
    fds_tset_iter_init(&it_tset, it_set.set);
    ASSERT_EQ(fds_tset_iter_next(&it_tset), FDS_OK);
    auto tmplt_flow1_b = parse_template(it_tset, FDS_TYPE_TEMPLATE);
    r_flow1_b.compare_template(tmplt_flow1_b.get());
    ASSERT_EQ(fds_tset_iter_next(&it_tset), FDS_OK);
    auto tmplt_flow3_b = parse_template(it_tset, FDS_TYPE_TEMPLATE);
    r_flow3_b.compare_template(tmplt_flow3_b.get());
    EXPECT_EQ(fds_tset_iter_next(&it_tset), FDS_EOC);

    // Expect Data Set
    ASSERT_EQ(fds_sets_iter_next(&it_set), FDS_OK);
    ASSERT_EQ(ntohs(it_set.set->flowset_id), tid_flow1);
    fds_dset_iter_init(&it_dset, it_set.set, tmplt_flow1_b.get());
    ASSERT_EQ(fds_dset_iter_next(&it_dset), FDS_OK);
    drec = {it_dset.rec, it_dset.size, tmplt_flow1_b.get(), nullptr};
    r_flow1_b.compare_data(&drec, VALUE_EXPORT, VALUE_UPTIME);
    ASSERT_EQ(fds_dset_iter_next(&it_dset), FDS_OK);
    drec = {it_dset.rec, it_dset.size, tmplt_flow1_b.get(), nullptr};
    r_flow1_b.compare_data(&drec, VALUE_EXPORT, VALUE_UPTIME);
    EXPECT_EQ(fds_dset_iter_next(&it_dset), FDS_EOC);

    // Expect Data Set
    ASSERT_EQ(fds_sets_iter_next(&it_set), FDS_OK);
    ASSERT_EQ(ntohs(it_set.set->flowset_id), tid_opts);
    fds_dset_iter_init(&it_dset, it_set.set, tmplt_flow3_b.get());
    ASSERT_EQ(fds_dset_iter_next(&it_dset), FDS_OK);
    drec = {it_dset.rec, it_dset.size, tmplt_flow3_b.get(), nullptr};
    r_flow3_b.compare_data(&drec, VALUE_EXPORT, VALUE_UPTIME);
    EXPECT_EQ(fds_dset_iter_next(&it_dset), FDS_EOC);

    // Expect Data Set
    ASSERT_EQ(fds_sets_iter_next(&it_set), FDS_OK);
    ASSERT_EQ(ntohs(it_set.set->flowset_id), tid_flow2);
    fds_dset_iter_init(&it_dset, it_set.set, tmplt_flow2.get());
    ASSERT_EQ(fds_dset_iter_next(&it_dset), FDS_OK);
    drec = {it_dset.rec, it_dset.size, tmplt_flow2.get(), nullptr};
    r_flow2.compare_data(&drec, VALUE_EXPORT, VALUE_UPTIME);
    EXPECT_EQ(fds_dset_iter_next(&it_dset), FDS_EOC);

    // No more sets
    EXPECT_EQ(fds_sets_iter_next(&it_set), FDS_EOC);
}

/// Try to convert messages with partly missing Template definitions (add them later)
TEST_F(MsgBase, missingTemplateDefinitions)
{
    // Message context
    const uint32_t VALUE_EXPORT = 1562857357U; // 2019-07-11T15:02:37+00:00
    const uint32_t VALUE_UPTIME = 7278632U;
    const uint32_t VALUE_ODID = 1;
    uint32_t VALUE_SEQ = 2632172U;
    struct ipx_msg_ctx msg_ctx = {m_session.get(), VALUE_ODID, 0};

    // Create a simple NetFlow message (without previous template definitions)
    const uint16_t tid_flow1 = 256;
    const uint16_t tid_flow2 = 257;
    const uint16_t tid_opts = 258;

    Rec_norm_multi      r_flow1(tid_flow1);
    Rec_norm_enterprise r_flow2(tid_flow2);
    Rec_opts_timestamps r_opts(tid_opts);

    nf9_set nf9_1a_dset(tid_flow1);
    nf9_1a_dset.add_rec(r_flow1.get_nf9_record());
    nf9_1a_dset.add_rec(r_flow1.get_nf9_record());
    nf9_set nf9_1b_dset(tid_flow2);
    nf9_1b_dset.add_rec(r_flow2.get_nf9_record());
    nf9_set nf9_1c_dset(tid_opts);
    nf9_1c_dset.add_rec(r_opts.get_nf9_record());
    nf9_1c_dset.add_rec(r_opts.get_nf9_record());
    nf9_msg nf9_1;
    nf9_1.set_odid(VALUE_ODID);
    nf9_1.set_time_unix(VALUE_EXPORT);
    nf9_1.set_time_uptime(VALUE_UPTIME);
    nf9_1.set_seq(VALUE_SEQ);
    nf9_1.add_set(nf9_1a_dset);
    nf9_1.add_set(nf9_1b_dset);
    nf9_1.add_set(nf9_1c_dset);

    // Try to convert the message
    uint16_t msg_size = nf9_1.size();
    uint8_t *msg_data = (uint8_t *) nf9_1.release();
    prepare_msg(&msg_ctx, msg_data, msg_size);
    ASSERT_EQ(ipx_nf9_conv_process(m_conv.get(), m_msg.get()), IPX_OK);

    // Try to parse the header
    msg_data = ipx_msg_ipfix_get_packet(m_msg.get());
    auto *ipfix_hdr = reinterpret_cast<struct fds_ipfix_msg_hdr*>(msg_data);
    EXPECT_EQ(ntohs(ipfix_hdr->version), FDS_IPFIX_VERSION);
    EXPECT_GE(ntohs(ipfix_hdr->length), FDS_IPFIX_MSG_HDR_LEN);
    EXPECT_EQ(ntohl(ipfix_hdr->odid), VALUE_ODID);
    EXPECT_EQ(ntohl(ipfix_hdr->export_time), VALUE_EXPORT);
    EXPECT_EQ(ntohl(ipfix_hdr->seq_num), 0);

    // Try to parse the body
    struct fds_sets_iter it_set;
    struct fds_tset_iter it_tset;
    struct fds_dset_iter it_dset;
    struct fds_drec drec;
    fds_sets_iter_init(&it_set, ipfix_hdr);

    // Expect empty message (no Data Sets can be converted without previous Template definition)
    EXPECT_EQ(fds_sets_iter_next(&it_set), FDS_EOC);

    // Create another message but define only few templates -------------------
    nf9_set nf9_2a_tset(IPX_NF9_SET_OPTS_TMPLT);
    nf9_2a_tset.add_rec(r_opts.get_nf9_template());
    nf9_set nf9_2b_tset(IPX_NF9_SET_TMPLT);
    nf9_2b_tset.add_rec(r_flow1.get_nf9_template());
    nf9_set nf9_2c_dset(tid_flow1);
    nf9_2c_dset.add_rec(r_flow1.get_nf9_record());
    nf9_set nf9_2d_dset(tid_opts);
    nf9_2d_dset.add_rec(r_opts.get_nf9_record());
    nf9_set nf9_2e_dset(tid_flow2);   // note: template is still undefined
    nf9_2e_dset.add_rec(r_flow2.get_nf9_record());
    nf9_msg nf9_2;
    nf9_2.set_odid(VALUE_ODID);
    nf9_2.set_time_unix(VALUE_EXPORT);
    nf9_2.set_time_uptime(VALUE_UPTIME);
    nf9_2.set_seq(VALUE_SEQ + 1); // expect seq. number overflow
    nf9_2.add_set(nf9_2a_tset);
    nf9_2.add_set(nf9_2b_tset);
    nf9_2.add_set(nf9_2c_dset);
    nf9_2.add_set(nf9_2d_dset);
    nf9_2.add_set(nf9_2e_dset);

    // Try to convert the message
    msg_size = nf9_2.size();
    msg_data = (uint8_t *) nf9_2.release();
    prepare_msg(&msg_ctx, msg_data, msg_size);
    ASSERT_EQ(ipx_nf9_conv_process(m_conv.get(), m_msg.get()), IPX_OK);

    // Try to parse the header
    msg_data = ipx_msg_ipfix_get_packet(m_msg.get());
    ipfix_hdr = reinterpret_cast<struct fds_ipfix_msg_hdr*>(msg_data);
    EXPECT_EQ(ntohs(ipfix_hdr->version), FDS_IPFIX_VERSION);
    EXPECT_GE(ntohs(ipfix_hdr->length), FDS_IPFIX_MSG_HDR_LEN);
    EXPECT_EQ(ntohl(ipfix_hdr->odid), VALUE_ODID);
    EXPECT_EQ(ntohl(ipfix_hdr->export_time), VALUE_EXPORT);
    EXPECT_EQ(ntohl(ipfix_hdr->seq_num), 0); // 0 Data Records in the previous message

    // Try to parse the body
    fds_sets_iter_init(&it_set, ipfix_hdr);

    // Expect Options Template Set
    ASSERT_EQ(fds_sets_iter_next(&it_set), FDS_OK);
    ASSERT_EQ(ntohs(it_set.set->flowset_id), FDS_IPFIX_SET_OPTS_TMPLT);
    fds_tset_iter_init(&it_tset, it_set.set);
    ASSERT_EQ(fds_tset_iter_next(&it_tset), FDS_OK);
    auto tmplt_opts = parse_template(it_tset, FDS_TYPE_TEMPLATE_OPTS);
    r_opts.compare_template(tmplt_opts.get());
    EXPECT_EQ(fds_tset_iter_next(&it_tset), FDS_EOC);

    // Expect Template Set
    ASSERT_EQ(fds_sets_iter_next(&it_set), FDS_OK);
    ASSERT_EQ(ntohs(it_set.set->flowset_id), FDS_IPFIX_SET_TMPLT);
    fds_tset_iter_init(&it_tset, it_set.set);
    ASSERT_EQ(fds_tset_iter_next(&it_tset), FDS_OK);
    auto tmplt_flow1 = parse_template(it_tset, FDS_TYPE_TEMPLATE);
    r_flow1.compare_template(tmplt_flow1.get());
    EXPECT_EQ(fds_tset_iter_next(&it_tset), FDS_EOC);

    // Expect Data Set
    ASSERT_EQ(fds_sets_iter_next(&it_set), FDS_OK);
    ASSERT_EQ(ntohs(it_set.set->flowset_id), tid_flow1);
    fds_dset_iter_init(&it_dset, it_set.set, tmplt_flow1.get());
    ASSERT_EQ(fds_dset_iter_next(&it_dset), FDS_OK);
    drec = {it_dset.rec, it_dset.size, tmplt_flow1.get(), nullptr};
    r_flow1.compare_data(&drec, VALUE_EXPORT, VALUE_UPTIME);
    EXPECT_EQ(fds_dset_iter_next(&it_dset), FDS_EOC);

    // Expect Data Set
    ASSERT_EQ(fds_sets_iter_next(&it_set), FDS_OK);
    ASSERT_EQ(ntohs(it_set.set->flowset_id), tid_opts);
    fds_dset_iter_init(&it_dset, it_set.set, tmplt_opts.get());
    ASSERT_EQ(fds_dset_iter_next(&it_dset), FDS_OK);
    drec = {it_dset.rec, it_dset.size, tmplt_opts.get(), nullptr};
    r_opts.compare_data(&drec, VALUE_EXPORT, VALUE_UPTIME);
    EXPECT_EQ(fds_dset_iter_next(&it_dset), FDS_EOC);

    // No more sets (the last one must be ignored due to missing Template definition)
    EXPECT_EQ(fds_sets_iter_next(&it_set), FDS_EOC);

    // Create another message (define the last Template) -----------------------
    nf9_set nf9_3a_tset(IPX_NF9_SET_TMPLT);
    nf9_3a_tset.add_rec(r_flow2.get_nf9_template());
    nf9_set nf9_3b_dset(tid_flow1);
    nf9_3b_dset.add_rec(r_flow1.get_nf9_record());
    nf9_3b_dset.add_rec(r_flow1.get_nf9_record());
    nf9_set nf9_3c_dset(tid_flow2);
    nf9_3c_dset.add_rec(r_flow2.get_nf9_record());
    nf9_3c_dset.add_rec(r_flow2.get_nf9_record());
    nf9_msg nf9_3;
    nf9_3.set_odid(VALUE_ODID);
    nf9_3.set_time_unix(VALUE_EXPORT + 100);
    nf9_3.set_time_uptime(VALUE_UPTIME + 100);
    nf9_3.set_seq(VALUE_SEQ + 2); // expect seq. number overflow
    nf9_3.add_set(nf9_3a_tset);
    nf9_3.add_set(nf9_3b_dset);
    nf9_3.add_set(nf9_3c_dset);

    // Try to convert the message
    msg_size = nf9_3.size();
    msg_data = (uint8_t *) nf9_3.release();
    prepare_msg(&msg_ctx, msg_data, msg_size);
    ASSERT_EQ(ipx_nf9_conv_process(m_conv.get(), m_msg.get()), IPX_OK);

    // Try to parse the header
    msg_data = ipx_msg_ipfix_get_packet(m_msg.get());
    ipfix_hdr = reinterpret_cast<struct fds_ipfix_msg_hdr*>(msg_data);
    EXPECT_EQ(ntohs(ipfix_hdr->version), FDS_IPFIX_VERSION);
    EXPECT_GE(ntohs(ipfix_hdr->length), FDS_IPFIX_MSG_HDR_LEN);
    EXPECT_EQ(ntohl(ipfix_hdr->odid), VALUE_ODID);
    EXPECT_EQ(ntohl(ipfix_hdr->export_time), VALUE_EXPORT + 100);
    EXPECT_EQ(ntohl(ipfix_hdr->seq_num), 2); // 2 Data Records in the previous messages

    // Try to parse the body
    fds_sets_iter_init(&it_set, ipfix_hdr);

    // Expect Template Set
    ASSERT_EQ(fds_sets_iter_next(&it_set), FDS_OK);
    ASSERT_EQ(ntohs(it_set.set->flowset_id), FDS_IPFIX_SET_TMPLT);
    fds_tset_iter_init(&it_tset, it_set.set);
    ASSERT_EQ(fds_tset_iter_next(&it_tset), FDS_OK);
    auto tmplt_flow2 = parse_template(it_tset, FDS_TYPE_TEMPLATE);
    r_flow2.compare_template(tmplt_flow2.get());
    EXPECT_EQ(fds_tset_iter_next(&it_tset), FDS_EOC);

    // Expect Data Set
    ASSERT_EQ(fds_sets_iter_next(&it_set), FDS_OK);
    ASSERT_EQ(ntohs(it_set.set->flowset_id), tid_flow1);
    fds_dset_iter_init(&it_dset, it_set.set, tmplt_flow1.get());
    ASSERT_EQ(fds_dset_iter_next(&it_dset), FDS_OK);
    drec = {it_dset.rec, it_dset.size, tmplt_flow1.get(), nullptr};
    r_flow1.compare_data(&drec, VALUE_EXPORT + 100, VALUE_UPTIME + 100);
    ASSERT_EQ(fds_dset_iter_next(&it_dset), FDS_OK);
    drec = {it_dset.rec, it_dset.size, tmplt_flow1.get(), nullptr};
    r_flow1.compare_data(&drec, VALUE_EXPORT + 100, VALUE_UPTIME + 100);
    EXPECT_EQ(fds_dset_iter_next(&it_dset), FDS_EOC);

    // Expect Data Set
    ASSERT_EQ(fds_sets_iter_next(&it_set), FDS_OK);
    ASSERT_EQ(ntohs(it_set.set->flowset_id), tid_flow2);
    fds_dset_iter_init(&it_dset, it_set.set, tmplt_flow2.get());
    ASSERT_EQ(fds_dset_iter_next(&it_dset), FDS_OK);
    drec = {it_dset.rec, it_dset.size, tmplt_flow2.get(), nullptr};
    r_flow2.compare_data(&drec, VALUE_EXPORT + 100, VALUE_UPTIME + 100);
    ASSERT_EQ(fds_dset_iter_next(&it_dset), FDS_OK);
    drec = {it_dset.rec, it_dset.size, tmplt_flow2.get(), nullptr};
    r_flow2.compare_data(&drec, VALUE_EXPORT + 100, VALUE_UPTIME + 100);
    EXPECT_EQ(fds_dset_iter_next(&it_dset), FDS_EOC);

    // No more sets
    EXPECT_EQ(fds_sets_iter_next(&it_set), FDS_EOC);
}

// Convert out-of-order messages
TEST_F(MsgBase, outOfOrderMessages)
{
    // Message context
    const uint32_t VALUE_EXPORT = 1562857357U; // 2019-07-11T15:02:37+00:00
    const uint32_t VALUE_UPTIME = 7278632U;
    const uint32_t VALUE_ODID = 12345678UL;
    uint32_t VALUE_SEQ = 625372U;
    struct ipx_msg_ctx msg_ctx = {m_session.get(), VALUE_ODID, 0};

    // Create a simple NetFlow message -----------------------------------------
    const uint16_t tid_flow1 = 256;
    const uint16_t tid_flow2 = 257;

    Rec_norm_multi      r_flow1(tid_flow1);
    Rec_norm_enterprise r_flow2(tid_flow2);

    nf9_set nf9_1a_tset(IPX_NF9_SET_TMPLT);
    nf9_1a_tset.add_rec(r_flow1.get_nf9_template());
    nf9_1a_tset.add_rec(r_flow2.get_nf9_template());
    nf9_set nf9_1b_dset(tid_flow1);
    nf9_1b_dset.add_rec(r_flow1.get_nf9_record());
    nf9_msg nf9_1;
    nf9_1.set_odid(VALUE_ODID);
    nf9_1.set_time_unix(VALUE_EXPORT);
    nf9_1.set_time_uptime(VALUE_UPTIME);
    nf9_1.set_seq(VALUE_SEQ);
    nf9_1.add_set(nf9_1a_tset);
    nf9_1.add_set(nf9_1b_dset);

    // Try to convert the message
    uint16_t msg_size = nf9_1.size();
    uint8_t *msg_data = (uint8_t *) nf9_1.release();
    prepare_msg(&msg_ctx, msg_data, msg_size);
    ASSERT_EQ(ipx_nf9_conv_process(m_conv.get(), m_msg.get()), IPX_OK);

    // Try to parse the header
    msg_data = ipx_msg_ipfix_get_packet(m_msg.get());
    auto *ipfix_hdr = reinterpret_cast<struct fds_ipfix_msg_hdr*>(msg_data);
    EXPECT_EQ(ntohs(ipfix_hdr->version), FDS_IPFIX_VERSION);
    EXPECT_GE(ntohs(ipfix_hdr->length), FDS_IPFIX_MSG_HDR_LEN);
    EXPECT_EQ(ntohl(ipfix_hdr->odid), VALUE_ODID);
    EXPECT_EQ(ntohl(ipfix_hdr->export_time), VALUE_EXPORT);
    EXPECT_EQ(ntohl(ipfix_hdr->seq_num), 0);

    // Try to parse the body
    struct fds_sets_iter it_set;
    struct fds_tset_iter it_tset;
    struct fds_dset_iter it_dset;
    struct fds_drec drec;
    fds_sets_iter_init(&it_set, ipfix_hdr);

    // Expect Template Set
    ASSERT_EQ(fds_sets_iter_next(&it_set), FDS_OK);
    ASSERT_EQ(ntohs(it_set.set->flowset_id), FDS_IPFIX_SET_TMPLT);
    fds_tset_iter_init(&it_tset, it_set.set);
    ASSERT_EQ(fds_tset_iter_next(&it_tset), FDS_OK);
    auto tmplt_flow1 = parse_template(it_tset, FDS_TYPE_TEMPLATE);
    r_flow1.compare_template(tmplt_flow1.get());
    ASSERT_EQ(fds_tset_iter_next(&it_tset), FDS_OK);
    auto tmplt_flow2 = parse_template(it_tset, FDS_TYPE_TEMPLATE);
    r_flow2.compare_template(tmplt_flow2.get());
    EXPECT_EQ(fds_tset_iter_next(&it_tset), FDS_EOC);

    // Expect Data Set (content should be ok, if all previous test has passed)
    ASSERT_EQ(fds_sets_iter_next(&it_set), FDS_OK);
    ASSERT_EQ(ntohs(it_set.set->flowset_id), tid_flow1);

    // No more Sets
    EXPECT_EQ(fds_sets_iter_next(&it_set), FDS_EOC);

    // Create a NetFlow message (with sequence number from the future) ---------
    nf9_set nf9_2a_dset(tid_flow1);
    nf9_2a_dset.add_rec(r_flow1.get_nf9_record());
    nf9_set nf9_2b_dset(tid_flow2);
    nf9_2b_dset.add_rec(r_flow2.get_nf9_record());
    nf9_msg nf9_2;
    nf9_2.set_odid(VALUE_ODID);
    nf9_2.set_time_unix(VALUE_EXPORT);
    nf9_2.set_time_uptime(VALUE_UPTIME);
    nf9_2.set_seq(VALUE_SEQ + 10); // Go forward 10 messages
    nf9_2.add_set(nf9_2a_dset);
    nf9_2.add_set(nf9_2b_dset);

    // Try to convert the message
    msg_size = nf9_2.size();
    msg_data = (uint8_t *) nf9_2.release();
    prepare_msg(&msg_ctx, msg_data, msg_size);
    ASSERT_EQ(ipx_nf9_conv_process(m_conv.get(), m_msg.get()), IPX_OK);

    // Try to parse the header
    msg_data = ipx_msg_ipfix_get_packet(m_msg.get());
    ipfix_hdr = reinterpret_cast<struct fds_ipfix_msg_hdr*>(msg_data);
    EXPECT_EQ(ntohs(ipfix_hdr->version), FDS_IPFIX_VERSION);
    EXPECT_GE(ntohs(ipfix_hdr->length), FDS_IPFIX_MSG_HDR_LEN);
    EXPECT_EQ(ntohl(ipfix_hdr->odid), VALUE_ODID);
    EXPECT_EQ(ntohl(ipfix_hdr->export_time), VALUE_EXPORT);
    // New IPFIX sequence number should not be affected by the out-of-order message
    EXPECT_EQ(ntohl(ipfix_hdr->seq_num), 1); // 1 Data Record in the previous message

    // Try to parse the body
    fds_sets_iter_init(&it_set, ipfix_hdr);

    // Expect Data Set
    ASSERT_EQ(fds_sets_iter_next(&it_set), FDS_OK);
    ASSERT_EQ(ntohs(it_set.set->flowset_id), tid_flow1);
    fds_dset_iter_init(&it_dset, it_set.set, tmplt_flow1.get());
    ASSERT_EQ(fds_dset_iter_next(&it_dset), FDS_OK);
    drec = {it_dset.rec, it_dset.size, tmplt_flow1.get(), nullptr};
    r_flow1.compare_data(&drec, VALUE_EXPORT, VALUE_UPTIME);
    EXPECT_EQ(fds_dset_iter_next(&it_dset), FDS_EOC);

    // Expect Data Set
    ASSERT_EQ(fds_sets_iter_next(&it_set), FDS_OK);
    ASSERT_EQ(ntohs(it_set.set->flowset_id), tid_flow2);
    fds_dset_iter_init(&it_dset, it_set.set, tmplt_flow2.get());
    ASSERT_EQ(fds_dset_iter_next(&it_dset), FDS_OK);
    drec = {it_dset.rec, it_dset.size, tmplt_flow2.get(), nullptr};
    r_flow2.compare_data(&drec, VALUE_EXPORT, VALUE_UPTIME);
    EXPECT_EQ(fds_dset_iter_next(&it_dset), FDS_EOC);

    // No more Sets
    EXPECT_EQ(fds_sets_iter_next(&it_set), FDS_EOC);

    // Create an empty NetFlow Message (with the following sequence number)
    nf9_msg nf9_3;
    nf9_3.set_odid(VALUE_ODID);
    nf9_3.set_time_unix(VALUE_EXPORT);
    nf9_3.set_time_uptime(VALUE_UPTIME);
    nf9_3.set_seq(VALUE_SEQ + 11);       // Message that follows after the previous message

    // Try to convert the message
    msg_size = nf9_3.size();
    msg_data = (uint8_t *) nf9_3.release();
    prepare_msg(&msg_ctx, msg_data, msg_size);
    ASSERT_EQ(ipx_nf9_conv_process(m_conv.get(), m_msg.get()), IPX_OK);

    // Try to parse the header
    msg_data = ipx_msg_ipfix_get_packet(m_msg.get());
    ipfix_hdr = reinterpret_cast<struct fds_ipfix_msg_hdr*>(msg_data);
    EXPECT_EQ(ntohs(ipfix_hdr->version), FDS_IPFIX_VERSION);
    EXPECT_EQ(ntohs(ipfix_hdr->length), FDS_IPFIX_MSG_HDR_LEN);
    EXPECT_EQ(ntohl(ipfix_hdr->odid), VALUE_ODID);
    EXPECT_EQ(ntohl(ipfix_hdr->export_time), VALUE_EXPORT);
    // New IPFIX sequence number should not be affected by the out-of-order message
    EXPECT_EQ(ntohl(ipfix_hdr->seq_num), 3); // 1 + 2 Data Records in the previous messages

    // Create a NetFlow message (with sequence number from the past) -----------
    nf9_set nf9_4a_dset(tid_flow1);
    nf9_4a_dset.add_rec(r_flow1.get_nf9_record());
    nf9_set nf9_4b_dset(tid_flow2);
    nf9_4b_dset.add_rec(r_flow2.get_nf9_record());
    nf9_msg nf9_4;
    nf9_4.set_odid(VALUE_ODID);
    nf9_4.set_time_unix(VALUE_EXPORT);
    nf9_4.set_time_uptime(VALUE_UPTIME);
    nf9_4.set_seq(VALUE_SEQ - 10);       // Go back 20 messages
    nf9_4.add_set(nf9_4a_dset);
    nf9_4.add_set(nf9_4b_dset);

    // Try to convert the message
    msg_size = nf9_4.size();
    msg_data = (uint8_t *) nf9_4.release();
    prepare_msg(&msg_ctx, msg_data, msg_size);
    ASSERT_EQ(ipx_nf9_conv_process(m_conv.get(), m_msg.get()), IPX_OK);

    // Try to parse the header
    msg_data = ipx_msg_ipfix_get_packet(m_msg.get());
    ipfix_hdr = reinterpret_cast<struct fds_ipfix_msg_hdr*>(msg_data);
    EXPECT_EQ(ntohs(ipfix_hdr->version), FDS_IPFIX_VERSION);
    EXPECT_GE(ntohs(ipfix_hdr->length), FDS_IPFIX_MSG_HDR_LEN);
    EXPECT_EQ(ntohl(ipfix_hdr->odid), VALUE_ODID);
    EXPECT_EQ(ntohl(ipfix_hdr->export_time), VALUE_EXPORT);
    // New IPFIX sequence number should not be affected by the out-of-order message
    EXPECT_EQ(ntohl(ipfix_hdr->seq_num), 3); // 1 + 2 + 0 Data Records in the previous messages

    // Try to parse the body
    fds_sets_iter_init(&it_set, ipfix_hdr);

    // Expect Data Set
    ASSERT_EQ(fds_sets_iter_next(&it_set), FDS_OK);
    ASSERT_EQ(ntohs(it_set.set->flowset_id), tid_flow1);
    fds_dset_iter_init(&it_dset, it_set.set, tmplt_flow1.get());
    ASSERT_EQ(fds_dset_iter_next(&it_dset), FDS_OK);
    drec = {it_dset.rec, it_dset.size, tmplt_flow1.get(), nullptr};
    r_flow1.compare_data(&drec, VALUE_EXPORT, VALUE_UPTIME);
    EXPECT_EQ(fds_dset_iter_next(&it_dset), FDS_EOC);

    // Expect Data Set
    ASSERT_EQ(fds_sets_iter_next(&it_set), FDS_OK);
    ASSERT_EQ(ntohs(it_set.set->flowset_id), tid_flow2);
    fds_dset_iter_init(&it_dset, it_set.set, tmplt_flow2.get());
    ASSERT_EQ(fds_dset_iter_next(&it_dset), FDS_OK);
    drec = {it_dset.rec, it_dset.size, tmplt_flow2.get(), nullptr};
    r_flow2.compare_data(&drec, VALUE_EXPORT, VALUE_UPTIME);
    EXPECT_EQ(fds_dset_iter_next(&it_dset), FDS_EOC);

    // No more Sets
    EXPECT_EQ(fds_sets_iter_next(&it_set), FDS_EOC);
}

// Try to convert NetFlow Message with unknown FlowSets (i.e. 1 < ID < 256)
// Unknown FlowSets should be skipped/ignored!
TEST_F(MsgBase, unknownFlowSets)
{
    // Message context
    const uint32_t VALUE_EXPORT = 1562857357U; // 2019-07-11T15:02:37+00:00
    const uint32_t VALUE_UPTIME = 7278632U;
    const uint32_t VALUE_ODID = 12345678UL;
    uint32_t VALUE_SEQ = 625372U;
    struct ipx_msg_ctx msg_ctx = {m_session.get(), VALUE_ODID, 0};

    // Create a simple NetFlow message -----------------------------------------
    uint16_t set_id1 = 2;   // Unknown
    uint16_t set_id2 = IPX_NF9_SET_TMPLT; // Template FlowSet
    uint16_t set_id3 = 255; // Unknown
    uint16_t set_id4 = 256; // Data FlowSet

    Rec_norm_multi r_flow(set_id4);

    nf9_set nf9_un1(set_id1);  // Unknown FlowSet
    nf9_un1.add_padding(40);
    nf9_set nf9_tset(set_id2);
    nf9_tset.add_rec(r_flow.get_nf9_template());
    nf9_set nf9_un2(set_id3);  // Unknown FlowSet
    nf9_un2.add_padding(1234);
    nf9_set nf9_dset(set_id4);
    nf9_dset.add_rec(r_flow.get_nf9_record());

    nf9_msg nf9;
    nf9.set_odid(VALUE_ODID);
    nf9.set_time_unix(VALUE_EXPORT);
    nf9.set_time_uptime(VALUE_UPTIME);
    nf9.set_seq(VALUE_SEQ);
    nf9.add_set(nf9_un1);
    nf9.add_set(nf9_tset);
    nf9.add_set(nf9_un2);
    nf9.add_set(nf9_dset);

    // Try to convert it
    uint16_t msg_size = nf9.size();
    uint8_t *msg_data = (uint8_t *) nf9.release();
    prepare_msg(&msg_ctx, msg_data, msg_size);
    ASSERT_EQ(ipx_nf9_conv_process(m_conv.get(), m_msg.get()), IPX_OK);

    // Try to parse the message
    msg_data = ipx_msg_ipfix_get_packet(m_msg.get());
    auto *ipfix_hdr = reinterpret_cast<struct fds_ipfix_msg_hdr*>(msg_data);
    EXPECT_EQ(ntohs(ipfix_hdr->version), FDS_IPFIX_VERSION);
    EXPECT_GE(ntohs(ipfix_hdr->length), FDS_IPFIX_MSG_HDR_LEN);
    EXPECT_EQ(ntohl(ipfix_hdr->odid), VALUE_ODID);
    EXPECT_EQ(ntohl(ipfix_hdr->export_time), VALUE_EXPORT);
    EXPECT_EQ(ntohl(ipfix_hdr->seq_num), 0);

    // Try to parse the body (unknown FlowSets should be skipped!)
    struct fds_sets_iter it_set;
    fds_sets_iter_init(&it_set, ipfix_hdr);

    // Expect Template Set
    ASSERT_EQ(fds_sets_iter_next(&it_set), FDS_OK);
    ASSERT_EQ(ntohs(it_set.set->flowset_id), FDS_IPFIX_SET_TMPLT);
    fds_tset_iter it_tset;
    fds_tset_iter_init(&it_tset, it_set.set);
    ASSERT_EQ(fds_tset_iter_next(&it_tset), FDS_OK);
    // Parse the Template
    auto tmplt = parse_template(it_tset, FDS_TYPE_TEMPLATE);
    r_flow.compare_template(tmplt.get());
    // Expect no more Templates in the Set
    EXPECT_EQ(fds_tset_iter_next(&it_tset), FDS_EOC);

    // Expect Data Set
    ASSERT_EQ(fds_sets_iter_next(&it_set), FDS_OK);
    ASSERT_EQ(ntohs(it_set.set->flowset_id), set_id4);
    struct fds_dset_iter it_dset;
    fds_dset_iter_init(&it_dset, it_set.set, tmplt.get());
    ASSERT_EQ(fds_dset_iter_next(&it_dset), FDS_OK);

    struct fds_drec drec = {it_dset.rec, it_dset.size, tmplt.get(), nullptr};
    r_flow.compare_data(&drec, VALUE_EXPORT, VALUE_UPTIME);

    // No more records
    EXPECT_EQ(fds_dset_iter_next(&it_dset), FDS_EOC);
    EXPECT_EQ(fds_sets_iter_next(&it_set), FDS_EOC);
}

// Conversion of flow record based on inapropriate template
TEST_F(MsgBase, templateAndRecordMismatch)
{
    // Message context
    const uint32_t VALUE_EXPORT = 1562857357U; // 2019-07-11T15:02:37+00:00
    const uint32_t VALUE_UPTIME = 10001;
    const uint32_t VALUE_ODID = 0;
    uint32_t VALUE_SEQ = 625372U;
    struct ipx_msg_ctx msg_ctx = {m_session.get(), VALUE_ODID, 0};

    // Create a simple NetFlow Message
    uint16_t tid = 256;
    Rec_norm_basic rec_multi(tid);
    Rec_norm_multi rec_norm(tid);

    nf9_set nf9_tset(IPX_NF9_SET_TMPLT);
    nf9_tset.add_rec(rec_norm.get_nf9_template());
    nf9_set nf9_dset(tid);
    nf9_dset.add_rec(rec_multi.get_nf9_record());
    nf9_msg nf9;
    nf9.set_odid(VALUE_ODID);
    nf9.set_time_unix(VALUE_EXPORT);
    nf9.set_time_uptime(VALUE_UPTIME);
    nf9.set_seq(VALUE_SEQ);
    nf9.add_set(nf9_tset);
    nf9.add_set(nf9_dset);

    // Try to convert the message
    uint16_t msg_size = nf9.size();
    uint8_t *msg_data = (uint8_t *) nf9.release();
    prepare_msg(&msg_ctx, msg_data, msg_size);
    ASSERT_EQ(ipx_nf9_conv_process(m_conv.get(), m_msg.get()), IPX_ERR_FORMAT);
}

// Try to convert non-NetFlow message
TEST_F(MsgBase, conversionOfNonNetFlow)
{
    const uint32_t VALUE_ODID = 12110;
    struct ipx_msg_ctx msg_ctx = {m_session.get(), VALUE_ODID, 0};

    // Prepare invalid data
    const uint16_t size = 512;
    uint8_t *mem = (uint8_t *) malloc(size);
    ASSERT_NE(mem, nullptr);
    memset(mem, 0, size);

    // Try to convert the data
    uint16_t msg_size = size;
    uint8_t *msg_data = mem;
    prepare_msg(&msg_ctx, msg_data, msg_size);
    ASSERT_EQ(ipx_nf9_conv_process(m_conv.get(), m_msg.get()), IPX_ERR_FORMAT);
}

// Try to convert too short NetFlow message (shorter than the message header)
TEST_F(MsgBase, conversionOfTooShortMessage)
{
    // Message context
    const uint32_t VALUE_EXPORT = 1562857357U; // 2019-07-11T15:02:37+00:00
    const uint32_t VALUE_UPTIME = 10001;
    const uint32_t VALUE_ODID = 0;
    uint32_t VALUE_SEQ = 625372U;
    struct ipx_msg_ctx msg_ctx = {m_session.get(), VALUE_ODID, 0};

    // Create a simple (valid) NetFlow Message
    nf9_msg nf9;
    nf9.set_odid(VALUE_ODID);
    nf9.set_time_unix(VALUE_EXPORT);
    nf9.set_time_uptime(VALUE_UPTIME);
    nf9.set_seq(VALUE_SEQ);

    // Try to convert the message, but trim last few bytes
    uint16_t msg_size = nf9.size();
    uint8_t *msg_data = (uint8_t *) nf9.release();

    for (uint16_t new_size = 0; new_size < msg_size; ++new_size) {
        SCOPED_TRACE("Size: " + std::to_string(new_size));

        // Create a new converter for each message
        converter_create(IPX_VERB_DEBUG);

        uint8_t *new_data = (uint8_t *) malloc(new_size);
        memcpy(new_data, msg_data, new_size);
        prepare_msg(&msg_ctx, new_data, new_size);
        ASSERT_EQ(ipx_nf9_conv_process(m_conv.get(), m_msg.get()), IPX_ERR_FORMAT);
    }

    free(msg_data);
}

// Try to convert Template FlowSet with invalid Template ID (< 256)
TEST_F(MsgBase, invalidTemplateID)
{
    // Create a record and its context
    const uint32_t VALUE_EXPORT = 1562857357U; // 2019-07-11T15:02:37+00:00
    const uint32_t VALUE_UPTIME = 10001;
    const uint32_t VALUE_ODID = 10;
    struct ipx_msg_ctx msg_ctx = {m_session.get(), VALUE_ODID, 0};

    uint16_t tid = 255; // lower than expected
    Rec_norm_enterprise rec(tid);

    nf9_set nf9_tset(IPX_NF9_SET_TMPLT);
    nf9_tset.add_rec(rec.get_nf9_template());
    nf9_msg nf9;
    nf9.set_odid(VALUE_ODID);
    nf9.set_time_unix(VALUE_EXPORT);
    nf9.set_time_uptime(VALUE_UPTIME);
    nf9.add_set(nf9_tset);

    // Try to convert the message
    uint16_t msg_size = nf9.size();
    uint8_t *msg_data = (uint8_t *) nf9.release();
    prepare_msg(&msg_ctx, msg_data, msg_size);
    ASSERT_EQ(ipx_nf9_conv_process(m_conv.get(), m_msg.get()), IPX_ERR_FORMAT);
}

// Invalid size of field in the Template definition
// i.e. Data Record would be longer than max. possible NetFlow Message
TEST_F(MsgBase, longTemplateDataLength)
{
    // Create a record and its context
    const uint32_t VALUE_EXPORT = 1562857357U; // 2019-07-11T15:02:37+00:00
    const uint32_t VALUE_UPTIME = 10001;
    const uint32_t VALUE_ODID = 10;
    struct ipx_msg_ctx msg_ctx = {m_session.get(), VALUE_ODID, 0};

    Rec_norm_basic rec(256);

    nf9_set nf9_tset(IPX_NF9_SET_TMPLT);
    nf9_tset.add_rec(rec.get_nf9_template());
    nf9_msg nf9;
    nf9.set_odid(VALUE_ODID);
    nf9.set_time_unix(VALUE_EXPORT);
    nf9.set_time_uptime(VALUE_UPTIME);
    nf9.add_set(nf9_tset);

    // Try to convert the message
    uint16_t msg_size = nf9.size();
    uint8_t *msg_data = (uint8_t *) nf9.release();
    // Overwrite size of few template fields
    struct ipx_nf9_tset *tset = (struct ipx_nf9_tset *) (msg_data + IPX_NF9_MSG_HDR_LEN);
    struct ipx_nf9_trec *trec = &tset->first_record;
    trec->fields[0].length = htons(20196);
    trec->fields[1].length = htons(27324);
    trec->fields[2].length = htons(10000);
    trec->fields[3].length = htons(20120);

    prepare_msg(&msg_ctx, msg_data, msg_size);
    ASSERT_EQ(ipx_nf9_conv_process(m_conv.get(), m_msg.get()), IPX_ERR_FORMAT);
}

// Invalid size of field in the Template definition (zero size Data Record)
TEST_F(MsgBase, zeroSizeTemplateDataLength)
{
    // Create an invalid Data Record
    class Rec_empty : public Rec_base {
    public:
        Rec_empty(uint16_t tid) {
            // Create a zero Template with few zero size fields -> size of Data Record is also 0
            add_field_octets(IPX_NF9_IE_IN_BYTES,       0, nullptr);
            add_field_octets(IPX_NF9_IE_IN_PKTS,        0, nullptr);
            add_field_octets(IPX_NF9_IE_IPV4_SRC_ADDR,  0, nullptr);
            add_field_octets(IPX_NF9_IE_IPV4_DST_ADDR,  0, nullptr);
            build(tid);
        }
    };

    // Create a record and its context
    const uint32_t VALUE_EXPORT = 1562857357U; // 2019-07-11T15:02:37+00:00
    const uint32_t VALUE_UPTIME = 10001;
    const uint32_t VALUE_ODID = 10;
    struct ipx_msg_ctx msg_ctx = {m_session.get(), VALUE_ODID, 0};

    Rec_empty rec(256);

    nf9_set nf9_tset(IPX_NF9_SET_TMPLT);
    nf9_tset.add_rec(rec.get_nf9_template());
    nf9_msg nf9;
    nf9.set_odid(VALUE_ODID);
    nf9.set_time_unix(VALUE_EXPORT);
    nf9.set_time_uptime(VALUE_UPTIME);
    nf9.add_set(nf9_tset);

    // Try to convert the message
    uint16_t msg_size = nf9.size();
    uint8_t *msg_data = (uint8_t *) nf9.release();
    prepare_msg(&msg_ctx, msg_data, msg_size);
    ASSERT_EQ(ipx_nf9_conv_process(m_conv.get(), m_msg.get()), IPX_ERR_FORMAT);
}

// Conversion of very long NetFlow message (converted IPFIX Message is too long)
// Due to additional information, IPFIX Message can exceed its max. size 2^16
TEST_F(MsgBase, tooLongIPFIX)
{
    // Create a record and its context
    const uint32_t VALUE_EXPORT = 1562857357U; // 2019-07-11T15:02:37+00:00
    const uint32_t VALUE_UPTIME = 10001;
    const uint32_t VALUE_ODID = 10;
    struct ipx_msg_ctx msg_ctx = {m_session.get(), VALUE_ODID, 0};

    uint16_t tid = 256;
    Rec_norm_basic rec(tid);

    /* Try to create a max. size NetFlow message (i.e. up to 2^16)
     * NF9 header = 20B
     * Template FlowSet header = 4B
     * Template def (basic) = 36B
     * Data FlowSet header = 4B
     * N x Data Record (basic i.e. 28B per rec) -> N = 2338 basic records
     */

    nf9_set nf9_tset(IPX_NF9_SET_TMPLT);
    nf9_tset.add_rec(rec.get_nf9_template());
    nf9_set nf9_dset(tid);
    for (size_t i = 0; i < 2338U; ++i) {
        nf9_dset.add_rec(rec.get_nf9_record());
    }
    nf9_msg nf9;
    nf9.set_odid(VALUE_ODID);
    nf9.set_time_unix(VALUE_EXPORT);
    nf9.set_time_uptime(VALUE_UPTIME);
    nf9.add_set(nf9_tset);
    nf9.add_set(nf9_dset);

    // Try to convert the message
    uint16_t msg_size = nf9.size();
    uint8_t *msg_data = (uint8_t *) nf9.release();
    prepare_msg(&msg_ctx, msg_data, msg_size);
    ASSERT_EQ(ipx_nf9_conv_process(m_conv.get(), m_msg.get()), IPX_ERR_FORMAT);
}

// Unexpected end of a Template definition
TEST_F(MsgBase, unexpectedEndOfTemplate)
{
    // Create a record and its context
    const uint32_t VALUE_EXPORT = 1562857357U; // 2019-07-11T15:02:37+00:00
    const uint32_t VALUE_UPTIME = 10001;
    const uint32_t VALUE_ODID = 10;
    struct ipx_msg_ctx msg_ctx = {m_session.get(), VALUE_ODID, 0};

    uint16_t tid = 3452;
    Rec_norm_enterprise rec_norm(tid);

    nf9_set nf9_tset(IPX_NF9_SET_TMPLT);
    nf9_tset.add_rec(rec_norm.get_nf9_template());
    const uint16_t hdr_size = IPX_NF9_MSG_HDR_LEN;
    uint16_t set_size = nf9_tset.size();

    for (uint16_t i = 1; i < set_size; ++i) {
        // Prepare a NetFlow message with too short Template FlowSet
        SCOPED_TRACE("FlowSet size: " + std::to_string(i));
        nf9_tset.overwrite_len(i);    // Override FlowSet size

        nf9_msg nf9;
        nf9.set_odid(VALUE_ODID);
        nf9.set_time_unix(VALUE_EXPORT);
        nf9.set_time_uptime(VALUE_UPTIME);
        nf9.add_set(nf9_tset);
        uint8_t *msg_orig = (uint8_t *) nf9.release();

        // Create a copy of the original message with reduced size for valgrind check
        uint16_t new_size = hdr_size + i;
        uint8_t *new_msg = (uint8_t *) malloc(new_size);
        ASSERT_NE(new_msg, nullptr);
        memcpy(new_msg, msg_orig, new_size);
        free(msg_orig);

        converter_create(IPX_VERB_DEBUG); // Create a new converter for each message
        prepare_msg(&msg_ctx, new_msg, new_size);
        ASSERT_EQ(ipx_nf9_conv_process(m_conv.get(), m_msg.get()), IPX_ERR_FORMAT);
    }

    // Now do the same thing for Options Template
    Rec_opts_simple rec_opts(tid);
    nf9_set nf9_tset_opt(IPX_NF9_SET_OPTS_TMPLT);
    nf9_tset_opt.add_rec(rec_opts.get_nf9_template());
    set_size = nf9_tset_opt.size();

    for (uint16_t i = 1; i < set_size; ++i) {
        // Prepare a NetFlow message with too short Template FlowSet
        SCOPED_TRACE("FlowSet size: " + std::to_string(i));
        nf9_tset_opt.overwrite_len(i);    // Override FlowSet size

        nf9_msg nf9;
        nf9.set_odid(VALUE_ODID);
        nf9.set_time_unix(VALUE_EXPORT);
        nf9.set_time_uptime(VALUE_UPTIME);
        nf9.add_set(nf9_tset_opt);
        uint8_t *msg_orig = (uint8_t *) nf9.release();

        // Create a copy of the original message with reduced size for valgrind check
        uint16_t new_size = hdr_size + i;
        uint8_t *new_msg = (uint8_t *) malloc(new_size);
        ASSERT_NE(new_msg, nullptr);
        memcpy(new_msg, msg_orig, new_size);
        free(msg_orig);

        converter_create(IPX_VERB_DEBUG); // Create a new converter for each message
        prepare_msg(&msg_ctx, new_msg, new_size);
        ASSERT_EQ(ipx_nf9_conv_process(m_conv.get(), m_msg.get()), IPX_ERR_FORMAT);
    }
}

// Template definition without fields (zero field count)
TEST_F(MsgBase, invalidTemplateDef)
{
    // Create a record and its context
    const uint32_t VALUE_EXPORT = 1562857357U; // 2019-07-11T15:02:37+00:00
    const uint32_t VALUE_UPTIME = 10001;
    const uint32_t VALUE_ODID = 10;
    struct ipx_msg_ctx msg_ctx = {m_session.get(), VALUE_ODID, 0};

    Rec_norm_basic rec(256);

    nf9_set nf9_tset(IPX_NF9_SET_TMPLT);
    nf9_tset.add_rec(rec.get_nf9_template());
    nf9_msg nf9;
    nf9.set_odid(VALUE_ODID);
    nf9.set_time_unix(VALUE_EXPORT);
    nf9.set_time_uptime(VALUE_UPTIME);
    nf9.add_set(nf9_tset);

    // Try to convert the message
    uint16_t msg_size = nf9.size();
    uint8_t *msg_data = (uint8_t *) nf9.release();
    // Overwrite field count
    struct ipx_nf9_tset *tset = (struct ipx_nf9_tset *) (msg_data + IPX_NF9_MSG_HDR_LEN);
    struct ipx_nf9_trec *trec = &tset->first_record;
    trec->count = htons(0);

    prepare_msg(&msg_ctx, msg_data, msg_size);
    ASSERT_EQ(ipx_nf9_conv_process(m_conv.get(), m_msg.get()), IPX_ERR_FORMAT);
}

// Options Template with invalid definition of Scope fields (not multiple of 4, etc.)
TEST_F(MsgBase, invalidOptionsTemplateDef)
{
    // Create a record and its context
    const uint32_t VALUE_EXPORT = 1562857357U; // 2019-07-11T15:02:37+00:00
    const uint32_t VALUE_UPTIME = 10001;
    const uint32_t VALUE_ODID = 10;
    struct ipx_msg_ctx msg_ctx = {m_session.get(), VALUE_ODID, 0};

    Rec_opts_simple rec(256);

    nf9_set nf9_tset(IPX_NF9_SET_OPTS_TMPLT);
    nf9_tset.add_rec(rec.get_nf9_template());
    nf9_msg nf9;
    nf9.set_odid(VALUE_ODID);
    nf9.set_time_unix(VALUE_EXPORT);
    nf9.set_time_uptime(VALUE_UPTIME);
    nf9.add_set(nf9_tset);

    uint16_t msg_size = nf9.size();
    uint8_t *msg_orig = (uint8_t *) nf9.release();
    uint8_t *msg_data;
    struct ipx_nf9_opts_tset *tset;

    // -------- Try to convert a message with zero scope fields -----------
    msg_data = (uint8_t *) malloc(msg_size);
    ASSERT_NE(msg_data, nullptr);
    memcpy(msg_data, msg_orig, msg_size);
    tset = (struct ipx_nf9_opts_tset *) (msg_data + IPX_NF9_MSG_HDR_LEN);
    tset->first_record.scope_length = htons(0); // << Zero size

    converter_create(IPX_VERB_DEBUG);
    prepare_msg(&msg_ctx, msg_data, msg_size);
    // NOTE: RFC3954 doesn't say that at least one field is required -> it is probably ok
    ASSERT_EQ(ipx_nf9_conv_process(m_conv.get(), m_msg.get()), IPX_OK);

    // ------- Try to convert a message with invalid size of Scope fields -----------
    msg_data = (uint8_t *) malloc(msg_size);
    ASSERT_NE(msg_data, nullptr);
    memcpy(msg_data, msg_orig, msg_size);
    tset = (struct ipx_nf9_opts_tset *) (msg_data + IPX_NF9_MSG_HDR_LEN);
    tset->first_record.scope_length = htons(6); // << Not a multiple of 4

    converter_create(IPX_VERB_DEBUG);
    prepare_msg(&msg_ctx, msg_data, msg_size);
    ASSERT_EQ(ipx_nf9_conv_process(m_conv.get(), m_msg.get()), IPX_ERR_FORMAT);

    // ------ Try to convert a message with zero scope fields -----------
    msg_data = (uint8_t *) malloc(msg_size);
    ASSERT_NE(msg_data, nullptr);
    memcpy(msg_data, msg_orig, msg_size);
    tset = (struct ipx_nf9_opts_tset *) (msg_data + IPX_NF9_MSG_HDR_LEN);
    tset->first_record.option_length = htons(7); // << Not a multiple of 4

    converter_create(IPX_VERB_DEBUG);
    prepare_msg(&msg_ctx, msg_data, msg_size);
    ASSERT_EQ(ipx_nf9_conv_process(m_conv.get(), m_msg.get()), IPX_ERR_FORMAT);

    free(msg_orig);
}

// Unexpected end of Data FlowSet
TEST_F(MsgBase, unexpectedEndOfDataSet)
{
    // Create a record and its context
    const uint32_t VALUE_EXPORT = 1562857357U; // 2019-07-11T15:02:37+00:00
    const uint32_t VALUE_UPTIME = 10001;
    const uint32_t VALUE_ODID = 10;
    struct ipx_msg_ctx msg_ctx = {m_session.get(), VALUE_ODID, 0};

    uint16_t tid = 256;
    Rec_norm_enterprise rec(tid);

    nf9_set nf9_tset(IPX_NF9_SET_TMPLT);
    nf9_tset.add_rec(rec.get_nf9_template());
    nf9_set nf9_dset(tid);
    nf9_dset.add_rec(rec.get_nf9_record());
    nf9_msg nf9;
    nf9.set_odid(VALUE_ODID);
    nf9.set_time_unix(VALUE_EXPORT);
    nf9.set_time_uptime(VALUE_UPTIME);
    nf9.add_set(nf9_tset);
    nf9.add_set(nf9_dset);

    // Try to convert the message
    uint16_t msg_size = nf9.size();
    uint8_t *msg_data = (uint8_t *) nf9.release();

    for (uint16_t i = 1; i < nf9_dset.size(); ++i) {
        // Create a copy and try different Data FlowSet sizes
        SCOPED_TRACE("Removed (bytes): " + std::to_string(i));

        uint16_t new_size = msg_size - i;
        uint8_t *new_msg = (uint8_t *) malloc(new_size);
        ASSERT_NE(new_msg, nullptr);
        memcpy(new_msg, msg_data, new_size);

        converter_create(IPX_VERB_DEBUG);
        prepare_msg(&msg_ctx, new_msg, new_size);
        ASSERT_EQ(ipx_nf9_conv_process(m_conv.get(), m_msg.get()), IPX_ERR_FORMAT);
    }

    free(msg_data);
}
#include <gtest/gtest.h>
#include <MsgGen.h>
#include <libfds.h>

extern "C" {
    #include <core/context.h>
    #include <core/message_ipfix.h>
    #include <core/modifier.h>
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

/** \brief Class for storing data record with parsed template */
class Data {
private:
    fds_template *tmplt;
public:
    fds_drec rec;

    Data() {
        // Parse template
        const uint16_t tmplt_id = 256;
        ipfix_trec trec(tmplt_id);
        trec.add_field(1, 4);               // bytes
        trec.add_field(2, 4);               // packets
        uint16_t tmplt_size = trec.size();
        uint8_t *tmplt_raw = trec.release();
        fds_template_parse(FDS_TYPE_TEMPLATE, tmplt_raw, &tmplt_size, &tmplt);
        free(tmplt_raw);
        assert(tmplt != NULL);

        // Create data record
        ipfix_drec drec;
        drec.append_uint(1000, 4);
        drec.append_uint(20, 4);

        rec.size = drec.size();
        rec.data = drec.release();
        rec.tmplt = tmplt;
        rec.snap = NULL;
    }

    ~Data() {
        free(rec.data);
        fds_template_destroy(tmplt);
    }
};

/** \brief Class creating all types of transport session based on net configuration */
class Session {
private:
    ipx_session *sessions[4];
public:
    Session(ipx_session_net net_cfg, const std::string filepath) {
        sessions[FDS_SESSION_TCP]  = ipx_session_new_tcp(&net_cfg);
        sessions[FDS_SESSION_UDP]  = ipx_session_new_udp(&net_cfg, 0, 0);
        sessions[FDS_SESSION_SCTP] = ipx_session_new_sctp(&net_cfg);
        sessions[FDS_SESSION_FILE] = ipx_session_new_file(filepath.c_str());
    };

    ~Session() {
        for (int i = 0; i < 4; i++) {
            ipx_session_destroy(sessions[i]);
        }
    };

    ipx_session *getSession(enum fds_session_type type) {
        return sessions[type];
    }
};

/** \brief Adder function appending first field */
int adder_one(const struct fds_drec *rec, struct ipx_modifier_output output[], void *cb_data) {
    (void) rec, (void) cb_data;
    output[0].length = 4;
    return 0;
}

/** \brief Adder function appending second field */
int adder_two(const struct fds_drec *rec, struct ipx_modifier_output output[], void *cb_data) {
    (void) rec, (void) cb_data;
    output[0].length = 4;
    output[1].length = 4;
    return 0;
}

/** \brief Adder function appending both fields */
int adder_three(const struct fds_drec *rec, struct ipx_modifier_output output[], void *cb_data) {
    (void) rec, (void) cb_data;
    output[0].length = 4;
    output[1].length = 4;
    output[2].length = 4;
    return 0;
}

/**
 * \brief Main testing fixture for testing modifier component
 */
class Common : public ::testing::Test {
protected:
    using ctx_uniq = std::unique_ptr<ipx_ctx_t, decltype(&ipx_ctx_destroy)>;
    using mod_uniq = std::unique_ptr<struct ipx_modifier, decltype(&ipx_modifier_destroy)>;

    ipx_modifier *mod;
    ipx_ctx_t *ctx;
    ipx_session_net net_cfg;
    Data *rec; // Parsed data record + template

    // Setup
    void SetUp() override {
        static const std::string ident = "Testing module";
        static const ipx_verb_level DEF_VERB = IPX_VERB_INFO;
        static const ipx_modifier_field fields[] = {{50, 4, 0}, {60, 4, 0}, {70, 4, 0}};
        static const size_t fields_cnt = sizeof(fields) / sizeof(*fields);

        // Initialize (plugin) context and modifier
        ctx_uniq     ctx_wrap(ipx_ctx_create(ident.c_str(), nullptr), &ipx_ctx_destroy);
        mod_uniq     mod_wrap(ipx_modifier_create(fields, fields_cnt, NULL, NULL, &DEF_VERB, ident.c_str()),
                        &ipx_modifier_destroy);

        mod = mod_wrap.release();
        ctx = ctx_wrap.release();

        setNetCfg();

        // Initialize data record
        rec = new Data();
    }

    // Tear Down
    void TearDown() override {
        delete rec;
        ipx_ctx_destroy(ctx);
        ipx_modifier_destroy(mod);
    }

    /** \brief Set network configuration for transport session */
    void setNetCfg(uint16_t sport = 1000, uint16_t dport = 1000) {
        net_cfg.port_src = sport;
        net_cfg.port_dst = dport;
        net_cfg.l3_proto = AF_INET;
        ASSERT_EQ(inet_pton(AF_INET, "192.168.0.2", &net_cfg.addr_src.ipv4), 1);
        ASSERT_EQ(inet_pton(AF_INET, "192.168.0.1", &net_cfg.addr_src.ipv4), 1);
    }

    /** \brief Set modifier context with empty IPFIX message */
    void addSession(ipx_session *session, uint16_t odid = 0, uint32_t export_time = 0, int rc = IPX_OK) {
        // Create message context
        ipx_msg_ctx msg_ctx;
        msg_ctx.session = session;
        msg_ctx.odid = odid;

        // Create new parsed IPFIX message with given context
        ipfix_msg msg;
        uint16_t msg_size = msg.size();
        uint8_t *msg_data = reinterpret_cast<uint8_t *>(msg.release());
        ipx_msg_ipfix_t *ipfix_message = ipx_msg_ipfix_create(ctx, &msg_ctx, msg_data, msg_size);
        ASSERT_NE(ipfix_message, nullptr);

        if (export_time) {
            fds_ipfix_msg_hdr *hdr = (fds_ipfix_msg_hdr *) msg_data;
            hdr->export_time = htonl(export_time);
        }

        // Set modifier context and destroy message
        ipx_msg_garbage_t *garbage;
        ASSERT_EQ(ipx_modifier_add_session(mod, ipfix_message, &garbage), rc);
        if (garbage) {
            ipx_msg_garbage_destroy(garbage);
        }
        ipx_msg_ipfix_destroy(ipfix_message);
    }

    /** \brief Modify record with given callback */
    void modifyRecord(modifier_adder_cb_t adder_cb) {
        ipx_msg_garbage_t *gb_msg;

        // Modify record
        fds_drec *modified_rec = NULL;
        ipx_modifier_set_adder_cb(mod, adder_cb);
        modified_rec = ipx_modifier_modify(mod, &(rec->rec), &gb_msg);
        EXPECT_NE(modified_rec, nullptr);

        // Destroy garbage if it exists
        if (gb_msg) {
            ipx_msg_garbage_destroy(gb_msg);
        }

        // Free memory
        free(modified_rec->data);
        free(modified_rec);
    }

    /** \brief Check for (non) existance of template in modifiers current template manager */
    void findTemplate(int rc, uint16_t tmplt_id, uint16_t field_cnt = 0) {
        // Get Manager
        const fds_tmgr *mgr = ipx_modifier_get_manager(mod);
        ASSERT_NE(mgr, nullptr);

        // Check template
        const struct fds_template *tmplt = NULL;
        ASSERT_EQ(fds_tmgr_template_get((fds_tmgr_t *) mgr, tmplt_id, &tmplt), rc);
        if (rc == FDS_OK) {
            EXPECT_EQ(tmplt->fields_cnt_total, field_cnt);
        }
    }
};

/** \brief Class for parametrized testing single sessions in modifier */
class SingleSession: public Common, public ::testing::WithParamInterface<enum fds_session_type> {
protected:
    using session_uniq = std::unique_ptr<struct ipx_session, decltype(&ipx_session_destroy)>;
    ipx_session *session;

    // Setup
    void SetUp() override {
        Common::SetUp();

        // Create session based on test parameter
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

        session = session_wrap.release();
    };

    // Cleanup
    void TearDown() override {
        ipx_session_destroy(session);
        Common::TearDown();
    }
};

/** \brief Class used for testing mixed sessions types */
class MixedSessions: public Common {
protected:
    /** \brief Create a Sessions object */
    Session* createSessions(std::string filename, uint16_t sport = 1000, uint16_t dport = 1000) {
        setNetCfg(sport, dport);
        return new Session(net_cfg, filename);
    }

    /** \brief Remove session from modifier, check number of remaining sessions*/
    void removeSession(ipx_session *session, size_t remain, int rc = IPX_OK) {
        ipx_msg_garbage_t *gb_msg;
        ASSERT_EQ(ipx_modifier_remove_session(mod, session, &gb_msg), rc);
        ASSERT_NE(gb_msg, nullptr);
        EXPECT_EQ(mod->sessions.ctx_valid, remain);
        ipx_msg_garbage_destroy(gb_msg);
    }
};

/** \brief Class used for testing session errors in modifier*/
class ErrorSession: public MixedSessions {
    // Just using different name ...
};

// Define parameters of parametrized test
INSTANTIATE_TEST_SUITE_P(Modifier, SingleSession, ::testing::Values(FDS_SESSION_UDP,
    FDS_SESSION_TCP, FDS_SESSION_SCTP, FDS_SESSION_FILE));

// Simple test for creating and destroying modifier
TEST_F(Common, createAndDestroy) {
    ASSERT_NE(mod, nullptr);
    EXPECT_EQ(ipx_modifier_get_manager(mod), nullptr);
    EXPECT_EQ(mod->sessions.ctx_valid, 0u);
    EXPECT_EQ(mod->sessions.ctx_alloc, IPX_MODIFIER_DEF_CTX);
}

/** Single session and ODID session created */
TEST_P(SingleSession, createSingleODID) {
    // Add new session
    addSession(session);

    // Attempt to add new template into modifier
    modifyRecord(&adder_one);
    findTemplate(FDS_OK, 256, 3);
    findTemplate(FDS_ERR_NOTFOUND, 257);

    // Change adder callback so the other field is added and new template created (from same message)
    modifyRecord(&adder_two);
    findTemplate(FDS_OK, 257, 4);

    // Try to add the same message with same session (should not create new session context)
    addSession(session);
    findTemplate(FDS_OK, 256, 3);
    findTemplate(FDS_OK, 257, 4);
    findTemplate(FDS_ERR_NOTFOUND, 258);

    // Add another template and check its existance
    modifyRecord(&adder_three);
    findTemplate(FDS_OK, 258, 5);
}

/** Single session but multiple ODIDs created */
TEST_P(SingleSession, createDifferentODID) {
    // Add new session with ODID 0
    addSession(session);
    modifyRecord(&adder_one);
    findTemplate(FDS_OK, 256, 3);

    // Add new session with ODID 100
    addSession(session, 100);
    modifyRecord(&adder_two);
    findTemplate(FDS_OK, 256, 4);

    // Add new session with ODID 20
    addSession(session, 20);
    findTemplate(FDS_ERR_NOTFOUND, 256);

    // Check if sorted by ODID
    ASSERT_EQ(mod->sessions.ctx_valid, 3u);
    EXPECT_EQ(mod->sessions.ctx[0].odid, 0u);
    EXPECT_EQ(mod->sessions.ctx[1].odid, 20u);
    EXPECT_EQ(mod->sessions.ctx[2].odid, 100u);
}

/** Delete existing session with many ODIDs (should delete all of them) */
TEST_P(SingleSession, deleteExisting) {
    // Add new session with different ODIDs
    const size_t session_cnt = 10;
    for (size_t i = 0; i < session_cnt; i++) {
        addSession(session, i);
    }
    ASSERT_EQ(mod->sessions.ctx_valid, session_cnt);

    // Remove all contexts (single session but different ODIDs)
    ipx_msg_garbage_t *gb_msg;
    ASSERT_EQ(ipx_modifier_remove_session(mod, session, &gb_msg), IPX_OK);
    ASSERT_NE(gb_msg, nullptr);

    EXPECT_EQ(mod->sessions.ctx_valid, 0u);
    EXPECT_EQ(mod->curr_ctx, nullptr);

    ipx_msg_garbage_destroy(gb_msg);
}

/** Create multiple sessions, access their templates ... */
TEST_F(MixedSessions, createDifferentTypes) {
    // Create sessions
    Session *sessions = createSessions("test_session");

    // Add new TCP session
    addSession(sessions->getSession(FDS_SESSION_TCP));
    modifyRecord(&adder_one);
    modifyRecord(&adder_three);
    findTemplate(FDS_OK, 256, 3);
    findTemplate(FDS_OK, 257, 5);
    findTemplate(FDS_ERR_NOTFOUND, 258);

    // Add new UDP session
    addSession(sessions->getSession(FDS_SESSION_UDP));
    findTemplate(FDS_ERR_NOTFOUND, 256);

    // Add new SCTP session
    addSession(sessions->getSession(FDS_SESSION_SCTP));
    modifyRecord(&adder_three);
    findTemplate(FDS_OK, 256, 5);

    // Add FILE session
    addSession(sessions->getSession(FDS_SESSION_FILE));

    // Add new data to previously created UDP session
    addSession(sessions->getSession(FDS_SESSION_UDP));
    modifyRecord(&adder_one);
    modifyRecord(&adder_three);
    findTemplate(FDS_OK, 256, 3);
    findTemplate(FDS_OK, 257, 5);

    // Add more data to TCP session
    addSession(sessions->getSession(FDS_SESSION_TCP));
    modifyRecord(&adder_two);
    findTemplate(FDS_OK, 258, 4);

    EXPECT_EQ(mod->sessions.ctx_valid, 4u);
    delete sessions;
}

/** Create multiple sessions but with same type (TCP) */
TEST_F(MixedSessions, createSameType) {
    // Create array of sessions
    const size_t session_cnt = 4;
    Session *sessions[session_cnt];
    for (size_t i = 0; i < session_cnt; i++) {
        sessions[i] = createSessions("session");
    }

    // Add last created TCP session to test if sessions are
    // sorted by transport session
    addSession(sessions[3]->getSession(FDS_SESSION_TCP));
    modifyRecord(&adder_one);
    findTemplate(FDS_OK, 256, 3);

    // Add second TCP session with new session parameters
    addSession(sessions[2]->getSession(FDS_SESSION_TCP));
    findTemplate(FDS_ERR_NOTFOUND, 256);
    modifyRecord(&adder_three);
    findTemplate(FDS_OK, 256, 5);

    // Add third TCP session with new session parameters
    addSession(sessions[1]->getSession(FDS_SESSION_TCP));
    findTemplate(FDS_ERR_NOTFOUND, 256);

    // Add another record to second session
    addSession(sessions[2]->getSession(FDS_SESSION_TCP));
    modifyRecord(&adder_one);
    findTemplate(FDS_OK, 257, 3);

    // Add last TCP session
    addSession(sessions[0]->getSession(FDS_SESSION_TCP));
    findTemplate(FDS_ERR_NOTFOUND, 256);

    // Check order
    EXPECT_EQ(mod->sessions.ctx_valid, session_cnt);
    for (size_t i = 0; i < session_cnt; i++) {
        EXPECT_EQ(mod->sessions.ctx[i].session, sessions[i]->getSession(FDS_SESSION_TCP));
    }

    // Free memory
    for (size_t i = 0; i < session_cnt; i++) {
        delete sessions[i];
    }
}

/** Create multiple sessions with different types and ODIDs and gradually remove all of them */
TEST_F(MixedSessions, deleteMultipleTypesAndODIDs) {
    Session* sessions1 = createSessions("filename");
    Session* sessions2 = createSessions("filename2");

    // Add bunch of sessions
    addSession(sessions1->getSession(FDS_SESSION_TCP),  100);
    addSession(sessions1->getSession(FDS_SESSION_TCP),   30);
    addSession(sessions1->getSession(FDS_SESSION_TCP),    0);

    addSession(sessions1->getSession(FDS_SESSION_SCTP),   0);
    addSession(sessions1->getSession(FDS_SESSION_SCTP), 100);
    addSession(sessions1->getSession(FDS_SESSION_SCTP),  20);

    addSession(sessions2->getSession(FDS_SESSION_FILE), 100);
    addSession(sessions2->getSession(FDS_SESSION_FILE), 400);

    addSession(sessions2->getSession(FDS_SESSION_UDP),   80);
    addSession(sessions2->getSession(FDS_SESSION_UDP),  100);

    addSession(sessions1->getSession(FDS_SESSION_UDP),   30);
    addSession(sessions1->getSession(FDS_SESSION_UDP),   20);
    addSession(sessions1->getSession(FDS_SESSION_UDP),   60);
    addSession(sessions1->getSession(FDS_SESSION_UDP),    0);

    addSession(sessions1->getSession(FDS_SESSION_FILE),   0);

    ASSERT_EQ(mod->sessions.ctx_valid, 15u);

    // Delete them
    removeSession(sessions1->getSession(FDS_SESSION_UDP), 11u);
    removeSession(sessions2->getSession(FDS_SESSION_UDP), 9u);
    removeSession(sessions1->getSession(FDS_SESSION_TCP), 6u);
    removeSession(sessions1->getSession(FDS_SESSION_FILE), 5u);
    removeSession(sessions1->getSession(FDS_SESSION_SCTP), 2u);
    removeSession(sessions2->getSession(FDS_SESSION_FILE), 0u);

    delete sessions1;
    delete sessions2;
}

/** Add so many sessions, that template ID will reach its maximum value */
TEST_F(MixedSessions, reachMaximumTemplateID) {
    Session* sessions = createSessions("filename");
    addSession(sessions->getSession(FDS_SESSION_UDP), 0);

    // Simulate reaching maximum capacity
    mod->curr_ctx->next_id = UINT16_MAX;
    modifyRecord(&adder_one);
    modifyRecord(&adder_two); // Should make ID overflow

    // 257 because last template with ID 256 was still added ...
    findTemplate(FDS_OK, 256, 4);
    EXPECT_EQ(mod->curr_ctx->next_id, 257);
    modifyRecord(&adder_one);
    findTemplate(FDS_OK, 257, 3);

    delete sessions;
}

/** Attempt to modify record without setting session */
TEST_F(ErrorSession, modifyWithoutAddingSession) {
    ipx_msg_garbage_t *gb_msg;
    ipx_modifier_set_adder_cb(mod, &adder_one);
    ASSERT_EQ(ipx_modifier_modify(mod, &(rec->rec), &gb_msg), nullptr);
}

/** Attempt to delete non existing session */
TEST_F(ErrorSession, deleteNonExisting) {
    Session *sessions = createSessions("deleteEmpty");
    ipx_msg_garbage_t *gb_msg;
    EXPECT_EQ(ipx_modifier_remove_session(mod, sessions->getSession(FDS_SESSION_FILE), &gb_msg), IPX_ERR_NOTFOUND);
    delete sessions;
}

/** Attempt to delete non existing session */
TEST_F(ErrorSession, TCPSetTimeInHistory) {
    Session *sessions = createSessions("test");

    ipx_session *session = sessions->getSession(FDS_SESSION_TCP);
    addSession(session, 0 , 100);
    addSession(session, 0, 10, IPX_ERR_FORMAT);
    delete sessions;
}

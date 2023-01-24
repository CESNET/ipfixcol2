/**
 * \file message_builder.cpp
 * \author Adam Zvara <xzvara01@stud.fit.vutbr.cz>
 * \brief Test for building new IPFIX messages in collector
 * \date 2023
 */

#include <gtest/gtest.h>
#include <queue>
#include <MsgGen.h>
#include <libfds.h>
#include <ipfixcol2/session.h>

extern "C" {
    #include <core/context.h>
    #include <core/parser.h>
    #include <core/message_ipfix.h>
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

const size_t data_cnt = 3;

/** \brief Class for comparing new ipfix messages records and sets with */
class Comparator {
private:
    std::queue<struct fds_ipfix_set_hdr> used_sets;
    std::queue<struct fds_drec> used_recs;
public:
    /**
     * \brief Add set to sets queue
     *
     * \param id  Flowset_id of set
     * \param len Length of set
     */
    void add_set(uint16_t id, uint16_t len) {
        fds_ipfix_set_hdr hdr {id, len};
        used_sets.push(hdr);
    }

    /**
     * \brief Add record into recs queue
     *
     * \param rec FDS record
     */
    void add_rec(fds_drec rec) {
        used_recs.push(rec);
    }

    /**
     * \brief Compare sets of given message with stored sets
     * \note Compares sets based on their IDs and length
     * \warning Order of stored sets in Comparator needs to be the same as order of sets in IPFIX message
     *
     * \param new_message Message to compare with
     */
    void compare_sets(struct ipx_msg_ipfix *new_message) {
        int rc;
        ASSERT_EQ(new_message->sets.cnt_valid, used_sets.size());
        struct fds_sets_iter set_it;
        fds_sets_iter_init(&set_it, (struct fds_ipfix_msg_hdr *) new_message->raw_pkt);
        while ((rc = fds_sets_iter_next(&set_it)) == FDS_OK) {
            auto set = used_sets.front();
            EXPECT_EQ(ntohs(set_it.set->flowset_id), set.flowset_id);
            EXPECT_EQ(ntohs(set_it.set->length), FDS_IPFIX_SET_HDR_LEN + set.length);
            used_sets.pop();
        }
        EXPECT_EQ(rc, FDS_EOC);
    }

    /**
     * \brief Compare records of given message with stored records
     * \note Compares records based on theird size and data
     * \warning Order of stored records in Comparator needs to be the same as order of records in IPFIX message
     *
     * \param new_message Message to compare with
     */
    void compare_recs(struct ipx_msg_ipfix *new_message) {
        ASSERT_EQ(new_message->rec_info.cnt_valid, used_recs.size());
        int rc;
        struct fds_dset_iter it;
        fds_dset_iter_init(&it, new_message->sets.base[0].ptr, new_message->recs[0].rec.tmplt);
        while ((rc = fds_dset_iter_next(&it)) == FDS_OK) {
            auto rec = used_recs.front();
            EXPECT_EQ(it.size, rec.size);
            EXPECT_EQ(memcmp(it.rec, rec.data, rec.size), 0);
            used_recs.pop();
        }
        EXPECT_EQ(rc, FDS_EOC);
    }
};

using session_uniq = std::unique_ptr<struct ipx_session, decltype(&ipx_session_destroy)>;
using ctx_uniq = std::unique_ptr<ipx_ctx_t, decltype(&ipx_ctx_destroy)>;
using builder_uniq = std::unique_ptr<ipx_msg_builder_t, decltype(&ipx_msg_builder_destroy)>;
using msg_uniq = std::unique_ptr<fds_ipfix_msg_hdr, decltype(&free)>;
using cmp_uniq = std::unique_ptr<Comparator>;

/**
 * \brief Main testing fixture
 */
class Builder : public ::testing::Test {
private:
    ipx_msg_ctx msg_ctx;
    ipx_session *session;
    ipx_ctx_t *ctx;
protected:
    ipx_msg_builder_t *builder;
    // Templates and data records
    fds_template *tmplts[data_cnt];
    fds_drec recs[data_cnt];
    size_t rec_length;
    // Comparator class
    Comparator *cmp;


    // Setup
    void SetUp() override {
        // Create message builder and plugin context
        std::string ctx_name = "Testing context (builder)";

        ctx_uniq     ctx_wrap(ipx_ctx_create(ctx_name.c_str(), nullptr), &ipx_ctx_destroy);
        builder_uniq builder_wrap(ipx_msg_builder_create(), &ipx_msg_builder_destroy);
        session_uniq session_wrap(ipx_session_new_file("builder_test.data"), &ipx_session_destroy);
        cmp_uniq     cmp_wrap(new Comparator());

        ctx = ctx_wrap.release();
        builder = builder_wrap.release();
        session = session_wrap.release();
        cmp = cmp_wrap.release();

        msg_ctx = {session, 1, 0};

        // Parse templates
        for (uint16_t i = 0; i < data_cnt; i++) {
            ipfix_trec trec {(uint16_t) (FDS_IPFIX_SET_MIN_DSET + i)};

            trec.add_field(  7, 2); // sourceTransportPort
            trec.add_field(  8, 4); // sourceIPv4Address
            trec.add_field( 11, 2); // destinationTransportPort
            trec.add_field( 12, 4); // destinationIPv4Address
            trec.add_field(  4, 1); // protocolIdentifier

            uint16_t tmplt_size = trec.size();
            uint8_t *tmplt_raw = trec.release();
            fds_template_parse(FDS_TYPE_TEMPLATE, tmplt_raw, &tmplt_size, &(tmplts[i]));
            free(tmplt_raw);
            assert(tmplts[i] != NULL);
        }

        // Add data records
        for (uint16_t i = 0; i < data_cnt; i++) {
            ipfix_drec drec {};

            drec.append_uint(12345, 2);
            drec.append_ip("1.1.1.1");
            drec.append_uint(54321, 2);
            drec.append_ip("2.2.2.2");
            drec.append_uint(7, 1);

            recs[i].size  = drec.size();
            recs[i].data  = drec.release();
            recs[i].tmplt = tmplts[i % data_cnt];
            recs[i].snap  = NULL; // Snapshot is not necessary
        }

        rec_length = recs[0].size;
    }

    // Tear Down
    void TearDown() override {
        // Cleanup
        delete cmp;
        for (size_t i = 0; i < data_cnt; i++) {
            free(recs[i].data);
            fds_template_destroy(tmplts[i]);
        }
        ipx_session_destroy(session);
        ipx_msg_builder_destroy(builder);
        ipx_ctx_destroy(ctx);
    }

    /**
     * \brief Start new builder message
     *
     * \param hdr       Pointer to message header
     * \param maxbytes  Maximum length of message
     * \param hints     Allocation size for raw packet
     */
    void builder_start(fds_ipfix_msg_hdr *hdr, size_t maxbytes = 1000, size_t hints = 1000) {
        ASSERT_EQ(ipx_msg_builder_start(builder, hdr, maxbytes, hints), FDS_OK);
    }

    /**
     * \brief Create new message from builder, check its length and compare records and sets
     *
     * \param msg_size Expected size of new message (without header length)
     */
    void builder_end(size_t msg_size) {
        // Create new message from original message
        struct ipx_msg_ipfix *new_message = ipx_msg_builder_end(builder, ctx, &msg_ctx);
        ASSERT_NE(new_message, nullptr);

        EXPECT_EQ(new_message->raw_size, FDS_IPFIX_MSG_HDR_LEN + msg_size);

        // Compare sets and records
        if (new_message->raw_size != FDS_IPFIX_MSG_HDR_LEN) {
            cmp->compare_sets(new_message);
            cmp->compare_recs(new_message);
        }
        ipx_msg_ipfix_destroy(new_message);
    }
};

// -----------------------------------------------------------------------------

// Simple test to create message builder
TEST_F(Builder, createBuilder)
{
    ASSERT_NE(builder, nullptr);
}

// Empty message with IPFIX header
TEST_F(Builder, emptyMessage)
{
    ipfix_msg msg {};
    msg_uniq msg_hdr(msg.release(), &free);

    // Create new message from original message
    builder_start(msg_hdr.get());

    // End message
    builder_end(0);
}

// Free raw message used by builder dedicated function
TEST_F(Builder, explicitFreeRawMessage)
{
    ipfix_msg msg {};
    msg_uniq msg_hdr(msg.release(), &free);

    // Create new message from original message
    builder_start(msg_hdr.get());

    ipx_msg_builder_free_raw(builder);
}

// Add different data records to new message within single set
TEST_F(Builder, singleSetDataRecords)
{
    ipfix_msg msg {};
    msg_uniq msg_hdr(msg.release(), &free);

    // Create builder
    builder_start(msg_hdr.get());

    // Add 3 data records from same template (which belong to same set)
    constexpr size_t N = 3;
    for (size_t i = 0; i < N; i++) {
        cmp->add_rec(recs[0]);
        ASSERT_EQ(ipx_msg_builder_add_drec(builder, &(recs[0])), FDS_OK);
    }

    // // Add single set to comparator
    cmp->add_set(recs[0].tmplt->id, 3 * rec_length);

    // End message and compare messages
    builder_end(FDS_IPFIX_SET_HDR_LEN + N * rec_length);
}

// Add data records belonging to different sets
// Overall 6 sets are created, with IDs [256,257,258,256,257,258]
TEST_F(Builder, multipleSetDataRecords)
{
    ipfix_msg msg {};
    msg_uniq msg_hdr(msg.release(), &free);

    builder_start(msg_hdr.get());

    // Add data records with different templates
    constexpr size_t drec_cnt = 6;
    for (size_t i = 0; i < drec_cnt; i++) {
        fds_drec *rec = &(recs[i % data_cnt]);
        cmp->add_rec(*rec);
        cmp->add_set(rec->tmplt->id, rec->size);
        ASSERT_EQ(ipx_msg_builder_add_drec(builder, rec), FDS_OK);
    }

    const uint set_size = drec_cnt * FDS_IPFIX_SET_HDR_LEN;
    const uint rec_size = drec_cnt * rec_length;
    builder_end(set_size + rec_size);
}

// Create builder with shorter maximum size and attempt to create message longer than maxsize
TEST_F(Builder, maxBytesExceeded)
{
    ipfix_msg msg {};
    msg_uniq msg_hdr(msg.release(), &free);

    // Can only store N recs
    constexpr size_t N = 3;
    size_t set_size = N * rec_length + FDS_IPFIX_SET_HDR_LEN;
    builder_start(msg_hdr.get(), set_size + FDS_IPFIX_MSG_HDR_LEN);

    // Add 3 data records from same template (which belong to same set)
    for (size_t i = 0; i < N; i++) {
        cmp->add_rec(recs[0]);
        ASSERT_EQ(ipx_msg_builder_add_drec(builder, &(recs[0])), FDS_OK);
    }

    // Add single set to comparator
    cmp->add_set(recs[0].tmplt->id, N * rec_length);

    // Fail to add new data record
    ASSERT_EQ(ipx_msg_builder_add_drec(builder, &(recs[0])), FDS_ERR_DENIED);

    builder_end(set_size);
}

// Reallocation of message builder raw message when new data record is added
TEST_F(Builder, lowHintsDataRecord)
{
    ipfix_msg msg {};
    msg_uniq msg_hdr(msg.release(), &free);

    constexpr size_t N = 3;
    // Can only store N - 1 recs
    size_t hints = (N - 1) * rec_length + FDS_IPFIX_SET_HDR_LEN + FDS_IPFIX_MSG_HDR_LEN;
    builder_start(msg_hdr.get(), 1000, hints);

    // Add 3 data records from same template (which belong to same set)
    // On last iteration, reallocation occurs
    for (size_t i = 0; i < N; i++) {
        cmp->add_rec(recs[0]);
        ASSERT_EQ(ipx_msg_builder_add_drec(builder, &(recs[0])), FDS_OK);
    }

    // Add single set to comparator
    cmp->add_set(recs[0].tmplt->id, N * rec_length);

    builder_end(N * rec_length + FDS_IPFIX_SET_HDR_LEN);
}

// Reallocation of message builder when new set is added
TEST_F(Builder, lowHintsSet)
{
    ipfix_msg msg {};
    msg_uniq msg_hdr(msg.release(), &free);

    // Can only store single set with single record
    size_t hints = rec_length + FDS_IPFIX_SET_HDR_LEN + FDS_IPFIX_MSG_HDR_LEN;
    builder_start(msg_hdr.get(), 1000, hints);

    // Add 2 data records from different templates (they belong to different set)
    // On last iteration, reallocation within ipx_msg_builder_add_set occurs
    constexpr size_t N = 2;
    for (size_t i = 0; i < N; i++) {
        cmp->add_rec(recs[i]);
        cmp->add_set(recs[i].tmplt->id, rec_length);
        ASSERT_EQ(ipx_msg_builder_add_drec(builder, &(recs[i])), FDS_OK);
    }

    builder_end(N * rec_length + N * FDS_IPFIX_SET_HDR_LEN);
}

/** Reallocation of record offsets */
TEST_F(Builder, reallocDataOffsets) {
    ipfix_msg msg {};
    msg_uniq msg_hdr(msg.release(), &free);

    builder_start(msg_hdr.get());
    const size_t recs_cnt = 300;
    const size_t msg_size = FDS_IPFIX_SET_HDR_LEN + recs_cnt * rec_length;
    ipx_msg_builder_set_maxlength(builder, msg_size + FDS_IPFIX_MSG_HDR_LEN);

    for (size_t i = 0; i < recs_cnt; i++) {
        cmp->add_rec(recs[0]);
        ASSERT_EQ(ipx_msg_builder_add_drec(builder, &(recs[0])), IPX_OK);
    }
    cmp->add_set(recs[0].tmplt->id, recs_cnt * rec_length);
    builder_end(msg_size);
}

/** Reallocation of set offsets */
TEST_F(Builder, reallocSetOffsets) {
    ipfix_msg msg {};
    msg_uniq msg_hdr(msg.release(), &free);

    builder_start(msg_hdr.get());
    const size_t sets_cnt = 300;
    const size_t msg_size = FDS_IPFIX_SET_HDR_LEN * sets_cnt + sets_cnt * rec_length;
    ipx_msg_builder_set_maxlength(builder, msg_size + FDS_IPFIX_MSG_HDR_LEN);

    for (size_t i = 0; i < sets_cnt; i++) {
        auto idx = i % data_cnt;
        cmp->add_rec(recs[idx]);
        cmp->add_set(recs[idx].tmplt->id, rec_length);
        ASSERT_EQ(ipx_msg_builder_add_drec(builder, &(recs[idx])), IPX_OK);
    }
    builder_end(msg_size);
}

// Reuse builder to create 1000 gradually longer messages
TEST_F(Builder, ReuseBuilder)
{
    ipfix_msg msg {};
    msg_uniq msg_hdr(msg.release(), &free);

    const size_t msg_cnt = 1000;
    int rc;

    // Start from 1 so there is no special condition for empty message ...
    for (size_t i = 1; i < msg_cnt; i++) {
        // Start message
        builder_start(msg_hdr.get());

        // Gradually add more recs
        for (size_t j = 0; j < i; j++) {
            cmp->add_rec(recs[0]);
            rc = ipx_msg_builder_add_drec(builder, &(recs[0]));
            ASSERT_NE(rc, IPX_ERR_NOMEM);
            if (rc == IPX_ERR_DENIED) {
                // Set new max length
                size_t max_length = ipx_msg_builder_get_maxlength(builder);
                ipx_msg_builder_set_maxlength(builder,  max_length + 100);
                ASSERT_EQ(ipx_msg_builder_add_drec(builder, &(recs[0])), IPX_OK);
            }
        }

        // Add set and end message
        cmp->add_set(recs[0].tmplt->id, i * rec_length);
        builder_end(FDS_IPFIX_SET_HDR_LEN + i * rec_length);
    }
}
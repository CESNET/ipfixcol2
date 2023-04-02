#include <gtest/gtest.h>
#include <MsgGen.h>
#include <libfds.h>

extern "C" {
    #include <core/template_mapper.h>
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

/** \brief Create template with given ID and fields cnt */
struct fds_template *create_template(uint id, uint fields_cnt) {
    ipfix_trec trec(id);
    for (uint i = 1; i <= fields_cnt; ++i) {
        trec.add_field(i, 4);
    }
    uint16_t tmplt_size = trec.size();
    uint8_t *tmplt_raw = trec.release();

    fds_template *tmplt;
    fds_template_parse(FDS_TYPE_TEMPLATE, tmplt_raw, &tmplt_size, &tmplt);
    free(tmplt_raw);
    return tmplt;
}

/**
 * \brief Main testing fixture for testing mapper component
 */
class Common : public ::testing::Test {
protected:
    using mapper_uniq = std::unique_ptr<ipx_template_mapper_t, decltype(&ipx_mapper_destroy)>;

    ipx_template_mapper_t *mapper;

    // Parsed templates
    const uint tmplt_cnt = 2;
    const fds_template *t1[2];
    const fds_template *t2[2];

    // Setup
    void SetUp() override {
        // Initialize mapper
        mapper_uniq mapper_wrap(ipx_mapper_create(), &ipx_mapper_destroy);
        mapper = mapper_wrap.release();

        const uint16_t tmplt_id = 256;

        for (uint i = 0; i < tmplt_cnt; ++i) {
            t1[i] = create_template(tmplt_id,   2+i);
            t2[i] = create_template(tmplt_id+1, 3+i);
        }
    }

    // Tear Down
    void TearDown() override {
        ipx_mapper_destroy(mapper);

        for (uint i = 0; i < tmplt_cnt; ++i) {
            fds_template_destroy((struct fds_template *)t1[i]);
            fds_template_destroy((struct fds_template *)t2[i]);
        }
    }
};

TEST_F(Common, CreateAndDestroy)
{
    // Nothing to do
    ASSERT_NE(mapper, nullptr);
}

TEST_F(Common, LookUp_EmptyMapper)
{
    // Look up for template
    const fds_template *tmplt = ipx_mapper_lookup(mapper, t1[0], 256);
    ASSERT_EQ(tmplt, nullptr);
}

TEST_F(Common, Add_SingleTemplate)
{
    // Add template
    ASSERT_EQ(ipx_mapper_add(mapper, t1[0], 256), IPX_OK);

    // Lookup template
    const fds_template *tmplt = ipx_mapper_lookup(mapper, t1[0], 256);
    ASSERT_NE(tmplt, nullptr);
    EXPECT_EQ(fds_template_cmp(tmplt, t1[0]), 0);
}

TEST_F(Common, Add_MultipleTemplates_SameID)
{
    // Use templates with ID 257, set original ID to 256
    ASSERT_EQ(ipx_mapper_add(mapper, t2[0], 256), IPX_OK);
    ASSERT_EQ(ipx_mapper_add(mapper, t2[1], 256), IPX_OK);

    // Lookup using wrong template
    EXPECT_EQ(ipx_mapper_lookup(mapper, t1[0], 256), nullptr);

    // Lookup using correct template
    const struct fds_template *tmplt = ipx_mapper_lookup(mapper, t2[1], 256);
    ASSERT_NE(tmplt, nullptr);
    EXPECT_EQ(fds_template_cmp(tmplt, t2[1]), 0);

    tmplt = ipx_mapper_lookup(mapper, t2[0], 256);
    ASSERT_NE(tmplt, nullptr);
    EXPECT_EQ(fds_template_cmp(tmplt, t2[0]), 0);
}

TEST_F(Common, Add_MultipleTemplates_DifferentIDs)
{
    size_t size = 1000;
    fds_template **tmplts = new fds_template*[size];
    for (size_t i = 0; i < size; i++) {
        tmplts[i] = create_template(256 + i * 10, 2);
    }

    for (size_t i = 0; i < size; i++) {
        ASSERT_EQ(ipx_mapper_add(mapper, tmplts[i], tmplts[i]->id), IPX_OK);
    }

    const struct fds_template *tmplt;
    for (size_t i = 0; i < size; i++) {
        tmplt = ipx_mapper_lookup(mapper, tmplts[i], 256 + i * 10);
        ASSERT_NE(tmplt, nullptr);
        EXPECT_EQ(fds_template_cmp(tmplt, tmplts[i]), 0);
    }

    for (size_t i = 0; i < size; i++) {
        fds_template_destroy(tmplts[i]);
    }
    delete[] tmplts;
}

TEST_F(Common, Add_AllTemplates)
{
    size_t size = 65536 - 256;
    fds_template **tmplts = new fds_template*[size];
    for (size_t i = 0; i < size; i++) {
        tmplts[i] = create_template(256 + i, 2);
    }

    for (size_t i = 0; i < size; i++) {
        ASSERT_EQ(ipx_mapper_add(mapper, tmplts[i], tmplts[i]->id), IPX_OK);
    }

    const struct fds_template *tmplt;
    for (size_t i = 0; i < size; i++) {
        tmplt = ipx_mapper_lookup(mapper, tmplts[i], 256 + i);
        ASSERT_NE(tmplt, nullptr);
        EXPECT_EQ(fds_template_cmp(tmplt, tmplts[i]), 0);
    }

    for (size_t i = 0; i < size; i++) {
        fds_template_destroy(tmplts[i]);
    }
    delete[] tmplts;
}
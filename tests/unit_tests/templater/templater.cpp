/**
 * \author Michal Režňák
 * \date   7.9.17
 */
#include <gtest/gtest.h>
#include <ipfixcol2.h>
#include <ipfixcol2/common.h>

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

class Records: public ::testing::Test
{
public:
    ipfix_template_record* valid_2elem();
    ipfix_template_record* template257_2elem();
    ipfix_template_record* valid_withdrawal();
    ipfix_template_set* valid_set_2scopes();
    ipfix_template_set* set_template256_2scopes();
protected:
    ipx_tmpl_t *tmpl_udp;
    ipx_tmpl_t *tmpl_tcp;
    void SetUp() override {
        tmpl_udp = ipx_tmpl_create(50, 50, IPX_SESSION_TYPE_UDP);
        tmpl_tcp = ipx_tmpl_create(50, 50, IPX_SESSION_TYPE_TCP);
    }

    void TearDown() override {
        ipx_tmpl_destroy(tmpl_udp);
        ipx_tmpl_destroy(tmpl_tcp);
    }
};

ipfix_template_record *
Records::valid_2elem()
{
    auto res = static_cast<ipfix_template_record*>(malloc(sizeof(struct ipfix_template_record) + 12));
        ipx_set_uint_be(&res->count,                       2, 2);
        ipx_set_uint_be(&res->template_id,                 2, 258);
        ipx_set_uint_be(&res->fields[0].ie.length,         2, 9);
        ipx_set_uint_be(&res->fields[0].ie.id,             2, 0x8003);
        ipx_set_uint_be(&res->fields[1].enterprise_number, 4, 1);
        ipx_set_uint_be(&res->fields[2].ie.length,         2, IPFIX_VAR_IE_LENGTH);
        ipx_set_uint_be(&res->fields[2].ie.id,             2, 0x8004);
        ipx_set_uint_be(&res->fields[3].enterprise_number, 4, 1);
    return res;
}

ipfix_template_record *
Records::template257_2elem()
{
    auto res = static_cast<ipfix_template_record*>(malloc(sizeof(struct ipfix_template_record) + 12));
        ipx_set_uint_be(&res->count,                       2, 2);
        ipx_set_uint_be(&res->template_id,                 2, 257);
        ipx_set_uint_be(&res->fields[0].ie.length,         2, 9);
        ipx_set_uint_be(&res->fields[0].ie.id,             2, 0x8003);
        ipx_set_uint_be(&res->fields[1].enterprise_number, 4, 1);
        ipx_set_uint_be(&res->fields[2].ie.length,         2, IPFIX_VAR_IE_LENGTH);
        ipx_set_uint_be(&res->fields[2].ie.id,             2, 0x8004);
        ipx_set_uint_be(&res->fields[3].enterprise_number, 4, 1);
    return res;
}

ipfix_template_record*
Records::valid_withdrawal()
{
    auto res = static_cast<ipfix_template_record*>(malloc(sizeof(struct ipfix_template_record)));
    ipx_set_uint_be(&res->count,       2, 0);
    ipx_set_uint_be(&res->template_id, 2, 258);
    return res;
}

ipfix_template_set *
Records::valid_set_2scopes() {
    auto set = static_cast<ipfix_template_set*>(malloc(sizeof(struct ipfix_template_set) + 2*500));
        ipx_set_uint_be(&set->header.length,     2, 44);
        ipx_set_uint_be(&set->header.flowset_id, 2 ,2);
    ipfix_template_record *rec = &set->first_record;
        ipx_set_uint_be(&rec->count,                       2, 2);
        ipx_set_uint_be(&rec->template_id,                 2, 258);
        ipx_set_uint_be(&rec->fields[0].ie.id,             2, 0x8003);
        ipx_set_uint_be(&rec->fields[0].ie.length,         2, 3);
        ipx_set_uint_be(&rec->fields[1].enterprise_number, 4, 1);
        ipx_set_uint_be(&rec->fields[2].ie.id,             2, 0x8004);
        ipx_set_uint_be(&rec->fields[2].ie.length,         2, IPFIX_VAR_IE_LENGTH);
        ipx_set_uint_be(&rec->fields[3].enterprise_number, 4, 1);

    rec += 20;
        ipx_set_uint_be(&rec->count,                       2, 2);
        ipx_set_uint_be(&rec->template_id,                 2, 259);
        ipx_set_uint_be(&rec->fields[0].ie.length,         2, 3);
        ipx_set_uint_be(&rec->fields[0].ie.id,             2, 0x8004);
        ipx_set_uint_be(&rec->fields[1].enterprise_number, 4, 1);
        ipx_set_uint_be(&rec->fields[2].ie.length,         2, IPFIX_VAR_IE_LENGTH);
        ipx_set_uint_be(&rec->fields[2].ie.id,             2, 0x8004);
        ipx_set_uint_be(&rec->fields[3].enterprise_number, 4, 1);
    return set;
}

ipfix_template_set *
Records::set_template256_2scopes() {
    auto set = static_cast<ipfix_template_set*>(malloc(sizeof(struct ipfix_template_set) + 2*500));
        ipx_set_uint_be(&set->header.length,     2, 44);
        ipx_set_uint_be(&set->header.flowset_id, 2 ,2);
    ipfix_template_record *rec = &set->first_record;
        ipx_set_uint_be(&rec->count,                       2, 2);
        ipx_set_uint_be(&rec->template_id,                 2, 256);
        ipx_set_uint_be(&rec->fields[0].ie.id,             2, 0x8003);
        ipx_set_uint_be(&rec->fields[0].ie.length,         2, 3);
        ipx_set_uint_be(&rec->fields[1].enterprise_number, 4, 1);
        ipx_set_uint_be(&rec->fields[2].ie.id,             2, 0x8004);
        ipx_set_uint_be(&rec->fields[2].ie.length,         2, IPFIX_VAR_IE_LENGTH);
        ipx_set_uint_be(&rec->fields[3].enterprise_number, 4, 1);

    rec += 20;
        ipx_set_uint_be(&rec->count,                       2, 2);
        ipx_set_uint_be(&rec->template_id,                 2, 259);
        ipx_set_uint_be(&rec->fields[0].ie.length,         2, 3);
        ipx_set_uint_be(&rec->fields[0].ie.id,             2, 0x8004);
        ipx_set_uint_be(&rec->fields[1].enterprise_number, 4, 1);
        ipx_set_uint_be(&rec->fields[2].ie.length,         2, IPFIX_VAR_IE_LENGTH);
        ipx_set_uint_be(&rec->fields[2].ie.id,             2, 0x8004);
        ipx_set_uint_be(&rec->fields[3].enterprise_number, 4, 1);
    return set;
}

TEST_F(Records, valid) {
    auto scope = Records::valid_2elem();
    ipx_tmpl_set(tmpl_tcp, 60, 60);
    EXPECT_GT(ipx_tmpl_template_parse(tmpl_udp, scope, 20), 0);
    free(scope);
}

TEST_F(Records, incorect_number_of_template) {
    auto tmp = Records::template257_2elem();
    ipx_tmpl_set(tmpl_tcp, 60, 60);
    EXPECT_EQ(ipx_tmpl_template_parse(tmpl_udp, tmp, 20), IPX_ERR);
    free(tmp);
}

TEST_F(Records, long_template) {
    auto tmp = Records::valid_2elem();
    ipx_tmpl_set(tmpl_tcp, 60, 60);
    EXPECT_EQ(ipx_tmpl_template_parse(tmpl_udp, tmp, 1), IPX_ERR);
    free(tmp);
}

TEST_F(Records, withdrawal) {
    auto tmp = Records::valid_withdrawal();
    ipx_tmpl_template_t *res = NULL;
    ipx_tmpl_set(tmpl_tcp, 60, 60);
    EXPECT_EQ(ipx_tmpl_template_parse(tmpl_udp, tmp, 1), IPX_ERR);
    EXPECT_EQ(ipx_tmpl_template_get(tmpl_udp, 300, &res), IPX_NOT_FOUND);
    EXPECT_EQ(res, nullptr);
    free(tmp);
}

TEST_F(Records, set_valid) {
    auto set = Records::valid_set_2scopes();
    ipx_tmpl_set(tmpl_tcp, 60, 60);
    EXPECT_EQ(ipx_tmpl_template_set_parse(tmpl_udp, &set->header), IPX_OK);
    EXPECT_EQ(ipx_tmpl_template_set_parse(tmpl_tcp, &set->header), IPX_OK);
    free(set);
}

TEST_F(Records, set_long_template) {
    auto tmp = Records::template257_2elem();
    ipx_tmpl_set(tmpl_tcp, 60, 60);
    EXPECT_EQ(ipx_tmpl_template_parse(tmpl_udp, tmp, 1), IPX_ERR);
    free(tmp);
}

TEST_F(Records, tmpl_get_valid) {
    auto scope = Records::valid_2elem();
    ipx_tmpl_template_t *res = NULL;

    ipx_tmpl_set(tmpl_tcp, 60, 60);
    EXPECT_GT(ipx_tmpl_template_parse(tmpl_tcp, scope, 20), 0);
    EXPECT_EQ(ipx_tmpl_template_get(tmpl_tcp, 258, &res), IPX_OK);
    EXPECT_NE(res, nullptr);

    EXPECT_GT(ipx_tmpl_template_parse(tmpl_udp, scope, 20), 0);
    EXPECT_EQ(ipx_tmpl_template_get(tmpl_udp, 258, &res), IPX_OK);
    EXPECT_NE(res, nullptr);

    res = NULL;
    EXPECT_NE(ipx_tmpl_template_get(tmpl_tcp, 259, &res), IPX_OK);
    EXPECT_EQ(res, nullptr);

    EXPECT_NE(ipx_tmpl_template_get(tmpl_udp, 259, &res), IPX_OK);
    EXPECT_EQ(res, nullptr);

    free(scope);
}

TEST_F(Records, remove_valid) {
    auto set = Records::valid_set_2scopes();
    ipx_tmpl_set(tmpl_tcp, 60, 60);

    EXPECT_EQ(ipx_tmpl_template_set_parse(tmpl_tcp, &set->header), IPX_OK);
    ipx_tmpl_template_t *res = NULL;
    EXPECT_EQ(ipx_tmpl_template_get(tmpl_tcp, 258, &res), IPX_OK);
    EXPECT_EQ(ipx_tmpl_template_get(tmpl_tcp, 259, &res), IPX_OK);

    EXPECT_EQ(ipx_tmpl_template_remove(tmpl_tcp, 258), IPX_OK);
    EXPECT_EQ(ipx_tmpl_template_get(tmpl_tcp, 258, &res), IPX_NOT_FOUND);
    EXPECT_EQ(ipx_tmpl_template_get(tmpl_tcp, 259, &res), IPX_OK);

    EXPECT_EQ(ipx_tmpl_template_remove(tmpl_tcp, 259), IPX_OK);
    EXPECT_EQ(ipx_tmpl_template_get(tmpl_tcp, 259, &res), IPX_NOT_FOUND);
    free(set);
}

TEST_F(Records, clear_valid) {
    auto set = Records::valid_set_2scopes();
    ipx_tmpl_set(tmpl_tcp, 60, 60);

    EXPECT_EQ(ipx_tmpl_template_set_parse(tmpl_tcp, &set->header), IPX_OK);
    ipx_tmpl_template_t *res = NULL;
    EXPECT_EQ(ipx_tmpl_template_get(tmpl_tcp, 258, &res), IPX_OK);
    EXPECT_NE(res, nullptr);

    EXPECT_EQ(ipx_tmpl_clear(tmpl_tcp), IPX_OK);
    res = NULL;
    EXPECT_NE(ipx_tmpl_template_get(tmpl_tcp, 258, &res), IPX_OK);
    EXPECT_EQ(res, nullptr);

    free(set);
}

TEST_F(Records, snapshot_valid) {
    auto scope = Records::valid_2elem();
    auto withdrawal = Records::valid_withdrawal();
    ipx_tmpl_template_t *res = NULL;

    ipx_tmpl_set(tmpl_tcp, 60, 60);
    EXPECT_GT(ipx_tmpl_template_parse(tmpl_tcp, scope, 20), 0);
    EXPECT_EQ(ipx_tmpl_template_get(tmpl_tcp, 258, &res), IPX_OK);
    const ipx_tmpl_t *snap = ipx_tmpl_snapshot_get(tmpl_tcp);
    EXPECT_NE(snap, nullptr);

    ipx_tmpl_set(tmpl_tcp, 90, 90);
    EXPECT_GT(ipx_tmpl_template_parse(tmpl_tcp, withdrawal, 4), 0);
    EXPECT_EQ(ipx_tmpl_template_get(tmpl_tcp, 258, &res), IPX_NOT_FOUND);
    snap = ipx_tmpl_snapshot_get(tmpl_tcp);
    EXPECT_NE(snap, nullptr);

    res = NULL;
    EXPECT_EQ(ipx_tmpl_template_get(snap, 258, &res), IPX_OK);
    EXPECT_NE(res, nullptr);

    free(scope);
    free(withdrawal);
}

TEST_F(Records, garbage_valid) {
    auto scope         = Records::valid_2elem();
    auto withdrawal    = Records::valid_withdrawal();
    auto scope259      = Records::valid_2elem();
    auto withdrawal259 = Records::valid_withdrawal();
    ipx_set_uint_be(&scope259->template_id, 2, 259);
    ipx_set_uint_be(&withdrawal259->template_id, 2, 259);
    ipx_tmpl_template_t *res = NULL;

    // TCP
    ipx_tmpl_set(tmpl_tcp, 10, 10);
    EXPECT_GT(ipx_tmpl_template_parse(tmpl_tcp, scope, 20), 0);
    EXPECT_EQ(ipx_tmpl_template_get(tmpl_tcp, 258, &res), IPX_OK);

    ipx_tmpl_set(tmpl_tcp, 100, 100);
    EXPECT_EQ(ipx_tmpl_garbage_get(tmpl_tcp), nullptr);

    // UDP
    ipx_tmpl_set(tmpl_udp, 10, 10);
    EXPECT_GT(ipx_tmpl_template_parse(tmpl_udp, scope, 20), 0);
    EXPECT_EQ(ipx_tmpl_template_get(tmpl_udp, 258, &res), IPX_OK);
    EXPECT_GT(ipx_tmpl_template_parse(tmpl_udp, withdrawal, 4), 0);
    EXPECT_EQ(ipx_tmpl_template_get(tmpl_udp, 258, &res), IPX_NOT_FOUND);

    EXPECT_GT(ipx_tmpl_template_parse(tmpl_udp, scope259, 20), 0);
    EXPECT_EQ(ipx_tmpl_template_get(tmpl_udp, 259, &res), IPX_OK);
    EXPECT_GT(ipx_tmpl_template_parse(tmpl_udp, withdrawal259, 4), 0);
    EXPECT_EQ(ipx_tmpl_template_get(tmpl_udp, 259, &res), IPX_NOT_FOUND);

    ipx_tmpl_set(tmpl_udp, 90, 90);
    EXPECT_GT(ipx_tmpl_template_parse(tmpl_udp, scope, 20), 0);
    EXPECT_EQ(ipx_tmpl_template_get(tmpl_udp, 258, &res), IPX_OK);

    ipx_tmpl_set(tmpl_udp, 100, 100);
    EXPECT_EQ(ipx_tmpl_garbage_get(tmpl_udp), nullptr);

    free(scope);
}

TEST_F(Records, template_valid) {
    auto scope = Records::valid_2elem();
    ipx_tmpl_template_t *res = NULL;
    ipx_tmpl_set(tmpl_tcp, 10, 10);
    EXPECT_GT(ipx_tmpl_template_parse(tmpl_tcp, scope, 20), 0);
    EXPECT_EQ(ipx_tmpl_template_get(tmpl_tcp, 258, &res), IPX_OK);

    EXPECT_EQ(ipx_tmpl_template_type_get(res), IPX_TEMPLATE);
    EXPECT_EQ(ipx_tmpl_template_opts_type_get(res), IPX_OPTS_NO_OPTIONS);
    EXPECT_EQ(ipx_tmpl_template_id_get(res), 258);

    const ipx_tmpl_template_field *field = ipx_tmpl_template_field_get(res, 42);
    EXPECT_EQ(field, nullptr);

    field = ipx_tmpl_template_field_get(res, 0);
    EXPECT_EQ(field->en, 1);
    EXPECT_EQ(field->id, 3);
    EXPECT_EQ(field->length, 9);
    EXPECT_EQ(field->offset, 0);
    EXPECT_TRUE(field->last_identical);
    EXPECT_EQ(field->definition, nullptr);

    free(scope);
}

#include <gtest/gtest.h>
#include <MsgGen.h>
#include <libfds.h>
#include <memory>

extern "C" {
    #include <core/message_ipfix.h>
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

int
ipx_modifier_filter(struct fds_drec *rec, uint8_t *filter)
{
    if (rec == NULL || filter == NULL) {
        return IPX_ERR_ARG;
    }

    // Modify template
    struct fds_template *tmplt = ipfix_template_remove_fields(rec->tmplt, filter);
    if (tmplt == NULL) {
        return IPX_ERR_NOMEM;
    }

    // Modify record
    ipfix_msg_remove_drecs(rec, filter);
    rec->tmplt = tmplt;
    rec->snap = NULL;

    return IPX_OK;
}

int
ipx_modifier_append(struct fds_drec *rec,
    const struct ipx_modifier_field *fields,
    const struct ipx_modifier_output *buffers,
    const size_t fields_cnt)
{
    if (rec == NULL || fields == NULL || buffers == NULL) {
        return IPX_ERR_ARG;
    }

    // Modify template
    struct fds_template *tmplt = ipfix_template_add_fields(rec->tmplt, fields, buffers, fields_cnt);
    if (tmplt == NULL) {
        return IPX_ERR_NOMEM;
    }

    // Modify record
    if (ipfix_msg_add_drecs(rec, fields, buffers, fields_cnt) != IPX_OK) {
        fds_template_destroy(tmplt);
        return IPX_ERR_NOMEM;
    }
    rec->tmplt = tmplt;
    rec->snap = NULL;

    return IPX_OK;
}

/**
 * \brief Main testing fixture for testing modifier component
 */
class Modifier : public ::testing::Test {
protected:
    fds_template *tmplt;
    fds_drec rec;

    // Setup
    void SetUp() override {
        // Parse template
        const uint16_t tmplt_id = 256;
        ipfix_trec trec(tmplt_id);
        trec.add_field(81, trec.SIZE_VAR);  // interfaceName -> variable length
        trec.add_field(8, 4);               // SRC IPv4 address
        trec.add_field(12, 4);              // DSC IPv4 address
        trec.add_field(1, 4);               // bytes
        trec.add_field(2, 4);               // packets
        trec.add_field(210, trec.SIZE_VAR); // paddingOctets -> variable length
        trec.add_field(7,  2, 1);           // SRC port (with EN)
        trec.add_field(11, 2, 4);           // DST port (with EN)

        uint16_t tmplt_size = trec.size();
        uint8_t *tmplt_raw = trec.release();
        fds_template_parse(FDS_TYPE_TEMPLATE, tmplt_raw, &tmplt_size, &tmplt);
        free(tmplt_raw);
        assert(tmplt != NULL);

        // Create data record
        const size_t padding_size = 1000;
        uint8_t padding[padding_size] {};
        ipfix_drec drec;
        drec.append_string("enp0s3");               // variable length with single length octet
        drec.append_ip("10.10.10.10");
        drec.append_ip("20.20.20.20");
        drec.append_uint(1000, 4);
        drec.append_uint(20, 4);
        drec.append_octets(padding, padding_size);  // variable length with three length octets
        drec.append_uint(20, 2);
        drec.append_uint(25, 2);

        rec.size = drec.size();
        rec.data = drec.release();
        rec.tmplt = tmplt;
        rec.snap = NULL;
    }

    // Tear Down
    void TearDown() override {
        free(rec.data);
        fds_template_destroy(tmplt);
    }

    /**
     * \brief Compare template field with given data
     *
     * \param[in] field Field to compare with
     * \param[in] en Enterprise number
     * \param[in] id Field ID
     * \param[in] len Field length
     * \param[in] off Field offset
     */
    void check_field(fds_tfield field, uint8_t en, uint16_t id,
        uint16_t len = FDS_IPFIX_VAR_IE_LEN, uint16_t off = FDS_IPFIX_VAR_IE_LEN) {
        EXPECT_EQ(field.en, en);
        EXPECT_EQ(field.id, id);
        EXPECT_EQ(field.length, len);
        EXPECT_EQ(field.offset, off);
    }
};

/** ------------------------- FILTER TESTS ------------------------- **/

/**
 * \brief Class for testing filtering of template fields as well as
 *  data records
 */
class Filter : public Modifier {
protected:
    uint8_t *filter;

    // Setup
    void SetUp() override {
        Modifier::SetUp();

        // Initialize filter
        filter = new uint8_t[tmplt->fields_cnt_total];
        memset(filter, 0, tmplt->fields_cnt_total);
    }

    // Tear Down
    void TearDown() override {
        Modifier::TearDown();
        delete[] filter;
    }

};

// Filtering out records with static length
void static_filter(uint8_t *filter) {
    filter[1] = 1; // SRC IP
    filter[4] = 1; // packets
    filter[7] = 1; // DST port
}

// Filter out records with dynamic length
void dynamic_filter(uint8_t *filter) {
    filter[0] = 1; // interfaceName
    filter[5] = 1; // padding
}

// Filter out records at the beginning of template definition
void start_filter(uint8_t *filter) {
    filter[0] = 1; // interfaceName
    filter[1] = 1; // SRC IP
    filter[2] = 1; // DST IP
    filter[3] = 1; // bytes
}

// Filter out random records from template definition
void mixed_filter(uint8_t *filter) {
    filter[0] = 1; // interfaceName
    filter[1] = 1; // SRC IP
    filter[5] = 1; // padding
    filter[7] = 1; // DST port
}

// Filter out records at the end of template definition
void end_filter(uint8_t *filter) {
    filter[6] = 1; // SRC port
    filter[7] = 1; // DST port
}

// Filter out all fields from template/drec
void all_filter(uint8_t *filter) {
    memset(filter, 1, 8);
}

// Filter no fields from template
TEST_F(Filter, TemplateFields_None) {
    ASSERT_EQ(ipx_modifier_filter(&rec, filter), IPX_OK);
    fds_template *t = (fds_template *)(rec.tmplt);

    // Check modified template
    EXPECT_EQ(t->type, FDS_TYPE_TEMPLATE);
    EXPECT_EQ(t->raw.length, tmplt->raw.length);
    ASSERT_EQ(t->fields_cnt_total, tmplt->fields_cnt_total);

    // Compare fields
    for (uint16_t i = 0; i < t->fields_cnt_total; i++) {
        EXPECT_EQ(memcmp(&(t->fields[i]), &(tmplt->fields[i]), sizeof(fds_tfield)), 0);
    }

    fds_template_destroy(t);
}

// Filter out starting fields from template
TEST_F(Filter, TemplateFields_Start) {
    // Set filter
    start_filter(filter);

    ASSERT_EQ(ipx_modifier_filter(&rec, filter), IPX_OK);
    fds_template *t = (fds_template *)(rec.tmplt);

    // Check modified template
    EXPECT_EQ(t->type, FDS_TYPE_TEMPLATE);
    EXPECT_EQ(t->raw.length, 28u); // 4 (hdr) + 8 (2x en=0 fields) + 16 (2x en=1 fields)
    ASSERT_EQ(t->fields_cnt_total, 4u);

    // Check fields
    check_field(t->fields[0], 0u, 2u, 4u, 0u);      // packets
    check_field(t->fields[1], 0u, 210u, 65535u, 4); // padding
    check_field(t->fields[2], 1u, 7u, 2u);          // SRC port
    check_field(t->fields[3], 4u, 11u, 2u);         // DST port

    fds_template_destroy(t);
}

// Filter out last 2 fields from template
TEST_F(Filter, TemplateFields_End) {
    // Set filter
    end_filter(filter);

    ASSERT_EQ(ipx_modifier_filter(&rec, filter), IPX_OK);
    fds_template *t = (fds_template *)(rec.tmplt);

    // Check modified template
    EXPECT_EQ(t->type, FDS_TYPE_TEMPLATE);
    EXPECT_EQ(t->raw.length, 28u); // 4 (hdr) + 24 (6x en=0 fields)
    ASSERT_EQ(t->fields_cnt_total, 6u);

    // Check fields
    check_field(t->fields[0], 0u, 81u, FDS_IPFIX_VAR_IE_LEN, 0u);  // interfaceName
    check_field(t->fields[1], 0u, 8u, 4u);      // SRC IP
    check_field(t->fields[2], 0u, 12u, 4u);     // DST IP
    check_field(t->fields[3], 0u, 1u, 4u);      // bytes
    check_field(t->fields[4], 0u, 2u, 4u);      // packets
    check_field(t->fields[5], 0u, 210u);        // padding

    fds_template_destroy(t);
}

// Filter out random fields from template (start, middle, end fields)
TEST_F(Filter, TemplateFields_MixedPosition) {
    // Set filter
    mixed_filter(filter);

    ASSERT_EQ(ipx_modifier_filter(&rec, filter), IPX_OK);
    fds_template *t = (fds_template *)(rec.tmplt);

    // Check modified template
    EXPECT_EQ(t->type, FDS_TYPE_TEMPLATE);
    EXPECT_EQ(t->raw.length, 24u); // 4 (hdr) + 12 (3x en=0 fields) + 8 (1x en=1 field)
    ASSERT_EQ(t->fields_cnt_total, 4u);

    // Check fields
    check_field(t->fields[0], 0u, 12u, 4u, 0u);  // DST IP
    check_field(t->fields[1], 0u, 1u, 4u, 4u);   // bytes
    check_field(t->fields[2], 0u, 2u, 4u, 8u);   // packets
    check_field(t->fields[3], 1u, 7u, 2u, 12u);  // SRC port

    fds_template_destroy(t);
}

// Filter out all fields from template
TEST_F(Filter, TemplateFields_All) {
    // Set filter
    memset(filter, 1, tmplt->fields_cnt_total);

    ASSERT_EQ(ipx_modifier_filter(&rec, filter), IPX_OK);
    fds_template *t = (fds_template *)(rec.tmplt);

    // Check modified template
    EXPECT_EQ(t->type, FDS_TYPE_TEMPLATE);
    EXPECT_EQ(t->raw.length, 4u); // hdr only
    EXPECT_EQ(t->fields_cnt_total, 0u);

    fds_template_destroy(t);
}

// Filtering zero fields from original data record
TEST_F(Filter, DataRecords_None) {
    uint16_t rec_prev_size = rec.size;
    uint8_t *orig_raw = new uint8_t[rec.size];
    memcpy(orig_raw, rec.data, rec.size);

    // Filter out records based on filter function
    ASSERT_EQ(ipx_modifier_filter(&rec, filter), IPX_OK);

    // Check properties of returned record
    EXPECT_EQ(rec.size, rec_prev_size);
    EXPECT_EQ(memcmp(rec.data, orig_raw, rec.size), 0);

    fds_template_destroy((fds_template *)rec.tmplt);
    delete[] orig_raw;
}

// Filtering out static (non variable length) fields from original data record
// Also tests filtering last data record
TEST_F(Filter, DataRecords_StaticLength) {
    uint16_t rec_prev_size = rec.size;
    static_filter(filter);

    // Filter out records based on filter function
    ASSERT_EQ(ipx_modifier_filter(&rec, filter), IPX_OK);

    // Check properties of returned record
    EXPECT_EQ(rec.size, rec_prev_size - 10u);

    fds_drec_iter it;
    fds_drec_iter_init(&it, &rec, FDS_DREC_PADDING_SHOW);
    int rc;
    EXPECT_EQ((rc = fds_drec_iter_next(&it)), 0);

    // interfaceName
    EXPECT_EQ(it.field.size, 6);
    EXPECT_EQ(memcmp(it.field.data, "enp0s3", it.field.size), 0);

    // SRC IP filtered

    // DST IP
    EXPECT_EQ((rc = fds_drec_iter_next(&it)), 1);
    EXPECT_EQ(it.field.size, 4);
    uint8_t buffer[4] {};
    inet_pton(AF_INET, "20.20.20.20", buffer);
    EXPECT_EQ(memcmp(it.field.data, buffer, it.field.size), 0);

    // bytes
    EXPECT_EQ((rc = fds_drec_iter_next(&it)), 2);
    EXPECT_EQ(it.field.size, 4);
    EXPECT_EQ(ntohl(*(uint32_t *)it.field.data), 1000u);

    // packets filtered

    // padding
    EXPECT_EQ((rc = fds_drec_iter_next(&it)), 3);
    EXPECT_EQ(it.field.size, 1000);
    uint8_t empty[1000] {};
    EXPECT_EQ(memcmp(it.field.data, empty, it.field.size), 0);

    // SRC port
    EXPECT_EQ((rc = fds_drec_iter_next(&it)), 4);
    EXPECT_EQ(it.field.size, 2);
    EXPECT_EQ(ntohs(*(uint16_t *)it.field.data), 20);

    // DST port filtered

    // Iterator is at the end of record
    rc = fds_drec_iter_next(&it);
    EXPECT_EQ(rc, FDS_EOC);

    fds_template_destroy((fds_template *)rec.tmplt);
}

// Filtering out dynamic (variable length) fields from original data record
// Also tests filtering first data record
TEST_F(Filter, DataRecords_VariableLength) {
    uint16_t rec_prev_size = rec.size;
    dynamic_filter(filter);

    // Filter out records based on filter function
    ASSERT_EQ(ipx_modifier_filter(&rec, filter), IPX_OK);

    // Check properties of returned record
    EXPECT_EQ(rec.size, rec_prev_size - 1010u);

    fds_drec_iter it;
    fds_drec_iter_init(&it, &rec, FDS_DREC_PADDING_SHOW);
    int rc;

    // interfaceName filtered

    // SRC IP
    EXPECT_EQ((rc = fds_drec_iter_next(&it)), 0);
    EXPECT_EQ(it.field.size, 4);
    uint8_t buffer[4] {};
    inet_pton(AF_INET, "10.10.10.10", buffer);
    EXPECT_EQ(memcmp(it.field.data, buffer, it.field.size), 0);

    // DST IP
    EXPECT_EQ((rc = fds_drec_iter_next(&it)), 1);
    EXPECT_EQ(it.field.size, 4);
    memset(buffer, 0, 4);
    inet_pton(AF_INET, "20.20.20.20", buffer);
    EXPECT_EQ(memcmp(it.field.data, buffer, it.field.size), 0);

    // bytes
    EXPECT_EQ((rc = fds_drec_iter_next(&it)), 2);
    EXPECT_EQ(it.field.size, 4);
    EXPECT_EQ(ntohl(*(uint32_t *)it.field.data), 1000u);

    // packets filtered
    EXPECT_EQ((rc = fds_drec_iter_next(&it)), 3);
    EXPECT_EQ(it.field.size, 4);
    EXPECT_EQ(ntohl(*(uint32_t *)it.field.data), 20u);

    // padding filtered

    // SRC port filtered
    EXPECT_EQ((rc = fds_drec_iter_next(&it)), 4);
    EXPECT_EQ(it.field.size, 2);
    EXPECT_EQ(ntohs(*(uint16_t *)it.field.data), 20);

    // DST port
    EXPECT_EQ((rc = fds_drec_iter_next(&it)), 5);
    EXPECT_EQ(it.field.size, 2);
    EXPECT_EQ(ntohs(*(uint16_t *)it.field.data), 25);

    // Iterator is at the end of record
    rc = fds_drec_iter_next(&it);
    EXPECT_EQ(rc, FDS_EOC);

    fds_template_destroy((fds_template *)rec.tmplt);
}

/** ------------------------- ADDING TESTS ------------------------- **/

#define STATIC_CNT 4
#define DYNAMIC_CNT 2

class Adder : public Modifier {
protected:
    // Struct used for defining new fields and their values
    ipx_modifier_field static_fields[STATIC_CNT];
    ipx_modifier_field dynamic_fields[DYNAMIC_CNT];
    ipx_modifier_output static_output[STATIC_CNT];
    ipx_modifier_output dynamic_output[DYNAMIC_CNT];

    void *orig_drec_data;
    uint16_t prev_size;

    // Structs used for comparing data in record
    fds_drec_field field;
    fds_drec_iter it;

    // Setup
    void SetUp() override {
        Modifier::SetUp();
        // Initalize static fields and output buffer
        static_init();
        dynamic_init();
        // Store original data pointer to free memory
        orig_drec_data = rec.data;
        prev_size = rec.size;
    }

    // Tear Down
    void TearDown() override {
        Modifier::TearDown();
        if (orig_drec_data != rec.data) {
            free(orig_drec_data);
            fds_template_destroy((fds_template *)rec.tmplt);
        }
    }

    /**
     * \brief Initialize fields used for testing with non-variable fields
     *
     * id = 10, length = 2, en = 1
     * id = 20, length = 4, en = 0
     * id = 30, length = 6, en = 1
     * id = 40, length = 8, en = 0
     *
     */
    void static_init() {
        for (auto i = 1; i <= STATIC_CNT; i++) {
            static_fields[i-1].en = i % 2;
            static_fields[i-1].id = i * 10;
            static_fields[i-1].length = i * 2;
            static_output[i-1].length = IPX_MODIFIER_SKIP;
        }
    }

    /**
     * \brief Initialize fields used for testing with variable fields
     *
     * id = 1000, length = VARIABLE, en = 0
     * id = 2000, length = VARIABLE, en = 0
     *
     */
    void dynamic_init() {
        for (auto i = 1; i <= DYNAMIC_CNT; i++) {
            dynamic_fields[i-1].en = 0;
            dynamic_fields[i-1].id = i * 1000;
            dynamic_fields[i-1].length = FDS_IPFIX_VAR_IE_LEN;
            dynamic_output[i-1].length = -1;
        }
    }

    /**
     * \brief Set value in output buffer
     *
     * \param field Field specifier for given field
     * \param out Output buffer
     * \param value Value to set to output buffer
     * \param size Size of value to copy
     */
    void set_value(ipx_modifier_field &field, ipx_modifier_output &out, void *value, size_t size) {
        if (field.length == FDS_IPFIX_VAR_IE_LEN) {
            out.length = size;
        } else {
            out.length = field.length;
        }
        memcpy(out.raw, value, size);
    }

    /**
     * \brief Compare record template with given values
     *
     * \param data_len Sum of appended fields lengths
     * \param fields_cnt Number of appended fields
     * \param raw_length Actual raw length of new fields
     */
    void cmp_template_overall(const uint16_t data_len, const uint8_t fields_cnt, const uint16_t raw_length) {
        ASSERT_EQ(rec.tmplt->type, FDS_TYPE_TEMPLATE);
        EXPECT_EQ(rec.tmplt->data_length, tmplt->data_length + data_len);
        EXPECT_EQ(rec.tmplt->fields_cnt_total, tmplt->fields_cnt_total + fields_cnt);
        EXPECT_EQ(rec.tmplt->raw.length, tmplt->raw.length + raw_length);
        // Compare raw templates (except headers and new fields)
        // +-4 for skipping header since number of fields is different
        EXPECT_EQ(memcmp(rec.tmplt->raw.data+4, tmplt->raw.data+4, tmplt->raw.length-4), 0);
    }

    /**
     * \brief Compare record template field at given position with used field
     *
     * \param pos Position of field in parsed template
     * \param used Used modifier field
     */
    void cmp_template_field(const uint16_t pos, const ipx_modifier_field used) {
        EXPECT_EQ(rec.tmplt->fields[pos].id, used.id);
        EXPECT_EQ(rec.tmplt->fields[pos].en, used.en);
        EXPECT_EQ(rec.tmplt->fields[pos].length, used.length);
    }

    /**
     * \brief Check original data part of record before appending new data
     *
     * \param added_size Size of appended data
     */
    void cmp_data_overall(const uint16_t added_size) {
        EXPECT_EQ(rec.size, prev_size + added_size);
        EXPECT_EQ(memcmp(rec.data, orig_drec_data, prev_size), 0);
    }

    /**
     * \brief Check single appended data record
     *
     * \param field Field from data record to compare values with
     * \param data Data expected in record
     * \param size Size of data in record
     */
    void cmp_data_record(const fds_drec_field field, void *data, uint16_t size) {
        if (size < 255) {
            EXPECT_EQ(field.size, size);
        } else {
            EXPECT_EQ(field.size, htons(size));
        }
        EXPECT_EQ(memcmp(field.data, data, size), 0);
    }

};

// Append zero fields to record
TEST_F(Adder, Static_ZeroFields) {
    // Modify record
    ASSERT_EQ(ipx_modifier_append(&rec, static_fields, static_output, STATIC_CNT), IPX_OK);

    // Check template and record
    cmp_template_overall(0, 0, 0);
    cmp_data_overall(0);
}

// Append zero fields to record but keep them in template and records as 0
TEST_F(Adder, Static_ZeroFields_KeepEmptyOutputs) {
    for (struct ipx_modifier_output &i : static_output) {
        i.length = -1;
    }
    // Modify record
    ASSERT_EQ(ipx_modifier_append(&rec, static_fields, static_output, STATIC_CNT), IPX_OK);

    // Check template
    cmp_template_overall(20, 4, 24);

    // Check record
    cmp_data_overall(20);

    uint64_t zero = 0;
    int p = 8;

    for (auto i : static_fields) {
        ASSERT_EQ(fds_drec_find(&rec, i.en, i.id, &field), p++);
        cmp_data_record(field, &zero, i.length);
    }

}

// Append single value to data record and template
TEST_F(Adder, Static_SingleField) {
    // Add single value to output buffer
    uint8_t pos = 1;
    uint32_t value = 422322;
    ipx_modifier_field used = static_fields[pos]; // {.id = 20, .length = 4, .en = 0}
    set_value(used, static_output[pos], &value, 4);

    // Modify record
    ASSERT_EQ(ipx_modifier_append(&rec, static_fields, static_output, STATIC_CNT), IPX_OK);

    // Check modified template
    cmp_template_overall(used.length, 1, 4);
    cmp_template_field(rec.tmplt->fields_cnt_total - 1, used);

    // Check modified record
    cmp_data_overall(4);
    fds_drec_find(&rec, used.en, used.id, &field);
    cmp_data_record(field, &value, 4);
}

// Append single value (EN specific) to data record and template
TEST_F(Adder, Static_SingleField_WithEN) {
    // Add single value (with EN) to output buffer
    uint8_t pos = 0;
    uint16_t value = 15213;
    ipx_modifier_field used = static_fields[pos]; // {.id = 10, .length = 2, .en = 1}
    set_value(used, static_output[pos], &value, used.length);

    // Modify template
    ASSERT_EQ(ipx_modifier_append(&rec, static_fields, static_output, STATIC_CNT), IPX_OK);

    // Check modified template
    cmp_template_overall(used.length, 1, 8);
    cmp_template_field(rec.tmplt->fields_cnt_total - 1, used);

    // Check modified record
    cmp_data_overall(2);
    fds_drec_find(&rec, used.en, used.id, &field);
    cmp_data_record(field, &value, 2);
}

// Append multiple values (EN and non-EN specific) to data record and template
TEST_F(Adder, Static_MultipleFields) {
    // Add all values to output buffer
    int used_length = 0;
    long value = 0x123456789ABCDEF;
    for (uint8_t pos = 0; pos < STATIC_CNT; pos++) {
        ipx_modifier_field used = static_fields[pos];
        set_value(used, static_output[pos], &value, used.length);
        used_length += used.length;
    }

    // Modify template
    ASSERT_EQ(ipx_modifier_append(&rec, static_fields, static_output, STATIC_CNT), IPX_OK);

    // Check modified template
    cmp_template_overall(used_length, 4, 24); // 8 (2x4=non-EN) + 16 (2x8=EN)
    for (int i = 0; i < STATIC_CNT; i++) {
        cmp_template_field(rec.tmplt->fields_cnt_total - (STATIC_CNT - i), static_fields[i]);
    }

    // Check modified record
    cmp_data_overall(20);
    fds_drec_iter_init(&it, &rec, 0);
    // Set iterator to first appended field
    ASSERT_EQ(fds_drec_iter_find(&it, static_fields[0].en, static_fields[0].id), 8);

    // Iterate through all remaining data records and compare them
    int size = 2;
    do {
        cmp_data_record(it.field, &value, size);
        size += 2;
    } while (fds_drec_iter_next(&it) != FDS_EOC);
}

// Append multiple values, some of which are kept in template and records with no values
TEST_F(Adder, Static_MultipleFields_KeepEmptyOutputs) {
    long value = 0x123456789ABCDEF;

    // Keep first field (length = 2)
    static_output[0].length = -1;
    // Use second and third field (lengths = {4, 6})
    set_value(static_fields[1], static_output[1], &value, 4);
    set_value(static_fields[2], static_output[2], &value, 6);
    // Do not use fourth field
    static_output[3].length = IPX_MODIFIER_SKIP;

    // Modify template
    ASSERT_EQ(ipx_modifier_append(&rec, static_fields, static_output, STATIC_CNT), IPX_OK);

    // Check modified template
    cmp_template_overall(12, 3, 20); // 4 (non-EN) + 16 (2x8=EN)
    for (int i = 0; i < 3; i++) {
        cmp_template_field(rec.tmplt->fields_cnt_total - (3 - i), static_fields[i]);
    }

    // Check modified record
    cmp_data_overall(12);
    fds_drec_iter_init(&it, &rec, 0);
    // Set iterator to first appended field
    ASSERT_EQ(fds_drec_iter_find(&it, static_fields[0].en, static_fields[0].id), 8);
    EXPECT_EQ(*(uint16_t *)it.field.data, 0);
    ASSERT_EQ(fds_drec_iter_next(&it), 9);

    // Iterate through all remaining data records and compare them
    int size = 4;
    do {
        cmp_data_record(it.field, &value, size);
        size += 2;
    } while (fds_drec_iter_next(&it) != FDS_EOC);
}

// Append zero fields to record but keep them in template and records as 0
TEST_F(Adder, Dynamic_ZeroFields) {
    dynamic_output[0].length = IPX_MODIFIER_SKIP;
    dynamic_output[1].length = IPX_MODIFIER_SKIP;
    // Modify record
    ASSERT_EQ(ipx_modifier_append(&rec, dynamic_fields, dynamic_output, DYNAMIC_CNT), IPX_OK);

    // Check template
    cmp_template_overall(0, 0, 0);

    // Check record
    cmp_data_overall(0);
}

// Append zero fields to record but keep them in template and records as 0
TEST_F(Adder, Dynamic_ZeroFields_KeepEmptyOutputs) {
    // Modify record
    ASSERT_EQ(ipx_modifier_append(&rec, dynamic_fields, dynamic_output, DYNAMIC_CNT), IPX_OK);

    // Check template
    cmp_template_overall(2, 2, 8);

    // Check record
    cmp_data_overall(2);

    uint64_t zero = 0;
    int p = 8;

    for (auto i : dynamic_fields) {
        ASSERT_EQ(fds_drec_find(&rec, i.en, i.id, &field), p++);
        cmp_data_record(field, &zero, 0);
    }
}

// Append values with variable length (one with single prefix octet, second with 3 prefix octets)
TEST_F(Adder, Dynamic_MultipleFields) {
    // Add all values to output buffer
    uint8_t values[1000] = {0x12, 0x34, 0x56, };
    values[678] = 0x78;
    values[999] = 0xAA;
    set_value(dynamic_fields[0], dynamic_output[0], &values, 3);
    set_value(dynamic_fields[1], dynamic_output[1], &values, 1000);

    // Modify template
    ASSERT_EQ(ipx_modifier_append(&rec, dynamic_fields, dynamic_output, DYNAMIC_CNT), IPX_OK);

    // Check modified template
    cmp_template_overall(2, 2, 8); // 8 (2x4=non-EN)
    cmp_template_field(rec.tmplt->fields_cnt_total - 2, dynamic_fields[0]);
    cmp_template_field(rec.tmplt->fields_cnt_total - 1, dynamic_fields[1]);

    // Check modified record
    cmp_data_overall(1007); // 4 (1B prefix + 3B) + 10003 (3B prefix + 1000B)

    fds_drec_iter_init(&it, &rec, 0);

    // First field
    ASSERT_EQ(fds_drec_iter_find(&it, dynamic_fields[0].en, dynamic_fields[0].id), 8);
    cmp_data_record(it.field, &values, 3);

    // Second field
    ASSERT_EQ(fds_drec_iter_find(&it, dynamic_fields[1].en, dynamic_fields[1].id), 9);
    cmp_data_record(it.field, &values, 1000);

    EXPECT_EQ(fds_drec_iter_next(&it), FDS_EOC);
}

// Append value with variable length (single prefix octet) and
TEST_F(Adder, Dynamic_MultipleFields_KeepEmptyOutputs) {
    // Add all values to output buffer
    uint8_t values[] = {0x12, 0x34, 0x56};
    set_value(dynamic_fields[0], dynamic_output[0], &values, 3);
    dynamic_output[1].length = IPX_MODIFIER_SKIP;

    // Modify template
    ASSERT_EQ(ipx_modifier_append(&rec, dynamic_fields, dynamic_output, DYNAMIC_CNT), IPX_OK);

    // Check modified template
    cmp_template_overall(1, 1, 4);
    cmp_template_field(rec.tmplt->fields_cnt_total - 1, dynamic_fields[0]);

    // Check modified record
    cmp_data_overall(4); // 4 (1B prefix + 3B)
    fds_drec_find(&rec, dynamic_fields[0].en, dynamic_fields[0].id, &field);
    cmp_data_record(field, &values, 3);
}
#include <gtest/gtest.h>
#include <MsgGen.h>

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

TEST(basic, simple_msg)
{
    // IPFIX Message
    ipfix_msg msg{};

    // Add a template
    ipfix_trec tmplt1{256};
    tmplt1.add_field(1, 4); // bytes
    tmplt1.add_field(2, 4); // packets

    ipfix_set tmplt_set {FDS_IPFIX_SET_TMPLT};
    tmplt_set.add_rec(tmplt1);
    msg.add_set(tmplt_set);

    // Add a record
    ipfix_drec rec1{};
    rec1.append_uint(10, 4);
    rec1.append_uint(20, 4);

    ipfix_set data_set{256}; // Based on template 256
    data_set.add_rec(rec1);
    msg.add_set(data_set);

    msg.dump();
}


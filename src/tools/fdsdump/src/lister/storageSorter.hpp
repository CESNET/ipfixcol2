
#pragma once

#include <vector>

#include <common/field.hpp>
#include <common/flow.hpp>
#include <common/ipaddr.hpp>

#include "storageRecord.hpp"

namespace fdsdump {
namespace lister {

class StorageSorter {
public:

    // Expected format "<field>,<field>:asc,<field>:desc", default is descending
    StorageSorter(const std::string desc, const shared_iemgr &iemgr);
    ~StorageSorter() = default;

    bool operator()(const StorageRecord &lhs, const StorageRecord &rhs);
    bool operator()(const Flow &lhs, const Flow &rhs);

private:
    using Sorter = int (*)(Field &, const Flow &, const Flow &);

    enum class Order {
        ascending,
        descending,
    };

    struct Item {
        Field field;
        Sorter sorter;
    };

    std::vector<Item> m_items;

    Item determine_item(const std::string &name, const shared_iemgr &iemgr) const;
    Sorter determine_sorter(const Field &field, Order order) const;
    Order determine_order(const std::string &name) const;

    static int cmp_uint_desc(Field &field, const Flow &lhs, const Flow &rhs);
    static int cmp_uint_asc(Field &field, const Flow &lhs, const Flow &rhs);
    static int cmp_datetime_desc(Field &field, const Flow &lhs, const Flow &rhs);
    static int cmp_datetime_asc(Field &field, const Flow &lhs, const Flow &rhs);
    static int cmp_ip_desc(Field &field, const Flow &lhs, const Flow &rhs);
    static int cmp_ip_asc(Field &field, const Flow &lhs, const Flow &rhs);
};

} // lister
} // fdsdump

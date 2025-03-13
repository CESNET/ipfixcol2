/**
 * @file
 * @author Lukas Hutak <hutak@cesnet.cz>
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Storage sorter
 *
 * Copyright: (C) 2024 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <vector>

#include <common/field.hpp>
#include <common/flow.hpp>
#include <common/ipaddr.hpp>

#include <lister/storageRecord.hpp>

namespace fdsdump {
namespace lister {

class StorageSorter {
public:

    // Expected format "<field>,<field>:asc,<field>:desc", default is descending
    StorageSorter(const std::string desc);
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

    Item determine_item(const std::string &name) const;
    Sorter determine_sorter(const Field &field, Order order) const;
    Order determine_order(const std::string &name) const;

    static int cmp_uint_desc(Field &field, const Flow &lhs, const Flow &rhs);
    static int cmp_uint_asc(Field &field, const Flow &lhs, const Flow &rhs);
    static int cmp_datetime_desc(Field &field, const Flow &lhs, const Flow &rhs);
    static int cmp_datetime_asc(Field &field, const Flow &lhs, const Flow &rhs);
    static int cmp_ip_desc(Field &field, const Flow &lhs, const Flow &rhs);
    static int cmp_ip_asc(Field &field, const Flow &lhs, const Flow &rhs);
    static int cmp_int_desc(Field &field, const Flow &lhs, const Flow &rhs);
    static int cmp_int_asc(Field &field, const Flow &lhs, const Flow &rhs);
    static int cmp_bool_desc(Field &field, const Flow &lhs, const Flow &rhs);
    static int cmp_bool_asc(Field &field, const Flow &lhs, const Flow &rhs);
    static int cmp_string_desc(Field &field, const Flow &lhs, const Flow &rhs);
    static int cmp_string_asc(Field &field, const Flow &lhs, const Flow &rhs);
    static int cmp_bytes_desc(Field &field, const Flow &lhs, const Flow &rhs);
    static int cmp_bytes_asc(Field &field, const Flow &lhs, const Flow &rhs);
};

} // lister
} // fdsdump

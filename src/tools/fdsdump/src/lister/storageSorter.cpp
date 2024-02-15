/**
 * @file
 * @author Lukas Hutak <hutak@cesnet.cz>
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Storage sorter
 *
 * Copyright: (C) 2024 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <algorithm>
#include <string>
#include <stdexcept>
#include <vector>

#include <common/common.hpp>
#include <common/fieldView.hpp>

#include <lister/storageSorter.hpp>

namespace fdsdump {
namespace lister {

StorageSorter::StorageSorter(const std::string desc)
{
    std::vector<std::string> fields = string_split(desc, ",");

    for (const auto &field : fields) {
        try {
            Item item = determine_item(field);
            m_items.push_back(item);
        } catch (const std::exception &ex) {
            throw std::runtime_error(
                "Unable to process order field '" + field + "': " + ex.what());
        }
    }
}

StorageSorter::Item
StorageSorter::determine_item(const std::string &name) const
{
    const size_t delim_pos = name.find_last_of('/');

    Field field {std::string{name, 0, delim_pos}};
    Order order {Order::descending};

    if (delim_pos != std::string::npos) {
        order = determine_order(std::string{name, delim_pos + 1});
    }

    Sorter sorter = determine_sorter(field, order);
    return {field, sorter};
}

StorageSorter::Sorter
StorageSorter::determine_sorter(const Field &field, Order order) const
{
    switch (field.get_type()) {
    case FieldType::num_unsigned:
        return (order == Order::descending)
            ? &StorageSorter::cmp_uint_desc
            : &StorageSorter::cmp_uint_asc;

    case FieldType::datetime:
        return (order == Order::descending)
            ? &StorageSorter::cmp_datetime_desc
            : &StorageSorter::cmp_datetime_asc;

    case FieldType::ipaddr:
        return (order == Order::descending)
            ? &StorageSorter::cmp_ip_desc
            : &StorageSorter:: cmp_ip_asc;

    case FieldType::num_signed:
        return (order == Order::descending)
            ? &StorageSorter::cmp_int_desc
            : &StorageSorter:: cmp_int_asc;

    case FieldType::boolean:
        return (order == Order::descending)
            ? &StorageSorter::cmp_bool_desc
            : &StorageSorter:: cmp_bool_asc;

    case FieldType::string:
        return (order == Order::descending)
            ? &StorageSorter::cmp_string_desc
            : &StorageSorter:: cmp_string_asc;

    case FieldType::bytes:
        return (order == Order::descending)
            ? &StorageSorter::cmp_bytes_desc
            : &StorageSorter:: cmp_bytes_asc;

    case FieldType::none:
        if (field.is_alias()) {
            throw std::domain_error("Failed to determine common data type of the alias");
        } else {
            throw std::domain_error("Unknown data type");
        }

    default:
        throw std::domain_error("Sorting of the given data type is not supported");
    }
}

StorageSorter::Order
StorageSorter::determine_order(const std::string &name) const
{
    std::string name_copy {name};

    string_trim(name_copy);

    if (name_copy == "asc" || name_copy == "a") {
        return Order::ascending;
    } else if (name_copy == "desc" || name_copy == "d") {
        return Order::descending;
    } else {
        throw std::invalid_argument("Invalid order specification: '" + name_copy + "'");
    }
}

bool
StorageSorter::operator()(const StorageRecord &lhs, const StorageRecord &rhs)
{
    return (*this)(lhs.get_flow_const(), rhs.get_flow_const());
}

bool
StorageSorter::operator()(const Flow &lhs, const Flow &rhs)
{
    // Flow records can have only one direction to be sortable
    assert(lhs.dir == DIRECTION_FWD || lhs.dir == DIRECTION_REV);
    assert(rhs.dir == DIRECTION_FWD || rhs.dir == DIRECTION_REV);

    // TODO: catch errors

    for (auto &item : m_items) {
        int ret = (item.sorter)(item.field, lhs, rhs);
        if (ret != 0) {
            return (ret < 0) ? true : false;
        }
    }

    // Undefined order
    return false;
}

static bool
get_uint_max(Field &field, const Flow &flow, uint64_t &result)
{
    bool is_reverse {flow.dir == DIRECTION_REV};
    bool found {false};
    uint64_t max {0};

    auto selector = [&](struct fds_drec_field &data) -> void {
        max = std::max(max, FieldView(data).as_uint());
        found = true;
    };

    field.for_each(
        const_cast<struct fds_drec *>(&flow.rec),
        selector,
        is_reverse);

    result = max;
    return found;
}

static bool
get_uint_min(Field &field, const Flow &flow, uint64_t &result)
{
    bool is_reverse {flow.dir == DIRECTION_REV};
    bool found {false};
    uint64_t min {UINT64_MAX};

    auto selector = [&](struct fds_drec_field &data) -> void {
        min = std::min(min, FieldView(data).as_uint());
        found = true;
    };

    field.for_each(
        const_cast<struct fds_drec *>(&flow.rec),
        selector,
        is_reverse);

    result = min;
    return found;
}

int
StorageSorter::cmp_uint_desc(
    Field &field,
    const Flow &lhs,
    const Flow &rhs)
{
    uint64_t lhs_max;
    uint64_t rhs_max;
    bool lhs_is_defined = get_uint_max(field, lhs, lhs_max);
    bool rhs_is_defined = get_uint_max(field, rhs, rhs_max);

    if (lhs_is_defined != rhs_is_defined) {
        return lhs_is_defined ? -1 : 1;
    }

    if (lhs_max == rhs_max) {
        return 0;
    } else {
        return (lhs_max > rhs_max) ? -1 : 1;
    }
}

int
StorageSorter::cmp_uint_asc(
    Field &field,
    const Flow &lhs,
    const Flow &rhs)
{
    uint64_t lhs_min;
    uint64_t rhs_min;
    bool lhs_is_defined = get_uint_min(field, lhs, lhs_min);
    bool rhs_is_defined = get_uint_min(field, rhs, rhs_min);

    if (lhs_is_defined != rhs_is_defined) {
        return lhs_is_defined ? -1 : 1;
    }

    if (lhs_min == rhs_min) {
        return 0;
    } else {
        return (lhs_min < rhs_min) ? -1 : 1;
    }
}

static bool
get_datetime_max(Field &field, const Flow &flow, struct timespec &result)
{
    bool is_reverse {flow.dir == DIRECTION_REV};
    bool found {false};
    struct timespec max {};

    auto selector = [&](struct fds_drec_field &data) -> void {
        struct timespec value = FieldView(data).as_datetime();

        if (!found) {
            max = value;
            found = true;
            return;
        }

        if (value.tv_sec > max.tv_sec) {
            max = value;
            return;
        }

        if (value.tv_sec == max.tv_sec && value.tv_nsec > max.tv_sec) {
            max = value;
            return;
        }
    };

    field.for_each(
        const_cast<struct fds_drec *>(&flow.rec),
        selector,
        is_reverse);

    result = max;
    return found;
}

static bool
get_datetime_min(Field &field, const Flow &flow, struct timespec &result)
{
    bool is_reverse {flow.dir == DIRECTION_REV};
    bool found {false};
    struct timespec min {};

    auto selector = [&](struct fds_drec_field &data) -> void {
        struct timespec value = FieldView(data).as_datetime();

        if (!found) {
            min = value;
            found = true;
            return;
        }

        if (value.tv_sec < min.tv_sec) {
            min = value;
            return;
        }

        if (value.tv_sec == min.tv_sec && value.tv_nsec < min.tv_sec) {
            min = value;
            return;
        }
    };

    field.for_each(
        const_cast<struct fds_drec *>(&flow.rec),
        selector,
        is_reverse);

    result = min;
    return found;
}

static int cmp_datetime(const struct timespec &lhs, const struct timespec &rhs)
{
    if (lhs.tv_sec != rhs.tv_sec) {
        return (lhs.tv_sec < rhs.tv_sec) ? -1 : 1;
    } else if (lhs.tv_nsec != rhs.tv_nsec) {
        return (lhs.tv_nsec < rhs.tv_nsec) ? -1 : 1;
    } else {
        return 0;
    }
}

int
StorageSorter::cmp_datetime_desc(
    Field &field,
    const Flow &lhs,
    const Flow &rhs)
{
    struct timespec lhs_max;
    struct timespec rhs_max;
    bool lhs_is_defined = get_datetime_max(field, lhs, lhs_max);
    bool rhs_is_defined = get_datetime_max(field, rhs, rhs_max);

    if (lhs_is_defined != rhs_is_defined) {
        return lhs_is_defined ? -1 : 1;
    }

    return cmp_datetime(rhs_max, lhs_max); // swapped
}

int
StorageSorter::cmp_datetime_asc(
    Field &field,
    const Flow &lhs,
    const Flow &rhs)
{
    struct timespec lhs_min;
    struct timespec rhs_min;
    bool lhs_is_defined = get_datetime_min(field, lhs, lhs_min);
    bool rhs_is_defined = get_datetime_min(field, rhs, rhs_min);

    if (lhs_is_defined != rhs_is_defined) {
        return lhs_is_defined ? -1 : 1;
    }

    return cmp_datetime(lhs_min, rhs_min);
}

static bool
get_ip_max(Field &field, const struct Flow &flow, IPAddr &result)
{
    bool is_reverse {flow.dir == DIRECTION_REV};
    bool found {false};
    IPAddr max = IPAddr::zero();

    auto selector = [&](struct fds_drec_field &data) -> void {
        IPAddr value = FieldView(data).as_ipaddr();

        if (!found) {
            max = value;
            found = true;
            return;
        }

        if (value > max) {
            max = value;
            return;
        }
    };

    field.for_each(
        const_cast<struct fds_drec *>(&flow.rec),
        selector,
        is_reverse);

    result = max;
    return found;
}

static bool
get_ip_min(Field &field, const Flow &flow, IPAddr &result)
{
    bool is_reverse {flow.dir == DIRECTION_REV};
    bool found {false};
    IPAddr min = IPAddr::zero();

    auto selector = [&](struct fds_drec_field &data) -> void {
        IPAddr value = FieldView(data).as_ipaddr();

        if (!found) {
            min = value;
            found = true;
            return;
        }

        if (value < min) {
            min = value;
            return;
        }
    };

    field.for_each(
        const_cast<struct fds_drec *>(&flow.rec),
        selector,
        is_reverse);

    result = min;
    return found;
}

int
StorageSorter::cmp_ip_desc(
    Field &field,
    const Flow &lhs,
    const Flow &rhs)
{
    IPAddr lhs_max = IPAddr::zero();
    IPAddr rhs_max = IPAddr::zero();
    bool lhs_is_defined = get_ip_max(field, lhs, lhs_max);
    bool rhs_is_defined = get_ip_max(field, rhs, rhs_max);

    if (lhs_is_defined != rhs_is_defined) {
        return lhs_is_defined ? -1 : 1;
    }

    if (lhs_max == rhs_max) {
        return 0;
    } else {
        return (lhs_max > rhs_max) ? -1 : 1;
    }
}

int
StorageSorter::cmp_ip_asc(
    Field &field,
    const Flow &lhs,
    const Flow &rhs)
{
    IPAddr lhs_min = IPAddr::zero();
    IPAddr rhs_min = IPAddr::zero();
    bool lhs_is_defined = get_ip_min(field, lhs, lhs_min);
    bool rhs_is_defined = get_ip_min(field, rhs, rhs_min);

    if (lhs_is_defined != rhs_is_defined) {
        return lhs_is_defined ? -1 : 1;
    }

    if (lhs_min == rhs_min) {
        return 0;
    } else {
        return (lhs_min < rhs_min) ? -1 : 1;
    }
}

static bool
get_string_max(Field &field, const Flow &flow, std::string &result)
{
    bool is_reverse {flow.dir == DIRECTION_REV};
    bool found {false};
    std::string max {};

    auto selector = [&](struct fds_drec_field &data) -> void {
        std::string value = FieldView(data).as_string();

        if (!found) {
            max = value;
            found = true;
            return;
        }

        if (value > max) {
            max = value;
            return;
        }
    };

    field.for_each(
        const_cast<struct fds_drec *>(&flow.rec),
        selector,
        is_reverse);

    result = max;
    return found;
}

static bool
get_string_min(Field &field, const Flow &flow, std::string &result)
{
    bool is_reverse {flow.dir == DIRECTION_REV};
    bool found {false};
    std::string min {};

    auto selector = [&](struct fds_drec_field &data) -> void {
        std::string value = FieldView(data).as_string();

        if (!found) {
            min = value;
            found = true;
            return;
        }

        if (value < min) {
            min = value;
            return;
        }
    };

    field.for_each(
        const_cast<struct fds_drec *>(&flow.rec),
        selector,
        is_reverse);

    result = min;
    return found;
}

int
StorageSorter::cmp_string_asc(
    Field &field,
    const Flow &lhs,
    const Flow &rhs)
{
    std::string lhs_min;
    std::string rhs_min;
    bool lhs_is_defined = get_string_min(field, lhs, lhs_min);
    bool rhs_is_defined = get_string_min(field, rhs, rhs_min);

    if (lhs_is_defined != rhs_is_defined) {
        return lhs_is_defined ? -1 : 1;
    }

    if (lhs_min == rhs_min) {
        return 0;
    } else {
        return (lhs_min < rhs_min) ? -1 : 1;
    }
}

int
StorageSorter::cmp_string_desc(
    Field &field,
    const Flow &lhs,
    const Flow &rhs)
{
    std::string lhs_max;
    std::string rhs_max;
    bool lhs_is_defined = get_string_max(field, lhs, lhs_max);
    bool rhs_is_defined = get_string_max(field, rhs, rhs_max);

    if (lhs_is_defined != rhs_is_defined) {
        return lhs_is_defined ? -1 : 1;
    }

    if (lhs_max == rhs_max) {
        return 0;
    } else {
        return (lhs_max > rhs_max) ? -1 : 1;
    }
}

static bool
get_bytes_max(Field &field, const Flow &flow, std::vector<uint8_t> &result)
{
    bool is_reverse {flow.dir == DIRECTION_REV};
    bool found {false};
    std::vector<uint8_t> max {};

    auto selector = [&](struct fds_drec_field &data) -> void {
        std::vector<uint8_t> value = FieldView(data).as_bytes();

        if (!found) {
            max = value;
            found = true;
            return;
        }

        if (value > max) {
            max = value;
            return;
        }
    };

    field.for_each(
        const_cast<struct fds_drec *>(&flow.rec),
        selector,
        is_reverse);

    result = max;
    return found;
}

static bool
get_bytes_min(Field &field, const Flow &flow, std::vector<uint8_t> &result)
{
    bool is_reverse {flow.dir == DIRECTION_REV};
    bool found {false};
    std::vector<uint8_t> min {};

    auto selector = [&](struct fds_drec_field &data) -> void {
        std::vector<uint8_t> value = FieldView(data).as_bytes();

        if (!found) {
            min = value;
            found = true;
            return;
        }

        if (value < min) {
            min = value;
            return;
        }
    };

    field.for_each(
        const_cast<struct fds_drec *>(&flow.rec),
        selector,
        is_reverse);

    result = min;
    return found;
}

int
StorageSorter::cmp_bytes_asc(
    Field &field,
    const Flow &lhs,
    const Flow &rhs)
{
    std::vector<uint8_t> lhs_min;
    std::vector<uint8_t> rhs_min;
    bool lhs_is_defined = get_bytes_min(field, lhs, lhs_min);
    bool rhs_is_defined = get_bytes_min(field, rhs, rhs_min);

    if (lhs_is_defined != rhs_is_defined) {
        return lhs_is_defined ? -1 : 1;
    }

    if (lhs_min == rhs_min) {
        return 0;
    } else {
        return (lhs_min < rhs_min) ? -1 : 1;
    }
}

int
StorageSorter::cmp_bytes_desc(
    Field &field,
    const Flow &lhs,
    const Flow &rhs)
{
    std::vector<uint8_t> lhs_max;
    std::vector<uint8_t> rhs_max;
    bool lhs_is_defined = get_bytes_max(field, lhs, lhs_max);
    bool rhs_is_defined = get_bytes_max(field, rhs, rhs_max);

    if (lhs_is_defined != rhs_is_defined) {
        return lhs_is_defined ? -1 : 1;
    }

    if (lhs_max == rhs_max) {
        return 0;
    } else {
        return (lhs_max > rhs_max) ? -1 : 1;
    }
}

static bool
get_int_max(Field &field, const Flow &flow, int64_t &result)
{
    bool is_reverse {flow.dir == DIRECTION_REV};
    bool found {false};
    int64_t max {INT64_MIN};

    auto selector = [&](struct fds_drec_field &data) -> void {
        max = std::max(max, FieldView(data).as_int());
        found = true;
    };

    field.for_each(
        const_cast<struct fds_drec *>(&flow.rec),
        selector,
        is_reverse);

    result = max;
    return found;
}

static bool
get_int_min(Field &field, const Flow &flow, int64_t &result)
{
    bool is_reverse {flow.dir == DIRECTION_REV};
    bool found {false};
    int64_t min {INT64_MAX};

    auto selector = [&](struct fds_drec_field &data) -> void {
        min = std::min(min, FieldView(data).as_int());
        found = true;
    };

    field.for_each(
        const_cast<struct fds_drec *>(&flow.rec),
        selector,
        is_reverse);

    result = min;
    return found;
}

int
StorageSorter::cmp_int_asc(
    Field &field,
    const Flow &lhs,
    const Flow &rhs)
{
    int64_t lhs_min;
    int64_t rhs_min;
    bool lhs_is_defined = get_int_min(field, lhs, lhs_min);
    bool rhs_is_defined = get_int_min(field, rhs, rhs_min);

    if (lhs_is_defined != rhs_is_defined) {
        return lhs_is_defined ? -1 : 1;
    }

    if (lhs_min == rhs_min) {
        return 0;
    } else {
        return (lhs_min < rhs_min) ? -1 : 1;
    }
}

int
StorageSorter::cmp_int_desc(
    Field &field,
    const Flow &lhs,
    const Flow &rhs)
{
    int64_t lhs_max;
    int64_t rhs_max;
    bool lhs_is_defined = get_int_max(field, lhs, lhs_max);
    bool rhs_is_defined = get_int_max(field, rhs, rhs_max);

    if (lhs_is_defined != rhs_is_defined) {
        return lhs_is_defined ? -1 : 1;
    }

    if (lhs_max == rhs_max) {
        return 0;
    } else {
        return (lhs_max > rhs_max) ? -1 : 1;
    }
}

static bool
get_bool_max(Field &field, const Flow &flow, bool &result)
{
    bool is_reverse {flow.dir == DIRECTION_REV};
    bool found {false};
    bool max {false};

    auto selector = [&](struct fds_drec_field &data) -> void {
        max = max || FieldView(data).as_bool();
        found = true;
    };

    field.for_each(
        const_cast<struct fds_drec *>(&flow.rec),
        selector,
        is_reverse);

    result = max;
    return found;
}

static bool
get_bool_min(Field &field, const Flow &flow, bool &result)
{
    bool is_reverse {flow.dir == DIRECTION_REV};
    bool found {false};
    int64_t min {true};

    auto selector = [&](struct fds_drec_field &data) -> void {
        min = min && FieldView(data).as_bool();
        found = true;
    };

    field.for_each(
        const_cast<struct fds_drec *>(&flow.rec),
        selector,
        is_reverse);

    result = min;
    return found;
}

int
StorageSorter::cmp_bool_asc(
    Field &field,
    const Flow &lhs,
    const Flow &rhs)
{
    bool lhs_min;
    bool rhs_min;
    bool lhs_is_defined = get_bool_min(field, lhs, lhs_min);
    bool rhs_is_defined = get_bool_min(field, rhs, rhs_min);

    if (lhs_is_defined != rhs_is_defined) {
        return lhs_is_defined ? -1 : 1;
    }

    if (lhs_min == rhs_min) {
        return 0;
    } else {
        return (lhs_min < rhs_min) ? -1 : 1;
    }
}

int
StorageSorter::cmp_bool_desc(
    Field &field,
    const Flow &lhs,
    const Flow &rhs)
{
    bool lhs_max;
    bool rhs_max;
    bool lhs_is_defined = get_bool_max(field, lhs, lhs_max);
    bool rhs_is_defined = get_bool_max(field, rhs, rhs_max);

    if (lhs_is_defined != rhs_is_defined) {
        return lhs_is_defined ? -1 : 1;
    }

    if (lhs_max == rhs_max) {
        return 0;
    } else {
        return (lhs_max > rhs_max) ? -1 : 1;
    }
}

} // lister
} // fdsdump

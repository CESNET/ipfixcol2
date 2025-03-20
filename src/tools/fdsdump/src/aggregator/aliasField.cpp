/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Alias field
 *
 * Copyright: (C) 2024 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <aggregator/aliasField.hpp>

#include <algorithm>
#include <map>
#include <stdexcept>
#include <utility>

namespace fdsdump {
namespace aggregator {

static DataType
get_common_data_type(const std::vector<IpfixField> &fields)
{
    assert(!fields.empty());

    static const std::map<std::pair<DataType, DataType>, DataType> common_type_map {
        {{DataType::Unsigned8, DataType::Unsigned16}, DataType::Unsigned16},
        {{DataType::Unsigned8, DataType::Unsigned32}, DataType::Unsigned32},
        {{DataType::Unsigned8, DataType::Unsigned64}, DataType::Unsigned64},
        {{DataType::Unsigned16, DataType::Unsigned32}, DataType::Unsigned32},
        {{DataType::Unsigned16, DataType::Unsigned64}, DataType::Unsigned64},
        {{DataType::Unsigned32, DataType::Unsigned64}, DataType::Unsigned64},

        {{DataType::Signed8, DataType::Signed16}, DataType::Signed16},
        {{DataType::Signed8, DataType::Signed32}, DataType::Signed32},
        {{DataType::Signed8, DataType::Signed64}, DataType::Signed64},
        {{DataType::Signed16, DataType::Signed32}, DataType::Signed32},
        {{DataType::Signed16, DataType::Signed64}, DataType::Signed64},
        {{DataType::Signed32, DataType::Signed64}, DataType::Signed64},

        {{DataType::IPv4Address, DataType::IPv6Address}, DataType::IPAddress},
        {{DataType::IPv4Address, DataType::IPAddress}, DataType::IPAddress},
        {{DataType::IPv6Address, DataType::IPAddress}, DataType::IPAddress},
    };

    auto unify = [&](DataType a, DataType b) -> DataType {
        if (a == b) {
            return a;
        }

        auto it = common_type_map.find({a, b});
        if (it != common_type_map.end()) {
            return it->second;
        }

        it = common_type_map.find({b, a});
        if (it != common_type_map.end()) {
            return it->second;
        }

        throw std::invalid_argument("cannot get common data type for alias fields");
    };

    DataType data_type = fields.front().data_type();
    for (const auto &field : fields) {
        data_type = unify(data_type, field.data_type());
    }
    return data_type;
}

AliasField::AliasField(const fds_iemgr_alias &alias)
{
    if (alias.sources_cnt == 0) {
        throw std::invalid_argument("alias has zero source elements");
    }

    for (size_t i = 0; i < alias.sources_cnt; ++i) {
        m_sources.push_back(IpfixField(*alias.sources[i]));
    }

    DataType data_type = get_common_data_type(m_sources);
    set_data_type(data_type);

    set_name(alias.name);
}

bool
AliasField::load(FlowContext &ctx, Value &value) const
{
    for (const auto &source : m_sources) {

        if (source.data_type() == data_type()) {
            if (source.load(ctx, value)) {
                return true;
            }

        } else {
            Value tmp_value;

            if (source.load(ctx, tmp_value)) {
                auto tmp_value_view = ValueView(source.data_type(), tmp_value);

                switch (data_type()) {
                case DataType::IPAddress:
                    value.ip = tmp_value_view.as_ip();
                    break;
                case DataType::Unsigned16:
                    value.u16 = tmp_value_view.as_u16();
                    break;
                case DataType::Unsigned32:
                    value.u32 = tmp_value_view.as_u32();
                    break;
                case DataType::Unsigned64:
                    value.u64 = tmp_value_view.as_u64();
                    break;
                case DataType::Signed16:
                    value.i16 = tmp_value_view.as_i16();
                    break;
                case DataType::Signed32:
                    value.i32 = tmp_value_view.as_i32();
                    break;
                case DataType::Signed64:
                    value.i64 = tmp_value_view.as_i64();
                    break;
                default:
                    throw std::logic_error("unexpected alias field data type");
                }
                return true;
            }
        }
    }

    return false;
}

std::string
AliasField::repr() const
{
    std::string sources = "";
    for (size_t i = 0; i < m_sources.size(); i++) {
        if (i > 0) {
            sources += ", ";
        }
        sources += m_sources[i].repr();
    }

    return std::string("AliasField(") +
        + "name=" + name() + ", data_type=" + data_type_to_str(data_type())
        + ", size=" + std::to_string(size()) + ", offset=" + std::to_string(offset())
        + ", sources=[" + sources + "])";
}

bool
AliasField::operator==(const AliasField &other) const
{
    return m_sources.size() == other.m_sources.size()
        && std::equal(m_sources.begin(), m_sources.end(), other.m_sources.begin());
}

bool
AliasField::operator==(const Field &other) const
{
    if (const auto *other_alias = dynamic_cast<const AliasField *>(&other)) {
        return *this == *other_alias;
    } else {
        return false;
    }
}

} // aggregator
} // fdsdump

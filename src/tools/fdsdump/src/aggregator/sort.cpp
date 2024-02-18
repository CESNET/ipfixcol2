/**
 * @file
 * @author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * @brief View sort functions
 */

#include <algorithm>
#include <stdexcept>

#include "common/common.hpp"

#include "informationElements.hpp"
#include "sort.hpp"
#include "viewOld.hpp"

namespace fdsdump {
namespace aggregator {

static SortDir
get_default_sort_dir(ViewField *field)
{
    //TODO
    if (field->kind == ViewFieldKind::VerbatimKey && field->pen == IPFIX::iana && field->id == IPFIX::protocolIdentifier) {
        return SortDir::Ascending;
    }

    if (field->kind == ViewFieldKind::BidirectionalPortKey) {
        return SortDir::Ascending;
    }

    switch (field->data_type) {
    case DataType::IPv4Address:
    case DataType::IPv6Address:
    case DataType::IPAddress:
    case DataType::MacAddress:
        return SortDir::Ascending;

    default:
        return SortDir::Descending;
    }
}

std::vector<SortField>
make_sort_def(ViewDefinition &def, const std::string &sort_fields_str)
{
    std::vector<SortField> sort_fields;

    if (sort_fields_str.empty()) {
        return sort_fields;
    }

    for (const auto &sort_field_str : string_split(sort_fields_str, ",")) {
        SortField sort_field = {};

        auto pieces = string_split(sort_field_str, "/");

        if (pieces.size() > 2) {
            throw std::invalid_argument("Invalid sort field \"" + sort_field_str + "\" (invalid format)");
        }

        const std::string &field_name = pieces[0];

        sort_field.field = find_field(def, field_name);
        if (!sort_field.field) {
            throw std::invalid_argument("Invalid sort field \"" + sort_field_str + "\" (field not found)");
        }

        if (pieces.size() == 2) {
            if (pieces[1] == "asc") {
                sort_field.dir = SortDir::Ascending;
            } else if (pieces[1] == "desc") {
                sort_field.dir = SortDir::Descending;
            } else {
                throw std::invalid_argument("Invalid sort field \"" + sort_field_str + "\" (invalid ordering)");
            }
        } else {
            sort_field.dir = get_default_sort_dir(sort_field.field);
        }

        sort_fields.push_back(sort_field);
    }

    return sort_fields;
}

static int
compare_values(const ViewField &field, const ViewValue &a, const ViewValue &b)
{
    switch (field.data_type) {
    case DataType::Unsigned8:
        return a.u8 - b.u8;

    case DataType::Signed8:
        return a.i8 - b.i8;

    case DataType::Unsigned16:
        return a.u16 - b.u16;

    case DataType::Signed16:
        return a.i16 - b.i16;

    case DataType::Unsigned32:
        return a.u32 - b.u32;

    case DataType::Signed32:
        return a.i32 - b.i32;

    case DataType::DateTime:
    case DataType::Unsigned64:
        return a.u64 - b.u64;

    case DataType::Signed64:
        return a.i64 - b.i64;

    case DataType::IPv4Address:
        return memcmp(a.ipv4, b.ipv6, sizeof(a.ipv4));

    case DataType::IPv6Address:
        return memcmp(a.ipv6, b.ipv6, sizeof(a.ipv6));

    case DataType::IPAddress:
        return a.ip.length != b.ip.length ? a.ip.length - b.ip.length
            : a.ip.length == 4 ? memcmp(a.ipv4, b.ipv4, sizeof(a.ipv4))
            : memcmp(a.ipv6, b.ipv6, sizeof(a.ipv6));

    default:
        assert(0 && "Comparison not implemented for data type");
    }
}

int
compare_records(
    SortField sort_field,
    const ViewDefinition &def,
    uint8_t *record,
    uint8_t *other_record)
{
    (void) def;

    int result = compare_values(*sort_field.field,
            *(ViewValue *) (record + sort_field.field->offset),
            *(ViewValue *) (other_record + sort_field.field->offset));

    if (sort_field.dir == SortDir::Ascending) {
        result = -result;
    }

    return result;
}

int
compare_records(
    const std::vector<SortField> &sort_fields,
    const ViewDefinition &def,
    uint8_t *record,
    uint8_t *other_record)
{
    (void) def;
    int result;

    for (auto sort_field : sort_fields) {
        result = compare_values(*sort_field.field,
                *(ViewValue *) (record + sort_field.field->offset),
                *(ViewValue *) (other_record + sort_field.field->offset));

        if (result != 0) {
            if (sort_field.dir == SortDir::Ascending) {
                result = -result;
            }
            break;
        }

    }

    return result;
}

std::function<bool(uint8_t *, uint8_t *)>
make_comparer(
    const std::vector<SortField> &sort_fields,
    const ViewDefinition &def,
    bool reverse)
{
    std::function<bool(uint8_t *, uint8_t *)> compare;

    if (reverse) {
        if (sort_fields.size() == 1) {
            compare = [&](uint8_t *a, uint8_t *b) -> bool {
                return compare_records(sort_fields[0], def, a, b) > 0;
            };

        } else {
            compare = [&](uint8_t *a, uint8_t *b) -> bool {
                return compare_records(sort_fields, def, a, b) > 0;
            };
        }

    } else {
        if (sort_fields.size() == 1) {
            compare = [&](uint8_t *a, uint8_t *b) -> bool {
                return compare_records(sort_fields[0], def, a, b) > 0;
            };

        } else {
            compare = [&](uint8_t *a, uint8_t *b) -> bool {
                return compare_records(sort_fields, def, a, b) > 0;
            };
        }
    }

    return compare;
}

void
sort_records(
    std::vector<uint8_t *> &records,
    const std::vector<SortField> &sort_fields,
    const ViewDefinition &def)
{
    if (sort_fields.empty()) {
        return;
    }

    std::function<bool(uint8_t *, uint8_t *)> compare = make_comparer(sort_fields, def);
    std::sort(records.begin(), records.end(), compare);
}

} // aggregator
} // fdsdump

#include "view/sort.hpp"

#include <algorithm>

#include "common.hpp"
#include "view/view.hpp"
#include "utils/util.hpp"

std::vector<SortField>
make_sort_fields(ViewDefinition &def, const std::string &sort_fields_str)
{
    std::vector<SortField> sort_fields;

    for (const auto &sort_field_str : string_split(sort_fields_str, ",")) {
        SortField sort_field = {};

        auto pieces = string_split(sort_field_str, "/");

        if (pieces.size() > 2) {
            throw ArgError("Invalid sort field \"" + sort_field_str + "\" - invalid format");
        }

        const std::string &field_name = pieces[0];

        if (pieces.size() == 2) {
            if (pieces[1] == "asc") {
                sort_field.ascending = true;
            } else if (pieces[1] == "desc") {
                sort_field.ascending = false;
            } else {
                throw ArgError("Invalid sort field \"" + sort_field_str + "\" - invalid ordering");
            }
        }

        sort_field.field = find_field(def, field_name);
        if (!sort_field.field) {
            throw ArgError("Invalid sort field \"" + sort_field_str + "\" - field not found");
        }

        sort_fields.push_back(sort_field);
    }

    return sort_fields;
}

static int
compare_values(const ViewField &field, const ViewValue &a, const ViewValue &b)
{
    switch (field.data_type) {
    case DataType::DateTime:
    case DataType::Unsigned64:
        return a.u64 > b.u64 ? 1 : (a.u64 < b.u64 ? -1 : 0);

    case DataType::Signed64:
        return a.i64 > b.i64 ? 1 : (a.i64 < b.i64 ? -1 : 0);

    default:
        assert(0);
    }
}

int
compare_records(SortField sort_field, const ViewDefinition &def, uint8_t *record, uint8_t *other_record)
{
    int result = compare_values(*sort_field.field,
                                *(ViewValue *) (record + sort_field.field->offset),
                                *(ViewValue *) (other_record + sort_field.field->offset));

    if (sort_field.ascending) {
        result = -result;
    }

    return result;
}

int
compare_records(const std::vector<SortField> &sort_fields, const ViewDefinition &def, uint8_t *record, uint8_t *other_record)
{
    int result;

    for (auto sort_field : sort_fields) {
        result = compare_values(*sort_field.field,
                                *(ViewValue *) (record + sort_field.field->offset),
                                *(ViewValue *) (other_record + sort_field.field->offset));

        if (result != 0) {
            if (sort_field.ascending) {
                result = -result;
            }
            break;
        }

    }

    return result;
}

std::function<bool(uint8_t *, uint8_t *)>
make_comparer(const std::vector<SortField> &sort_fields, const ViewDefinition &def, bool reverse)
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
sort_records(std::vector<uint8_t *> &records, const std::vector<SortField> &sort_fields, const ViewDefinition &def)
{
    std::function<bool(uint8_t *, uint8_t *)> compare = make_comparer(sort_fields, def);

    std::sort(records.begin(), records.end(), compare);
}
#include "sorter.hpp"
#include <algorithm>

void
sort_records(std::vector<AggregateRecord *> &records, const std::string &sort_field, ViewDefinition &view_def)
{
    std::function<bool(AggregateRecord *, AggregateRecord *)> compare_fn;

    const ViewField *field = nullptr;

    //TODO: extract finding field and its offset into common as this is used in multiple places
    unsigned int offset = 0;

    for (const auto &field_ : view_def.key_fields) {
        if (field_.name == sort_field) {
            field = &field_;
            break;
        }
        offset += field_.size;
    }

    if (!field) {
        for (const auto &field_ : view_def.value_fields) {
            if (field_.name == sort_field) {
                field = &field_;
                break;
            }
            offset += field_.size;
        }
    }

    if (!field) {
        throw std::runtime_error("sort field \"" + sort_field + "\" not found");
    }

    switch (field->data_type) {
    case DataType::DateTime:
    case DataType::Unsigned64:
        compare_fn = [=](AggregateRecord *a, AggregateRecord *b) {
            return ((ViewValue *) (a->data + offset))->u64 > ((ViewValue *) (b->data + offset))->u64;
        };
        break;
    case DataType::Signed64:
        compare_fn = [=](AggregateRecord *a, AggregateRecord *b) {
            return ((ViewValue *) (a->data + offset))->i64 > ((ViewValue *) (b->data + offset))->i64;
        };
        break;
    default:
        throw std::runtime_error("sort field \"" + sort_field + "\" has unsupported data type");
    }

    std::sort(records.begin(), records.end(), compare_fn);
}

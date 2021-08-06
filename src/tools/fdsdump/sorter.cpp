#include "sorter.hpp"
#include <algorithm>

void
sort_records(std::vector<AggregateRecord *> &records, const std::string &sort_field, const ViewDefinition &view_def)
{
    auto compare_fn = get_compare_fn(sort_field, view_def);
    std::sort(records.begin(), records.end(), compare_fn);
}

CompareFn
get_compare_fn(const std::string &sort_field, const ViewDefinition &view_def)
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

    return compare_fn;
}

static size_t left(size_t i) { return 2*i + 1; }

static size_t right(size_t i) { return 2*i + 2; }

static void
fix_heap(size_t i, std::vector<AggregateRecord *> &records, CompareFn &compare)
{
    size_t left = 2*i + 1;
    size_t right = 2*i + 2;

    if (left < records.size() && records[left] && compare(records[i], records[left])) {
        if (right < records.size() && records[right] && compare(records[right], records[left])) {
            std::swap(records[i], records[left]);
            fix_heap(left, records, compare);

        } else {
            std::swap(records[i], records[right]);
            fix_heap(right, records, compare);

        }

    } else if (right < records.size() && records[right] && compare(records[i], records[right])) {
        std::swap(records[i], records[right]);
        fix_heap(right, records, compare);

    }
}

void
keep_top_n(std::vector<AggregateRecord *> &records, size_t n, CompareFn &compare_fn)
{
    std::vector<AggregateRecord *> top_records(n);

    for (size_t i = 0; i < n; i++) {

    }

}
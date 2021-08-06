#pragma once
#include <vector>
#include <functional>
#include "view.hpp"
#include "aggregatetable.hpp"

using CompareFn = std::function<bool(AggregateRecord *, AggregateRecord *)>;

void
sort_records(std::vector<AggregateRecord *> &records, const std::string &sort_field, const ViewDefinition &view_def);

CompareFn
get_compare_fn(const std::string &sort_field, const ViewDefinition &view_def);

void
keep_top_n(std::vector<AggregateRecord *> &records, size_t n, CompareFn &compare_fn);
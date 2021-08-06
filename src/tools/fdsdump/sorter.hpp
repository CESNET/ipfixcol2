#pragma once
#include <vector>
#include <functional>
#include "view.hpp"
#include "hashtable.hpp"

using CompareFn = std::function<bool(uint8_t *, uint8_t *)>;

void
sort_records(std::vector<uint8_t *> &records, const std::string &sort_field, const ViewDefinition &view_def);

CompareFn
get_compare_fn(const std::string &sort_field, const ViewDefinition &view_def);

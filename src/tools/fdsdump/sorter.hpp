#pragma once
#include <vector>
#include <functional>
#include "view.hpp"
#include "aggregate_table.hpp"

void
sort_records(std::vector<AggregateRecord *> &records, const std::string &sort_field, ViewDefinition &config);
#pragma once

#include "view/view.hpp"

#include <vector>
#include <string>
#include <functional>

struct SortField {
    ViewField *field;
    bool ascending;
};

std::vector<SortField>
make_sort_fields(ViewDefinition &def, const std::string &sort_fields_str);

int
compare_records(SortField sort_field, const ViewDefinition &def, uint8_t *record, uint8_t *other_record);

std::function<bool(uint8_t *, uint8_t *)>
make_comparer(const std::vector<SortField> &sort_fields, const ViewDefinition &def, bool reverse = false);

int
compare_records(const std::vector<SortField> &sort_fields, const ViewDefinition &def, uint8_t *record, uint8_t *other_record);

void
sort_records(std::vector<uint8_t *> &records, const std::vector<SortField> &sort_fields, const ViewDefinition &def);
/**
 * @file
 * @author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * @brief View sort functions
 */
#pragma once

#include "viewOld.hpp"

#include <vector>
#include <string>
#include <functional>

namespace fdsdump {
namespace aggregator {

enum class SortDir {
    Ascending,
    Descending
};

/**
 * @brief A field to sort on.
 */
struct SortField {
    ViewField *field;
    SortDir dir;
};

/**
 * @brief Parse a sort options string into a sort definition
 * @param def             The view definition
 * @param sort_fields_str The sort options string
 * @return The parsed sort fields
 */
std::vector<SortField>
make_sort_def(ViewDefinition &def, const std::string &sort_fields_str);

/**
 * @brief      Compare view records
 *
 * @param[in]  sort_field    The field to compare on
 * @param[in]  def           The view definition
 * @param[in]  record        The record
 * @param[in]  other_record  The other record
 * @return
 *   1 if record should be ordered BEFORE the other record,
 *   0 if the records are equal,
 *   -1 if the record should be ordered AFTER the other record
 */
int
compare_records(
    SortField sort_field,
    const ViewDefinition &def,
    uint8_t *record,
    uint8_t *other_record);


/**
 * @brief      Compare view records
 * @param sort_fields   The fields to compare on
 * @param def           The view definition
 * @param record        The record
 * @param other_record  The other record
 * @return
 *   1 if record should be ordered BEFORE the other record,
 *   0 if the records are equal,
 *   -1 if the record should be ordered AFTER the other record
 */
int
compare_records(
    const std::vector<SortField> &sort_fields,
    const ViewDefinition &def,
    uint8_t *record,
    uint8_t *other_record);

/**
 * @brief Make a record comparer function
 * @param sort_fields  The sort definition
 * @param def          The view definition
 * @param reverse      Whether the compare_records output should be reversed
 * @return     The comparer function
 */
std::function<bool(uint8_t *, uint8_t *)>
make_comparer(
    const std::vector<SortField> &sort_fields,
    const ViewDefinition &def,
    bool reverse = false);

/**
 * @brief Sort view records
 * @param[inout] records      The view records
 * @param[in]    sort_fields  The fields to sort on
 * @param[in]    def          The view definition
 */
void
sort_records(
    std::vector<uint8_t *> &records,
    const std::vector<SortField> &sort_fields,
    const ViewDefinition &def);

} // aggregator
} // fdsdump

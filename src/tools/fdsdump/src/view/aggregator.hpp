#pragma once

#include <array>
#include <vector>
#include <cstdint>

#include <libfds.h>

#include "utils/hashtable.hpp"
#include "view/sort.hpp"


class Aggregator
{
public:
    Aggregator(ViewDefinition view_def);

    void
    process_record(fds_drec &drec);

    void
    merge(Aggregator &other);

    HashTable m_table;

    std::vector<uint8_t *> &items() { return m_table.items(); }

private:
    ViewDefinition m_view_def;

    std::vector<uint8_t> m_key_buffer;

    void
    aggregate(fds_drec &drec, Direction direction);
};

std::vector<uint8_t *>
make_top_n(const ViewDefinition &def, std::vector<Aggregator *> &aggregators, size_t n, const std::vector<SortField> &sort_fields);

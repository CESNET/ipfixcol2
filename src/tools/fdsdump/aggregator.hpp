#pragma once

#include <array>
#include <vector>
#include <cstdint>
#include <libfds.h>
#include "config.hpp"
#include "hashtable.hpp"
#include "sorter.hpp"

class Aggregator
{
public:
    Aggregator(ViewDefinition view_def);

    void
    process_record(fds_drec &drec);

    void
    merge(Aggregator &other);

    void
    make_top_n(size_t n, CompareFn compare_fn);

    HashTable m_table;

    std::vector<uint8_t *> m_items;

private:
    ViewDefinition m_view_def;

    std::vector<uint8_t> m_key_buffer;

    void
    aggregate(fds_drec &drec, Direction direction);
};

std::vector<uint8_t *>
make_top_n(const ViewDefinition &def, std::vector<Aggregator *> &aggregators, size_t n, const std::vector<SortField> &sort_fields);

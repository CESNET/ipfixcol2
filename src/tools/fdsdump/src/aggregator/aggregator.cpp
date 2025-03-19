/**
 * @file
 * @author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * @brief Aggregator
 *
 * Copyright: (C) 2024 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <aggregator/field.hpp>
#define XXH_INLINE_ALL

#include <aggregator/aggregator.hpp>

#include <algorithm>
#include <cstring>
#include <iostream>
#include <functional>

#include "3rd_party/xxhash/xxhash.h"
#include <common/common.hpp>
#include <common/fieldView.hpp>

#include "binaryHeap.hpp"
#include "informationElements.hpp"
#include <aggregator/print.hpp>

namespace fdsdump {
namespace aggregator {

// Merge two hash tables containing records defined by view
void merge_hash_tables(const View &view, HashTable &dst_table, HashTable &src_table)
{
    uint8_t *dst_record = nullptr;
    for (uint8_t *src_record : src_table.items()) {
        bool found = dst_table.find_or_create(src_record, dst_record);
        if (found) {
            // If found - merge
            for (auto x : view.iter_values(dst_record, src_record)) {
                x.field.merge(x.value1, x.value2);
            }
        } else {
            // If not found - copy over
            // Key is already copied by find_or_create, so only value needs to be copied
            unsigned int key_size = view.key_size(src_record);
            unsigned int value_size = view.value_size();
            //TODO: Possible optimalization: Can we just move pointers instead of memcpy?
            std::memcpy(dst_record + key_size, src_record + key_size, value_size);
        }
    }
}

// Sort records
void sort_records(const View &view, std::vector<uint8_t *>& records)
{
    if (view.order_fields().empty()) {
        return;
    }
    std::sort(records.begin(), records.end(),
            [&view](uint8_t *a, uint8_t *b) { return view.ordered_before(a, b); });
}

Aggregator::Aggregator(const View &view) :
    m_table(view),
    m_key_buffer(65535),
    m_view(view)
{
}

void
Aggregator::process_record(Flow &flow)
{
    FlowContext ctx(flow.rec);

    if ((flow.dir & DIRECTION_FWD) != 0) {
        ctx.flow_dir = FlowDirection::Forward;
        if (m_view.has_inout_fields()) {
            ctx.view_dir = ViewDirection::In;
            aggregate(ctx);
            ctx.view_dir = ViewDirection::Out;
            aggregate(ctx);
        } else {
            aggregate(ctx);
        }
    }

    if ((flow.dir & DIRECTION_REV) != 0) {
        ctx.flow_dir = FlowDirection::Reverse;
        if (m_view.has_inout_fields()) {
            ctx.view_dir = ViewDirection::In;
            aggregate(ctx);
            ctx.view_dir = ViewDirection::Out;
            aggregate(ctx);
        } else {
            aggregate(ctx);
        }
    }
}

void
Aggregator::aggregate(FlowContext &ctx)
{
    // build key
    uint32_t size = 0;
    uint8_t *ptr = m_key_buffer.data();
    if (!m_view.is_fixed_size()) {
        size += sizeof(uint32_t);
    }
    for (const auto &pair : m_view.iter_keys(ptr)) {
        if (!pair.field.load(ctx, pair.value)) {
            return;
        }
        if (!m_view.is_fixed_size()) {
            size += pair.field.size(&pair.value);
        }
    }
    if (!m_view.is_fixed_size()) {
        *reinterpret_cast<uint32_t *>(m_key_buffer.data()) = size;
    }

    // find in hash table
    uint8_t *rec;
    if (!m_table.find_or_create(m_key_buffer.data(), rec)) {
        // init fields
        for (const auto &pair : m_view.iter_values(rec)) {
            pair.field.init(pair.value);
        }
    }

    // aggregate
    for (const auto &pair : m_view.iter_values(rec)) {
        pair.field.aggregate(ctx, pair.value);
    }
}

void
Aggregator::sort_items()
{
    sort_records(m_view, items());
}

void
Aggregator::merge(Aggregator &other)
{
    merge_hash_tables(m_view, m_table, other.m_table);
}

} // aggregator
} // fdsdump

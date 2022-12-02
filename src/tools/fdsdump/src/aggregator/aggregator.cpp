/**
 * @file
 * @author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * @brief Aggregator
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
    if (!m_view.order_fields().empty()) {
        std::sort(items().begin(), items().end(),
            [this](uint8_t *a, uint8_t *b) { return m_view.ordered_before(a, b); });
    }
}


//     uint8_t *record;

//     if (!m_table.find_or_create(&m_key_buffer[0], record)) {
//         init_values(*m_view_def.get(), record + m_view_def->keys_size);
//     }

//     for (const auto &iter : m_view_def->iter_values(record + m_view_def->keys_size)) {
//         aggregate_value(iter.field, drec, &iter.value, direction, drec_find_flags);
//     }
// }

// void
// Aggregator::merge(Aggregator &other, unsigned int max_num_items)
// {
//     unsigned int n = 0;
//     for (uint8_t *other_record : other.items()) {
//         if (max_num_items != 0 && n == max_num_items) {
//             break;
//         }

//         uint8_t *record;

//         if (!m_table.find_or_create(other_record, record)) {
//             //TODO: this copy is unnecessary, we could just take the already allocated record from the other table instead
//             memcpy(record, other_record, m_view_def->keys_size + m_view_def->values_size);
//         } else {
//             merge_records(*m_view_def.get(), record, other_record);
//         }

//         n++;
//     }
// }

} // aggregator
} // fdsdump

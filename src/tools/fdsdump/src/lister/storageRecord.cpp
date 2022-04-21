
#include <cstring>
#include <cstdint>
#include <stdexcept>

#include "storageRecord.hpp"

StorageRecord::StorageRecord(
        const struct fds_drec &rec,
        enum Direction dir,
        const shared_tsnapshot &snapshot)
    : m_data{new uint8_t[rec.size]}, m_snapshot{snapshot}
{
    const uint16_t tmplt_id = rec.tmplt->id;
    const fds_template *tmplt = fds_tsnapshot_template_get(m_snapshot.get(), tmplt_id);

    if (!tmplt) {
        throw std::runtime_error("Snapshot doesn't contain required template");;
    }

    std::memcpy(m_data.get(), rec.data, rec.size);

    m_flow.dir = dir;
    m_flow.rec.data = m_data.get();
    m_flow.rec.size = rec.size;
    m_flow.rec.tmplt = tmplt;
    m_flow.rec.snap = m_snapshot.get();
}

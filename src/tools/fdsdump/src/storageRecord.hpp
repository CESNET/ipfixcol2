
#pragma once

#include <libfds.h>
#include <memory>

#include "common.hpp"
#include "flow.hpp"

/**
 * @brief Flow storage record.
 *
 * The flow record contains the whole IPFIX Data Record and a reference to
 * a template snapshot necessary for its interpretation.
 */
class StorageRecord {
public:
    /**
     * @brief Create a storage record by creating a copy of IPFIX Data Record
     * extracted, for example, from FDS File.
     *
     * @param rec      Flow data record to be stored
     * @param dir      Direction of the record to be considered.
     * @param snapshot Template snapshot that should be used
     */
    StorageRecord(
        const struct fds_drec &rec,
        enum Direction dir,
        const shared_tsnapshot &snapshot);
    ~StorageRecord() = default;

    Flow &
    get_flow() { return m_flow; };

    const Flow &
    get_flow_const() const { return m_flow; };

private:
    std::unique_ptr<uint8_t[]> m_data;
    shared_tsnapshot m_snapshot;
    Flow m_flow;
};


#include <new>
#include <stdexcept>
#include <iostream>

#include <common/fieldView.hpp>
#include <common/flowProvider.hpp>
#include <common/ieMgr.hpp>

namespace fdsdump {

FlowProvider::FlowProvider()
{
    int ret;

    m_file.reset(fds_file_init());
    if (!m_file) {
        throw std::runtime_error("fds_file_init() has failed");
    }

    ret = fds_file_set_iemgr(m_file.get(), IEMgr::instance().ptr());
    if (ret != FDS_OK) {
        throw std::runtime_error("fds_file_set_iemgr() has failed: " + std::to_string(ret));
    }
}

void
FlowProvider::add_file(const std::string &file)
{
    m_remains.push_back(file);

    unique_file file_obj(fds_file_init(), &fds_file_close);
    if (!file_obj) {
        throw std::runtime_error("fds_file_init() has failed");
    }

    int ret;
    ret = fds_file_open(file_obj.get(), file.c_str(), FDS_FILE_READ);
    if (ret != FDS_OK) {
        const std::string err_msg = fds_file_error(file_obj.get());

        std::cerr << "fds_file_open('" << file << "') failed: " << err_msg << std::endl;
    }

    const fds_file_stats *stats = fds_file_stats_get(file_obj.get());
    if (stats) {
        m_total_flow_count += stats->recs_total;
    } else {
        std::cerr << "WARNING: " << file << " has no stats" << std::endl;
    }
}

void
FlowProvider::set_filter(const std::string &expr)
{
    fds_ipfix_filter_t *filter;
    int ret;

    ret = fds_ipfix_filter_create(&filter, IEMgr::instance().ptr(), expr.c_str());
    m_filter.reset(filter);

    if (ret != FDS_OK) {
        const std::string err_msg = fds_ipfix_filter_get_error(filter);
        throw std::runtime_error("fds_ipfix_filter_create() has failed: " + err_msg);
    }
}

void
FlowProvider::set_biflow_autoignore(bool enable)
{
    m_biflow_autoignore = enable;
}

bool
FlowProvider::prepare_next_file()
{
    while (true) {
        const std::string *next_file;
        int ret;

        // Check if there are more files to read...
        if (m_remains.empty()) {
            return false;
        }

        next_file = &m_remains.front();

        // Try to open it
        ret = fds_file_open(m_file.get(), next_file->c_str(), FDS_FILE_READ);
        if (ret != FDS_OK) {
            const std::string err_msg = fds_file_error(m_file.get());

            std::cerr << "fds_file_open('" << *next_file << "') failed: " << err_msg << std::endl;
            m_remains.pop_front();
            continue;
        }

        m_remains.pop_front();
        return true;
    }
}

bool
FlowProvider::prepare_next_record()
{
    int ret;

    ret = fds_file_read_rec(m_file.get(), &m_flow.rec, NULL);
    switch (ret) {
    case FDS_OK:
        m_processed_flow_count++;
        return true;
    case FDS_EOC:
        return false;
    default:
        throw std::runtime_error("fds_file_read_rec() has failed: " + std::to_string(ret));
    }
}

enum Direction
FlowProvider::filter_record(struct fds_drec *rec)
{
    const fds_template_flag_t tflags = rec->tmplt->flags;
    const bool is_biflow = (tflags & FDS_TEMPLATE_BIFLOW) != 0;

    if (!m_filter) {
        // No filter defined, match it
        return is_biflow ? DIRECTION_BOTH : DIRECTION_FWD;
    }

    if (is_biflow) {
        switch (fds_ipfix_filter_eval_biflow(m_filter.get(), rec)) {
        case FDS_IPFIX_FILTER_NO_MATCH:
            return DIRECTION_NONE;
        case FDS_IPFIX_FILTER_MATCH_FWD:
            return DIRECTION_FWD;
        case FDS_IPFIX_FILTER_MATCH_REV:
            return DIRECTION_REV;
        case FDS_IPFIX_FILTER_MATCH_BOTH:
            return DIRECTION_BOTH;
        }

        return DIRECTION_NONE;
    } else {
        return (fds_ipfix_filter_eval(m_filter.get(), rec))
            ? DIRECTION_FWD
            : DIRECTION_NONE;
    }
}

bool
FlowProvider::field_has_only_zero_value(struct fds_drec *rec, uint16_t id, bool reverse)
{
    const uint16_t flags = reverse ? FDS_DREC_BIFLOW_REV : FDS_DREC_BIFLOW_FWD;
    struct fds_drec_iter iter;
    int found = false;

    fds_drec_iter_init(&iter, rec, flags);

    while (fds_drec_iter_find(&iter, 0, id) != FDS_EOC) {
        FieldView field(iter.field);

        found = true;

        if (field.as_uint() != 0) {
            return false;
        }
    }

    return found;
}

bool
FlowProvider::direction_is_empty(struct fds_drec *rec, bool reverse)
{
    const uint16_t IPFIX_OCTET_DELTA = 1;
    const uint16_t IPFIX_PACKET_DELTA = 2;

    if (!field_has_only_zero_value(rec, IPFIX_OCTET_DELTA, reverse)) {
        return false;
    }

    if (!field_has_only_zero_value(rec, IPFIX_PACKET_DELTA, reverse)) {
        return false;
    }

    return true;
}

enum Direction
FlowProvider::biflow_autoignore(struct fds_drec *rec)
{
    const fds_template_flag_t tflags = rec->tmplt->flags;
    const bool is_biflow = (tflags & FDS_TEMPLATE_BIFLOW) != 0;
    int result = 0;

    if (!m_biflow_autoignore) {
        // Disabled
        return is_biflow ? DIRECTION_BOTH : DIRECTION_FWD;
    }

    if (!is_biflow) {
        return DIRECTION_FWD;
    }

    if (!direction_is_empty(rec, false)) {
        result |= DIRECTION_FWD;
    }

    if (!direction_is_empty(rec, true)) {
        result |= DIRECTION_REV;
    }

    return static_cast<enum Direction>(result);
}

Flow *
FlowProvider::next_record()
{
    int dir;

    while (true) {
        if (!m_file_ready) {
            if (!prepare_next_file()) {
                // No more file, no more records
                return nullptr;
            }

            m_file_ready = true;
        }

        if (!prepare_next_record()) {
            m_file_ready = false;
            continue;
        }

        dir = filter_record(&m_flow.rec);
        if (dir == DIRECTION_NONE) {
            continue;
        }

        dir &= biflow_autoignore(&m_flow.rec);
        if (dir == DIRECTION_NONE) {
            continue;
        }

        m_flow.dir = static_cast<enum Direction>(dir);
        return &m_flow;
    }
}

void
FlowProvider::reset_counters()
{
    m_processed_flow_count = 0;
    m_total_flow_count = 0;
}

} // fdsdump

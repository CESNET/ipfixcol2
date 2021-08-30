#pragma once

#include <vector>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <memory>

#include <ipfixcol2.h>

#include "Connection.h"

class Reconnector {
public:
    /**
     * \brief The constructor
     *
     * \param interval_secs  The sleep interval between reconnections
     * \param log_ctx        The logging context
     */
    Reconnector(unsigned int interval_secs, ipx_ctx_t *log_ctx);

    /**
     * Disable copy and move constructors
     */
    Reconnector(const Reconnector &) = delete;
    Reconnector(Reconnector &&) = delete;

    /**
     * \brief Add a connection that should be reconnected
     */
    void
    put(std::shared_ptr<Connection> &connection);

    /**
     * \brief The destructor - finish the thread
     */
    ~Reconnector();

private:
    /// Number of seconds to sleep for between attempting reconnections
    unsigned int m_interval_secs;

    /// The logging context
    ipx_ctx_t *m_log_ctx;

    /// The reconnector thread
    std::thread m_thread;

    /// Indicate whether `m_thread` should keep running
    bool m_keep_running = true;

    /// For accessing shared state
    std::mutex m_mutex;
    std::condition_variable m_cv;

    /// A buffer for connections to be put into by the `put` function and later collected by `m_thread`
    std::vector<std::shared_ptr<Connection>> m_put_connections;

    /// The connections to be reconnected, handled exclusively by `m_thread`
    std::vector<std::shared_ptr<Connection>> m_connections;

    void
    run();
};
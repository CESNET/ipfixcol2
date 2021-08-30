#include "Reconnector.h"
#include <iterator>

Reconnector::Reconnector(unsigned int interval_secs, ipx_ctx_t *log_ctx) :
    m_interval_secs(interval_secs),
    m_log_ctx(log_ctx)
{
    m_thread = std::thread([this]() { this->run(); });
}

void
Reconnector::put(std::shared_ptr<Connection> &connection)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_put_connections.push_back(connection);
}

Reconnector::~Reconnector()
{
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_keep_running = false;
        m_cv.notify_one();
    }

    m_thread.join();
}

/**
 * \brief The thread process
 */
void
Reconnector::run()
{
    while (m_keep_running) {
        // Collect connections that were `put` by the other thread
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_connections.insert(
                m_connections.end(),
                std::make_move_iterator(m_put_connections.begin()),
                std::make_move_iterator(m_put_connections.end()));
            m_put_connections.clear();
        }

        // Go through the connections and try to reconnect
        for (auto it = m_connections.begin(); it != m_connections.end(); ) {
            std::shared_ptr<Connection> &connection = *it;

            if (connection->m_finished) {
                // Connection is finished, there is no point in reconnecting
                it = m_connections.erase(it);
                continue;
            }

            try {
                IPX_CTX_INFO(m_log_ctx, "Attempting to reconnect to %s", connection->ident().c_str());
                connection->connect();
                IPX_CTX_WARNING(m_log_ctx, "A connection to %s reconnected", connection->ident().c_str());
                it = m_connections.erase(it);

            } catch (const ConnectionError &err) {
                it++;
            }
        }

        // Wait until the specified interval elapsed, or the reconnector is stopped
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            if (m_keep_running) {
                m_cv.wait_for(lock, std::chrono::seconds(m_interval_secs), [&]() { return !m_keep_running; });
            }
        }
    }
}


/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Worker class for running tasks in a separate thread
 * @date 2025
 *
 * Copyright(c) 2025 CESNET z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "common.h"
#include <thread>

/**
 * @class Worker
 * @brief A base class for creating worker threads that perform specific tasks.
 *
 * The Worker class provides a framework for running tasks in a separate thread,
 * with support for stopping the task, checking for errors, and joining the thread.
 * Derived classes must implement the `run` method to define the task logic.
 */
class Worker : Nonmoveable, Noncopyable {
public:
    /**
     * @brief Starts the worker thread and executes the task.
     */
    void start()
    {
        m_thread = std::thread([this]() { this->do_run(); });
    }

    /**
     * @brief Requests the worker to stop by setting a stop signal.
     */
    void request_stop()
    {
        if (!m_stop_signal) {
            m_stop_requested_at = std::time(nullptr);
        }
        m_stop_signal = true;
    }

    /**
     * @brief Checks if an error occurred during the task execution.
     *
     * @throws The exception captured during task execution, if any.
     */
    void check_error() const
    {
        if (m_errored) {
            std::rethrow_exception(m_exception);
        }
    }

    /**
     * @brief Joins the worker thread if it is joinable.
     */
    void join()
    {
        if (m_thread.joinable()) {
            m_thread.join();
        }
    }

    /**
     * @brief Destructor that ensures the worker thread is joined.
     */
    ~Worker()
    {
        join();
    }

protected:
    /**
     * @brief Pure virtual method to define the task logic.
     *
     * Derived classes must implement this method to specify the task to be executed.
     */
    virtual void run() = 0;

    /**
     * @brief Checks if a stop request has been made.
     *
     * @return True if a stop request has been made, otherwise false.
     */
    bool stop_requested() const
    {
        return m_stop_signal;
    }

    /**
     * Calculates the number of seconds that have passed since a stop was requested.
     *
     * @return The number of seconds elapsed since the stop request.
     */
    unsigned int secs_since_stop_requested() const
    {
        return std::time(nullptr) - m_stop_requested_at;
    }

private:
    std::thread m_thread; ///< The thread in which the task is executed.
    std::atomic_bool m_stop_signal = false; ///< Signal to request the task to stop.
    std::atomic_bool m_errored = false; ///< Flag indicating if an error occurred.
    std::exception_ptr m_exception = nullptr; ///< Pointer to the captured exception, if any.
    std::time_t m_stop_requested_at = 0; ///< When was stop requested.

    /**
     * @brief Internal method to execute the task and handle exceptions.
     */
    void do_run()
    {
        try {
            run();
        } catch (...) {
            m_exception = std::current_exception();
            m_errored = true;
        }
    }
};

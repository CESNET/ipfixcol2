/**
 * \file src/utils/threadrunner.hpp
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * \brief Generic thread runner
 *
 * Copyright (C) 2021 CESNET, z.s.p.o.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of the Company nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * ALTERNATIVELY, provided that this notice is retained in full, this
 * product may be distributed under the terms of the GNU General Public
 * License (GPL) version 2 or later, in which case the provisions
 * of the GPL apply INSTEAD OF those given above.
 *
 * This software is provided ``as is, and any express or implied
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose are disclaimed.
 * In no event shall the company or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even
 * if advised of the possibility of such damage.
 *
 */
#pragma once

#include <thread>
#include <stdexcept>
#include <functional>
#include <vector>

/**
 * \brief      A class to manage running callable objects in threads
 *
 * \tparam     Callable  Type of the runnable
 */
template <typename Callable>
class ThreadRunner {
public:
    /**
     * \brief      Constructs a new instance, a new thread is created for each of the callable
     *             where the callable is called
     *
     * \param      callables  The callables
     */
    ThreadRunner(std::vector<Callable> &callables) :
        m_threads(callables.size())
    {
        for (size_t i = 0; i < callables.size(); i++) {
            ThreadState *ts = &m_threads[i];
            Callable *callable = &callables[i];
            ts->thread = std::thread([ts, callable]() {
                try {
                    (*callable)();
                    ts->done = true;

                } catch (...) {
                    ts->exception = std::current_exception();
                }
            });
        }
    }

    /**
     * \brief      Check if the threads finished or if an exception occured
     *
     * \throw      The exception that was caught in one of the threads
     *
     * \return     true if everything finished, false if there are still threads running
     */
    bool
    poll()
    {
        bool done = true;
        
        for (auto &ts : m_threads) {
            if (ts.exception) {
                std::rethrow_exception(ts.exception);
                ts.exception = nullptr;
            }

            done &= ts.done;
        }

        return done;
    }


    /**
     * \brief      Block until all the running threads are finished
     */
    void
    join()
    {
        for (auto &ts : m_threads) {
            if (ts.exception) {
                std::rethrow_exception(ts.exception);
                ts.exception = nullptr;
            }

            if (!ts.thread.joinable()) {
                continue;
            }

            ts.thread.join();
        }
    }

private:
    struct ThreadState {
        std::thread thread;
        bool done = false;
        std::exception_ptr exception = nullptr;
    };

    std::vector<ThreadState> m_threads;
};

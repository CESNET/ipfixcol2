#pragma once

#include <thread>
#include <stdexcept>
#include <functional>
#include <vector>

template <typename Runnable>
class ThreadRunner {
public:
    ThreadRunner(std::vector<Runnable> &runnables) :
        m_threads(runnables.size()) 
    {
        //printf("runnables size: %d\n", runnables.size());
        for (size_t i = 0; i < runnables.size(); i++) {
            ThreadState *ts = &m_threads[i];
            Runnable *runnable = &runnables[i];
            //printf("starting thread\n");
            ts->thread = std::thread([ts, runnable]() {
                try {
                    //fprintf(stderr, "thread start\n");
                    Runnable &r = *runnable;
                    r();
                    //fprintf(stderr, "thread done\n");
                    ts->done = true;

                } catch (...) {
                    //fprintf(stderr, "thread dead\n");
                    ts->exception = std::current_exception();
                }
            });
        }
    }

    bool
    poll()
    {
        //printf("poll start\n");
        bool done = true;
        
        for (const auto &ts : m_threads) {
            if (ts.exception) {
                std::rethrow_exception(ts.exception);
            }

            done &= ts.done;
        }
        //printf("poll end\n");

        return done;
    }

    void
    join()
    {
        for (auto &ts : m_threads) {
            ts.thread.join();
        }
    }

private:
    struct ThreadState {
        std::thread thread;
        bool done;
        std::exception_ptr exception;
    };

    std::vector<ThreadState> m_threads;
};

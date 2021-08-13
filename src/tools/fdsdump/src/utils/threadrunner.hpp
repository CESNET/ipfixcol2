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

                    //fprintf(stderr, "thread %ull finished", ts->thread.get_id());

                } catch (...) {
                    //fprintf(stderr, "caught exception in thread %ull\n", ts->thread.get_id());
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
        
        for (auto &ts : m_threads) {
            if (ts.exception) {
                std::rethrow_exception(ts.exception);
                ts.exception = nullptr;
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
            if (ts.exception) {
                std::rethrow_exception(ts.exception);
                ts.exception = nullptr;
            }

            if (!ts.thread.joinable()) {
                //fprintf(stderr, "thread %ull is not joinable\n", ts.thread.get_id());
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

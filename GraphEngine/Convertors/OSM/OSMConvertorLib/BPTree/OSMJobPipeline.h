#pragma once

// TJobPipeline: the jobs of one producing thread done by several threads, the results taken by one consuming thread in the
// order of the jobs (the resolving of the ways and of the relations).
//   - Submit (the producer): a job is queued; it waits while too many jobs are in flight (submitted, not taken);
//   - a thread takes a job, does it by its own work function (its state - the lookups, the compressor - in the closure,
//     made on the thread by fnMakeWork), keeps the result by the number of the job;
//   - Next (the consumer): the result of the next job in the order; the error of a thread is thrown there.

#include "OSMBPTreeUtils.h"

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <exception>
#include <functional>
#include <map>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

namespace GraphEngine {
    namespace Convertors {
        namespace bptree {

            template <class TJob, class TResult>
            class TJobPipeline
            {
            public:
                typedef std::function<void(TJob&, TResult&)> TWork;

                // fnMakeWork - called on every thread once: the work function of the thread
                TJobPipeline(int nThreads, std::function<TWork()> fnMakeWork) : m_fnMakeWork(fnMakeWork)
                {
                    m_nThreads = (std::max)(1, nThreads);
                    m_nMaxInFlight = (uint64_t)m_nThreads * 2 + 1;
                    for (int i = 0; i < m_nThreads; ++i)
                        m_threads.emplace_back([this]() { WorkerLoop(); });
                }

                ~TJobPipeline() { Stop(); }

                // the producer: the job is taken (moved); false - the pipeline stopped (an error / Stop)
                bool Submit(TJob& job)
                {
                    {
                        std::unique_lock<std::mutex> lock(m_mutex);
                        m_cvSubmit.wait(lock, [this]() { return m_bStop || m_error || m_nSubmitted - m_nTaken < m_nMaxInFlight; });
                        if (m_bStop || m_error)
                            return false;
                        m_jobs.emplace_back(m_nSubmitted++, std::move(job));
                    }
                    m_cvJobs.notify_one();
                    job = TJob();
                    return true;
                }

                // the producer: no more jobs
                void InputDone()
                {
                    {
                        std::lock_guard<std::mutex> lock(m_mutex);
                        m_bInputDone = true;
                    }
                    m_cvResults.notify_all();
                }

                // the consumer: the result of the next job, false - all the jobs are taken (or stopped)
                bool Next(TResult& result)
                {
                    {
                        std::unique_lock<std::mutex> lock(m_mutex);
                        auto ready = [this]() { return m_bStop || m_error || m_results.count(m_nTaken) != 0 || (m_bInputDone && m_nTaken == m_nSubmitted); };
                        if (!ready())
                        {
                            auto start = TClock::now();
                            m_cvResults.wait(lock, ready);
                            m_dWaitSeconds += Since(start);
                        }
                        if (m_error)
                            std::rethrow_exception(m_error);
                        if (m_bStop)
                            return false;
                        auto it = m_results.find(m_nTaken);
                        if (it == m_results.end())
                            return false;
                        result = std::move(it->second);
                        m_results.erase(it);
                        ++m_nTaken;
                    }
                    m_cvSubmit.notify_all();
                    return true;
                }

                // the threads stop and are joined
                void Stop()
                {
                    {
                        std::lock_guard<std::mutex> lock(m_mutex);
                        m_bStop = true;
                    }
                    m_cvJobs.notify_all();
                    m_cvSubmit.notify_all();
                    m_cvResults.notify_all();
                    for (std::thread& thread : m_threads)
                        if (thread.joinable())
                            thread.join();
                }

                int    Threads() const { return m_nThreads; }
                double WorkSeconds() const { return m_nWorkNs.load() / 1e9; }   // all the threads
                double WaitSeconds() const { return m_dWaitSeconds; }           // the consumer waited for the results

            private:
                TJobPipeline(const TJobPipeline&);
                TJobPipeline& operator=(const TJobPipeline&);

                void WorkerLoop()
                {
                    try
                    {
                        TWork fnWork = m_fnMakeWork();
                        for (;;)
                        {
                            std::pair<uint64_t, TJob> job;
                            {
                                std::unique_lock<std::mutex> lock(m_mutex);
                                m_cvJobs.wait(lock, [this]() { return m_bStop || !m_jobs.empty(); });
                                if (m_bStop)
                                    return;
                                job = std::move(m_jobs.front());
                                m_jobs.pop_front();
                            }

                            auto start = TClock::now();
                            TResult result;
                            fnWork(job.second, result);
                            m_nWorkNs += NanosecondsSince(start);

                            {
                                std::lock_guard<std::mutex> lock(m_mutex);
                                m_results.emplace(job.first, std::move(result));
                            }
                            m_cvResults.notify_all();
                        }
                    }
                    catch (...)
                    {
                        {
                            std::lock_guard<std::mutex> lock(m_mutex);
                            if (!m_error)
                                m_error = std::current_exception();
                        }
                        m_cvResults.notify_all();
                        m_cvSubmit.notify_all();
                    }
                }

            private:
                std::function<TWork()> m_fnMakeWork;
                int      m_nThreads = 1;
                uint64_t m_nMaxInFlight = 3;

                std::vector<std::thread> m_threads;
                std::mutex m_mutex;
                std::condition_variable m_cvSubmit;    // a result is taken / stop
                std::condition_variable m_cvJobs;      // a job is submitted / stop
                std::condition_variable m_cvResults;   // a job is done / the input is done / stop
                std::deque<std::pair<uint64_t, TJob> > m_jobs;
                std::map<uint64_t, TResult> m_results;
                uint64_t m_nSubmitted = 0;
                uint64_t m_nTaken = 0;
                bool m_bInputDone = false;
                bool m_bStop = false;
                std::exception_ptr m_error;

                std::atomic<uint64_t> m_nWorkNs{0};
                double m_dWaitSeconds = 0.;
            };
        }
    }
}

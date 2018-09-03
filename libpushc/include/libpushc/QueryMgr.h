// Copyright 2018 Erik Götzfried
// Licensed under the Apache License, Version 2.0( the "License" );
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once
#include "libpushc/Base.h"
#include "libpushc/Context.h"
#include "libpushc/Job.h"
#include "libpushc/Worker.h"

// Manages compilation queries, jobs and workers.
class QueryMgr : public std::enable_shared_from_this<QueryMgr> {
    // Current state and settings
    std::shared_ptr<Context> context;
    // Handles access to open_jobs, no_jobs from multiple threads
    Mutex job_mtx;
    // All jobs which have to be executed
    std::stack<std::shared_ptr<BasicJob>> open_jobs;
    // Stores a list of all worker threads including the main thread
    std::list<std::shared_ptr<Worker>> worker;

    // Is true if no free jobs exist. Helps to wake up threads when new jobs occur.
    bool no_jobs = false;

    size_t job_ctr = 0; // used to give every job a new id

    template <typename FuncT, typename... Args>
    auto query_impl( FuncT fn, bool volatile_query, const Args &... args ) -> decltype( auto );

public:
    ~QueryMgr() { wait_finished(); }

    // Initialize the query manager and the whole compiler infrastructure and return the main worker. \param
    // thread_count is the total amount of workers (including this thread).
    std::shared_ptr<Worker> setup( size_t thread_count );

    // Creates a new query with the function of \param fn
    // \param args defines the argument provided for the query implementation. The first job from the query is
    // reserved for the calling worker and is thus not in the open_jobs list.
    template <typename FuncT, typename... Args>
    auto query( FuncT fn, const Args &... args ) -> decltype( auto ) {
        return query_impl( fn, false, args... );
    }
    // The same as query() but ensures that a query gets updated/checked even when the result for this input was
    // calculated earlier and cached.
    template <typename FuncT, typename... Args>
    auto query_volatile( FuncT fn, const Args &... args ) -> decltype( auto ) {
        return query_impl( fn, true, args... );
    }

    // Waits until all workers have finished. Call this method only from the main thread.
    void wait_finished();

    // Returns a free job or nullptr if no free job exist.
    // The returned job will always have the "free" status.
    // NOTE: nullptr is returned if no free jobs where found.
    std::shared_ptr<BasicJob> get_free_job();
};

#include "libpushc/QueryMgr.inl"

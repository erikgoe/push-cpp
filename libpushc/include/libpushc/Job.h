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

// Enables polymorphism over class Job
class BasicJob {
public:
    virtual ~BasicJob() {}
    virtual void run() = 0;
    
    std::atomic_int status; // 0=free, 1=reserved, 2=executing, 3=finished
};

// Stores a function which has to be executed to fulfill a query.
template <typename R>
class Job : BasicJob {
    std::packaged_task<R()> task; // the function which should be executed for this job

public:
    Job( std::function<R()> function ) {
        task = std::packaged_task<R()>( function );
        result = task.get_future();
    }
    // executes the job
    void run() {
        status = 2;
        task();
        status = 3;
    }

    std::future<R> result; // the job stores the result in this variable
};

// Stores the jobs for a specific query
class JobCollection {
    std::shared_ptr<QueryMgr> query_mgr; // internally needed for exectue()

public:
    // a list of jobs for the query. The first job in the list is reserved by default (see QueryMgr::query).
    std::list<std::shared_ptr<BasicJob>> jobs;

    // returns true if all jobs are done TODO impl
    bool is_finished();

    // blocks until all jobs are done TODO impl
    void wait_finished();

    // Work on open jobs until finished. Other workers may already handle jobs for the query
    // If no free jobs remain (expect the first, which may be reserved), is_finished() will return false.
    // if prevent_idle is true other jobs from the QueryMgr are executed when there are no free jobs left.
    // Returns a reference to itself. TODO impl
    JobCollection &execute( bool execut_reserved_first = true, bool prevent_idle = true );

    friend class QueryMgr;
};

// This class is used to build a list of jobs
class JobsBuilder {
    std::list<std::shared_ptr<BasicJob>> jobs;

public:
    // Add a new job body WITH a return value
    template <typename R>
    JobsBuilder &add_job( std::function<R()> fn ) {
        jobs.push_back( std::make_shared<Job<R>>( fn ) );
        return *this;
    }

    friend class QueryMgr;
};

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

template <typename R>
class Job;

// Enables polymorphism over class Job
class BasicJob {
public:
    virtual ~BasicJob() {}
    virtual bool run( Worker &w_ctx ) = 0;

    static const int STATUS_FREE = 0;
    static const int STATUS_EXE = 1;
    static const int STATUS_FIN = 2;
    std::atomic_int status; // 0=free, 1=executing, 2=finished
    size_t id; // job id

    BasicJob() { status = STATUS_FREE; }
    BasicJob( const BasicJob &other ) { this->status.store( other.status.load() ); }

    // Cast into any Job
    template <typename T>
    Job<T> &as() {
        return *static_cast<Job<T> *>( this );
    }
};

// Stores a function which has to be executed to fulfill a query.
template <typename R>
class Job : public BasicJob {
    std::shared_ptr<std::packaged_task<R( Worker &w_ctx )>> task; // the function which should be executed for this job
    std::shared_future<R> result; // the job stores the result in this variable

public:
    Job( std::function<R( Worker &w_ctx )> function ) {
        task = std::make_shared<std::packaged_task<R( Worker &w_ctx )>>( function );
        result = task->get_future().share();
    }

    // executes the job. Returns true if was executed
    bool run( Worker &w_ctx ) {
        int test_val = BasicJob::STATUS_FREE;
        if ( status.compare_exchange_strong( test_val, BasicJob::STATUS_EXE ) ) {
            ( *task )( w_ctx );
            status = BasicJob::STATUS_FIN;
            return true;
        } else
            return false;
    }

    const R get() { return result.get(); }
};

// Stores the jobs for a specific query
class JobCollection : public std::enable_shared_from_this<JobCollection> {
    std::shared_ptr<QueryMgr> query_mgr; // internally needed for exectue()

public:
    // a list of jobs for the query. The first job in the list is reserved by default (see QueryMgr::query).
    std::list<std::shared_ptr<BasicJob>> jobs;

    // returns true if all jobs are done
    bool is_finished();

    // Work on open jobs until finished. Other workers may already handle jobs for the query
    // If no free jobs remain (expect the first, which may be reserved), is_finished() will return false.
    // If \param prevent_idle is true other jobs from the QueryMgr are executed when there are no free jobs left. If
    // there are no free jobs left, the function will return. Returns a reference to itself.
    std::shared_ptr<JobCollection> execute( Worker &w_ctx, bool prevent_idle = true );

    friend class QueryMgr;
};

// This class is used to build a list of jobs
class JobsBuilder {
    std::list<std::shared_ptr<BasicJob>> jobs;

public:
    // Add a new job body with a return value
    template <typename R>
    JobsBuilder &add_job( std::function<R( Worker &w_ctx )> fn ) {
        jobs.push_back( std::static_pointer_cast<BasicJob>( std::make_shared<Job<R>>( fn ) ) );
        return *this;
    }
    friend class QueryMgr;
};

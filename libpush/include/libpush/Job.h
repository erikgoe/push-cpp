// Copyright 2019 Erik Götzfried
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
#include "libpush/Base.h"
#include "libpush/util/String.h"
#include "libpush/util/AnyResultWrapper.h"
#include "libpush/util/FunctionHash.h"
#include "libpush/Message.h"

template <typename R>
class Job;

// Enables polymorphism over class Job
class BasicJob {
public:
    virtual ~BasicJob() {}
    virtual bool run( Worker &w_ctx ) = 0;

    constexpr static int STATUS_FREE = 0;
    constexpr static int STATUS_EXE = 1;
    constexpr static int STATUS_FIN = 2;
    std::atomic_int status; // 0=free, 1=executing, 2=finished
    size_t id; // job id
    sptr<FunctionSignature> query_sig; // required to create sub-queries
    sptr<UnitCtx> ctx; // local unit context

    BasicJob() { status = STATUS_FREE; }
    BasicJob( const BasicJob &other ) {
        this->status.store( other.status.load() );
        id = other.id;
        query_sig = other.query_sig;
    }

    // Cast into any Jobs' result
    template <typename T>
    auto to() -> const T {
        return static_cast<Job<T> *>( this )->get();
    }

    // Cast into any Job
    template <typename T>
    Job<T> &as_job() {
        return *static_cast<Job<T> *>( this );
    }
};

// Stores a function which has to be executed to fulfill a query.
template <typename R>
class Job : public BasicJob {
    sptr<std::packaged_task<R( Worker &w_ctx )>> task; // the function which should be executed for this job
    std::shared_future<R> result; // the job stores the result in this variable

public:
    Job( std::function<R( Worker &w_ctx )> function ) {
        task = make_shared<std::packaged_task<R( Worker & w_ctx )>>( function );
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

    // Returns the result of the job execution
    const R get() { return result.get(); }
};

template <typename T>
class JobCollection;

// Base class for polymorphism
class BasicJobCollection : public std::enable_shared_from_this<BasicJobCollection> {
public:
    virtual ~BasicJobCollection() {}

    // Cast this object to a specific JobCollection
    template <typename T>
    JobCollection<T> &as_jc() {
        return *static_cast<JobCollection<T> *>( this );
    }
    // Cast this object to a specific JobCollection-shared_ptr
    template <typename T>
    sptr<JobCollection<T>> as_jc_ptr() {
        return std::static_pointer_cast<JobCollection<T>>( shared_from_this() );
    }
};

// Stores the jobs for a specific query
template <typename T>
class JobCollection : public BasicJobCollection {
    sptr<GlobalCtx> g_ctx; // internally needed for exectue()
    AnyResultWrapper<T> result; // stores the result of the query (not a job)
    FunctionSignature fn_sig; // required to callback g_ctx when jobs finished

public:
    // a list of jobs for the query. The first job in the list is reserved by default (see GlobalCtx::query).
    std::list<sptr<BasicJob>> jobs;

    // returns true if all jobs are done. You must use this method to enable query caching
    bool is_finished();

    // waits until all jobs have been finished without busy waiting
    sptr<JobCollection<T>> wait();

    // Work on open jobs until finished. Other workers may already handle jobs for the query
    // If no free jobs remain (expect the first, which may be reserved), is_finished() will return false.
    // If \param prevent_idle is true other jobs from the GlobalCtx are executed when there are no free jobs left. If
    // there are no free jobs left, the function will return. Returns a reference to itself.
    sptr<JobCollection<T>> execute( Worker &w_ctx, bool prevent_idle = true );

    // Returns the result of the query. Not from a job!
    decltype( result.get() ) get() { return result.get(); }

    friend class GlobalCtx;
};

// This class is used to build a list of jobs
class JobsBuilder {
    std::list<sptr<BasicJob>> jobs;
    sptr<FunctionSignature> query_sig;
    sptr<UnitCtx> ctx;

public:
    JobsBuilder( const sptr<FunctionSignature> &query_sig, sptr<UnitCtx> &ctx ) {
        this->query_sig = query_sig;
        this->ctx = ctx;
    }

    // Add a new job body with a return value
    template <typename R>
    JobsBuilder &add_job( std::function<R( Worker &w_ctx )> fn ) {
        jobs.push_back( std::static_pointer_cast<BasicJob>( make_shared<Job<R>>( fn ) ) );
        jobs.back()->query_sig = query_sig;
        jobs.back()->ctx = ctx;
        return *this;
    }

    // Switch the context for all following jobs. Already created jobs will have the old context. This will not change
    // the query signature!
    void switch_context( sptr<UnitCtx> &new_ctx ) { ctx = new_ctx; }
    
    friend class GlobalCtx;
};

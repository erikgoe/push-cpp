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
#include "libpush/Job.h"
#include "libpush/Worker.h"
#include "libpush/Preferences.h"
#include "libpush/util/FunctionHash.h"

// Stores meta information about a query
struct QueryCacheHead {
    constexpr static u8 STATE_UNDECIDED = 0b000; // Set after unserialization
    constexpr static u8 STATE_RED = 0b001; // the cached value may be invalid
    constexpr static u8 STATE_VOLATILE_RED = 0b011; // the cached value is invalid
    constexpr static u8 STATE_GREEN = 0b101; // the cached value is valid
    constexpr static u8 STATE_VOLATILE_GREEN =
        0b111; // the cached value is valid but must be recalculated the next incremental build

    FunctionSignature func; // signature of the query
    std::shared_ptr<BasicJobCollection> jc; // cached data
    u8 state = STATE_RED; // current state of the query
    u32 complexity = 0; // TODO
    std::list<std::shared_ptr<QueryCacheHead>> sub_dag; // queries which are called in this query

    QueryCacheHead( const FunctionSignature &func, const std::shared_ptr<BasicJobCollection> &jc ) {
        this->func = func;
        this->jc = jc;
    }
};

// Manages compilation queries, jobs, workers and settings.
class GlobalCtx : public std::enable_shared_from_this<GlobalCtx> {
    // Stores a list of all worker threads including the main thread
    std::list<std::shared_ptr<Worker>> worker;


    // Handles access to open_jobs, no_jobs, jobs_cv from multiple threads
    Mutex job_mtx;
    // All jobs which have to be executed
    std::stack<std::shared_ptr<BasicJob>> open_jobs;
    // Is true if no free jobs exist. Helps to wake up threads when new jobs occur.
    bool no_jobs = false;
    // Enables waiting for jobs
    ConditionVariable jobs_cv;
    // Is set to true in abort_compilation() and to false in reset(). Prevents new jobs from being created
    std::atomic_bool abort_new_jobs;
    size_t job_ctr = 0; // used to give every job a new id


    Mutex query_cache_mtx;
    // Enables caching of queries
    std::unordered_map<FunctionSignature, std::shared_ptr<QueryCacheHead>> query_cache;


    Mutex pref_mtx; // used for async preference access
    std::map<PrefType, std::unique_ptr<PrefValue>> prefs; // store all the prefs


    template <typename FuncT, typename... Args>
    auto query_impl( FuncT fn, std::shared_ptr<Worker> w_ctx, const Args &... args ) -> decltype( auto );

public:
    // Public data
    std::atomic_size_t error_count;
    std::atomic_size_t warning_count;
    std::atomic_size_t notification_count;
    std::atomic_size_t max_allowed_errors;
    std::atomic_size_t max_allowed_warnings;
    std::atomic_size_t max_allowed_notifications;


    ~GlobalCtx() { wait_finished(); }

    // Initialize the global context and the whole compiler infrastructure and return the main worker. \param
    // thread_count is the total amount of workers (including this thread).
    std::shared_ptr<Worker> setup( size_t thread_count, size_t cache_map_reserve = 256 );

    // In incremental build this method should be called before a new run
    void reset() {
        abort_new_jobs = false;

        Lock lock( query_cache_mtx );
        for ( auto &qc : query_cache ) {
            if ( qc.second->state == QueryCacheHead::STATE_GREEN ) {
                qc.second->state = QueryCacheHead::STATE_UNDECIDED;
            } else if ( qc.second->state & 0b010 ) { // Volatile
                qc.second->state = QueryCacheHead::STATE_VOLATILE_RED;
            }
        }
    }

    // Creates a new query with the function of \param fn.
    // \param args defines the argument provided for the query implementation. The first job from the query is
    // reserved for the calling worker and is thus not in the open_jobs list.
    template <typename FuncT, typename... Args>
    auto query( FuncT fn, Worker &w_ctx, const Args &... args ) -> decltype( auto );

    // Creates a new query with the function of \param fn.
    // \param args defines the argument provided for the query implementation. The first job from the query is
    // reserved for the calling worker and is thus not in the open_jobs list.
    template <typename FuncT, typename... Args>
    auto query( FuncT fn, std::shared_ptr<Worker> w_ctx, const Args &... args ) -> decltype( auto );

    // Returns the root unit context. Use it only to create new build queries.
    std::shared_ptr<UnitCtx> get_global_unit_ctx();

    // Waits until all workers have finished. Call this method only from the main thread.
    void wait_finished();

    // Returns a free job or nullptr if no free job exist.
    // The returned job will always have the "free" status.
    // NOTE: nullptr is returned if no free jobs where found.
    std::shared_ptr<BasicJob> get_free_job();

    // Cancel all waiting jobs and abort compilation (AbortCompilationError is thrown)
    void abort_compilation();

    // Returns if execution of jobs is allowed (only used internally)
    bool jobs_allowed() { return !abort_new_jobs; }

    // A job calls this method when he finishes
    void finish_job( const FunctionSignature &fn_sig ) {
        Lock lock( query_cache_mtx );
        query_cache[fn_sig]->state |= 0b101; // set green
    }

    // This method is used internally by the Worker class
    void set_volatile_job( const FunctionSignature &fn_sig ) {
        Lock lock( query_cache_mtx );
        query_cache[fn_sig]->state |= 0b011; // set volatile
    }

    // Waits with the jov_cv until all jobs in a JobCollection have been finished
    template <typename T>
    void wait_job_collection_finished( JobCollection<T> &jc ) {
        UniqueLock lk( job_mtx );
        jobs_cv.wait( lk, [&jc, this] {
            if ( abort_new_jobs ) {
                throw AbortCompilationError();
            }
            return jc.is_finished();
        } );
    }

    // Prints a message to the user
    template <MessageType MesT, typename... Args>
    void print_msg( std::shared_ptr<Worker> w_ctx, const MessageInfo &message,
                              const std::vector<MessageInfo> &notes, Args... head_args ) {
        print_msg_to_stdout( get_message<MesT>( w_ctx, message, notes, head_args... ) );
        if ( MesT < MessageType::error ) // fatal error
            throw AbortCompilationError();
    }


    // Returns a previously saved pref. If it was not saved, returns the default value for the preference type.
    template <typename ValT>
    auto get_pref( const PrefType &pref_type ) -> const decltype( ValT::value ) {
        Lock lock( pref_mtx );
        if ( prefs.find( pref_type ) == prefs.end() ) {
            prefs[pref_type] = std::make_unique<ValT>();
            LOG_WARN( "Using preference (" + to_string( static_cast<u32>( pref_type ) ) +
                      ") which was not set before." );
        }
        return prefs[pref_type]->get<ValT>().value;
    }
    // Returns a pref value or if it doesn't exist, saves \param default_value for the pref and returns it.
    template <typename ValT>
    auto get_pref_or_set( const PrefType &pref_type, const decltype( ValT::value ) &default_value ) -> const
        decltype( ValT::value ) {
        Lock lock( pref_mtx );
        if ( prefs.find( pref_type ) == prefs.end() ) {
            prefs[pref_type] = std::make_unique<ValT>( default_value );
            return default_value;
        }
        return prefs[pref_type]->get<ValT>().value;
    }
    // Stores a new pref or overwrites an existing one.
    template <typename PrefT, typename ValT>
    void set_pref( const PrefType &pref_type, const ValT &value ) {
        Lock lock( pref_mtx );
        prefs[pref_type] = std::make_unique<PrefT>( value );
    }

    // This method must be called to update some specific prefs like tab length
    void update_global_prefs();

    // Returns the name of the triplet element corresponding to the value. Returns and empty string if this value does
    // not exist
    static String get_triplet_elem_name( const String &value );
    // Returns the index-position of a triplet name
    static size_t get_triplet_pos( const String &name );
};

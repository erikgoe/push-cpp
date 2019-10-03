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

// Returns true if the query or its sub-queries must be re-run
bool requires_run( QueryCacheHead &head );

template <typename> struct ReturnType;

template <typename R, typename... Args>
struct ReturnType<R(*)(Args...)> {
   using type = R;
};

template <typename T>
using return_t = typename ReturnType<T>::type;


template <typename FuncT, typename... Args>
auto GlobalCtx::query_impl( FuncT fn, std::shared_ptr<Worker> w_ctx, const Args &... args ) -> decltype( auto ) {
    std::shared_ptr<UnitCtx> ctx;
    if ( w_ctx && w_ctx->curr_job )
        ctx = w_ctx->curr_job->ctx;
    else
        ctx = get_global_unit_ctx();

    auto jc =
        std::make_shared<JobCollection<return_t<decltype(fn)>>>();

    auto fn_sig = FunctionSignature::create( fn, *ctx, args... );
    {
        Lock lock( query_cache_mtx );
        std::shared_ptr<QueryCacheHead> head;
        if ( query_cache.find( fn_sig ) != query_cache.end() ) { // found cached
            head = query_cache[fn_sig];
            if ( !requires_run( *head ) ) { // cached state is valid
                LOG( "Using cached query result." );
                return head->jc
                    ->as_jc_ptr<return_t<decltype(fn)>>();
            } else { // exists but must be updated
                LOG( "Update cached query result." );
                jc =
                    head->jc->as_jc_ptr<return_t<decltype(fn)>>();
            }
        } else { // create new cache entry
            head = std::make_shared<QueryCacheHead>( fn_sig, std::static_pointer_cast<BasicJobCollection>( jc ) );
            query_cache[fn_sig] = head;
        }

        jc->fn_sig = fn_sig;

        // Update dag
        if ( w_ctx && w_ctx->curr_job ) { // if nullptr, there is no parent job TODO: test whether w_ctx is ever zero
            if ( query_cache.find( *w_ctx->curr_job->query_sig ) == query_cache.end() ) {
                LOG_ERR( "Parent query was not found in query_cache" );
            } else if ( std::find( head->sub_dag.begin(), head->sub_dag.end(),
                                   query_cache[*w_ctx->curr_job->query_sig] ) == head->sub_dag.end() ) {
                query_cache[*w_ctx->curr_job->query_sig]->sub_dag.push_back( head );
            }
        }
    }

    JobsBuilder jb( std::make_shared<FunctionSignature>( fn_sig ), ctx );

    if ( abort_new_jobs ) // Abort because some other thread has stopped execution
        throw AbortCompilationError();

    jc->result.wrap( fn, args..., jb, *ctx );

    jc->jobs = jb.jobs;
    jc->g_ctx = shared_from_this();
    if ( !jb.jobs.empty() ) // The first job will be skiped below, so set the id here
        jb.jobs.front()->id = job_ctr++;
    else { // no jobs have been created. Query finished
        Lock lock( query_cache_mtx );
        query_cache[fn_sig]->state |= 0b101; // set green
    }

    if ( jb.jobs.size() > 0 ) {
        {
            Lock lock( job_mtx );
            if ( jb.jobs.size() > 1 ) { // add all jobs (but the first) into the open_jobs list
                for ( auto itr = ++jb.jobs.begin(); itr != jb.jobs.end(); itr++ ) {
                    open_jobs.push( *itr );
                    ( *itr )->id = job_ctr++;
                }
            }
        }

        if ( no_jobs ) { // wake threads
            no_jobs = false;
            for ( auto &w : worker ) {
                w->notify();
            }
        }
    }

    return jc;
}

template <typename FuncT, typename... Args>
auto GlobalCtx::query( FuncT fn, Worker &w_ctx, const Args &... args ) -> decltype( auto )
{
    return query_impl( fn, w_ctx.shared_from_this(), args... );
}
template <typename FuncT, typename... Args>
auto GlobalCtx::query( FuncT fn, std::shared_ptr<Worker> w_ctx, const Args &... args ) -> decltype( auto ) {
    return query_impl( fn, w_ctx, args... );
}

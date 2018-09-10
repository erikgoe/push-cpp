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

#include "libpushc/stdafx.h"
#include "libpushc/QueryMgr.h"
#include "libpushc/Worker.h"
#include "libpushc/Message.h"

std::shared_ptr<Worker> QueryMgr::setup( size_t thread_count, size_t cache_map_reserve ) {
    if ( thread_count < 1 ) {
        LOG_ERR( "Must be at least one worker." );
    }

    // Context and prefs
    context = std::make_shared<Context>();
    set_default_preferences( context->prefs );

    // Query cache
    query_cache.reserve( cache_map_reserve );

    // Worker
    std::shared_ptr<Worker> main_worker = std::make_shared<Worker>( shared_from_this(), 0 );
    worker.push_back( main_worker );

    for ( size_t i = 1; i < thread_count; i++ ) {
        std::shared_ptr<Worker> w = std::make_shared<Worker>( shared_from_this(), i );
        w->work();
        worker.push_back( w );
    }

    reset();
    return main_worker;
}

void QueryMgr::wait_finished() {
    for ( auto &w : worker ) {
        w->stop();
    }
}

std::shared_ptr<BasicJob> QueryMgr::get_free_job() {
    std::shared_ptr<BasicJob> ret_job;

    Lock lock( job_mtx );
    while ( !open_jobs.empty() ) {
        if ( open_jobs.top()->status == BasicJob::STATUS_FREE ) { // found free job
            ret_job = open_jobs.top();
            open_jobs.pop();
            break;
        } else if ( open_jobs.top()->status == BasicJob::STATUS_EXE ) { // found a executing job => remove
            LOG_WARN( "Found executing job(" + to_string( open_jobs.top()->id ) + ") in open_jobs stack." );
            open_jobs.pop();
        } else if ( open_jobs.top()->status == BasicJob::STATUS_FIN ) { // found a finished job => delete
            LOG_WARN( "Found finished job(" + to_string( open_jobs.top()->id ) + ") in open_jobs stack." );
            open_jobs.pop();
        }
    }

    jobs_cv.notify_all(); // another job was propably finished before, so notify waiting threads

    if ( !ret_job )
        no_jobs = true;
    return ret_job;
}

void QueryMgr::abort_compilation() {
    Lock lock( job_mtx );
    if ( !open_jobs.empty() )
        open_jobs.pop();
    abort_new_jobs = true;
}

bool requires_run( QueryCacheHead &head ) {
    if ( head.state >= QueryCacheHead::STATE_GREEN ) {
        return false;
    } else if ( head.state >= QueryCacheHead::STATE_RED ) {
        return true;
    } else { // Undecided
        for ( auto &sub : head.sub_dag ) {
            if ( requires_run( *sub ) ) {
                head.state &= 0b011; // set red
                return true;
            }
        }
        return false;
    }
}

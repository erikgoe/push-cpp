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

std::shared_ptr<Worker> QueryMgr::setup( size_t thread_count ) {
    if ( thread_count < 1 ) {
        LOG_ERR( "Must be at least one worker." );
    }

    std::shared_ptr<Worker> main_worker = std::make_shared<Worker>();
    worker.push_back( main_worker );

    for ( size_t i = 1; i < thread_count; i++ ) {
        std::shared_ptr<Worker> w = std::make_shared<Worker>();
        w->work( shared_from_this() );
        worker.push_back( w );
    }

    return main_worker;
}

void QueryMgr::wait_finished() {
    for ( auto w : worker ) {
        w->stop();
    }
}

std::shared_ptr<BasicJob> QueryMgr::get_free_job() {
    std::shared_ptr<BasicJob> ret_job;

    Lock lock( job_mtx );
    while ( !open_jobs.empty() ) {
        int test_val = 0;
        if ( open_jobs.top()->status.compare_exchange_strong( test_val, 2 ) ) { // found free job
            ret_job = open_jobs.top();
            open_jobs.pop();
            break;
        } else if ( open_jobs.top()->status == 1 ) { // found a reserved job => move into reserved_jobs
            reserved_jobs.push( open_jobs.top() );
            open_jobs.pop();
            LOG_WARN( "Found reserved job in open_jobs stack." );
        } else if ( open_jobs.top()->status == 2 ) { // found a executing job => move into running_jobs
            running_jobs.push_back( open_jobs.top() );
            open_jobs.pop();
            LOG_WARN( "Found executing job in open_jobs stack." );
        } else if ( open_jobs.top()->status == 3 ) { // found a finished job => delete
            open_jobs.pop();
        }
    }
    if ( !ret_job ) { // nothing found in open_jobs
        while ( !reserved_jobs.empty() ) {
            int test_val = 1;
            if ( reserved_jobs.top()->status.compare_exchange_strong( test_val, 2 ) ) { // found reserved job
                ret_job = reserved_jobs.top();
                reserved_jobs.pop();
                break;
            } else if ( reserved_jobs.top()->status == 0 ) { // found a free job => move into open_jobs
                open_jobs.push( reserved_jobs.top() );
                reserved_jobs.pop();
                LOG_WARN( "Found free job in reserved_jobs stack." );
            } else if ( reserved_jobs.top()->status == 2 ) { // found a executing job => move into running_jobs
                running_jobs.push_back( reserved_jobs.top() );
                reserved_jobs.pop();
                LOG_WARN( "Found executing job in reserved_jobs stack." );
            } else if ( reserved_jobs.top()->status == 3 ) { // found a finished job => delete
                reserved_jobs.pop();
            }
        }
    }

    if ( ret_job )
        running_jobs.push_back( ret_job );
    return ret_job;
}

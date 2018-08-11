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

Worker::Worker( std::shared_ptr<QueryMgr> qm, size_t id ) {
    finish = false;
    this->qm = qm;
    this->id = id;
}

void Worker::work() {
    thread = std::make_unique<std::thread>( [this]() {
        std::shared_ptr<BasicJob> job = qm->get_free_job();
        while ( !finish ) {
            while ( job ) { // handle open jobs
                if ( job->run( *this ) )
                    LOG( "Thread " + to_string( id ) + " (extern) executed job(" + to_string( job->id ) + ")." );
                job = qm->get_free_job();
            }

            {
                LOG( "Thread " + to_string( id ) + " idle." );
                std::unique_lock<std::mutex> lk( mtx );
                cv.wait( lk, [this, &job] { return finish || ( job = qm->get_free_job() ); } );
                LOG( "Thread " + to_string( id ) + " continue." );
            }
        }
    } );
}
void Worker::stop() {
    if ( thread && thread->joinable() ) { // the main thread has not explicit thread object
        {
            std::lock_guard<std::mutex> lk( mtx );
            finish = true;
        }
        cv.notify_all();

        thread->join();
    }
}

void Worker::notify() {
    cv.notify_all();
}

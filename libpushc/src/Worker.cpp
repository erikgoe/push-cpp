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

void Worker::work( std::shared_ptr<QueryMgr> qm ) {
    this->qm = qm;

    thread = std::make_unique<std::thread>( [this, qm]() {
        while ( !finish ) {
            auto job = qm->get_free_job();
            if ( job )
                job->run();

            {
                std::unique_lock<std::mutex> lk( mtx );
                cv.wait( lk, [this, qm] { return finish || qm->get_free_job(); } );
            }
        }
    } );
}
void Worker::stop() {
    {
        std::lock_guard<std::mutex> lk( mtx );
        finish = true;
    }
    cv.notify_all();

    thread->join();
}

void Worker::notify() {
    cv.notify_all();
}

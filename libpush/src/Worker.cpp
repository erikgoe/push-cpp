// Copyright 2020 Erik Götzfried
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

#include "libpush/stdafx.h"
#include "libpush/GlobalCtx.h"
#include "libpush/Worker.h"
#include "libpush/Message.h"

Worker::Worker( sptr<GlobalCtx> g_ctx, size_t id ) {
    finish = false;
    this->g_ctx = g_ctx;
    this->id = id;
}

void Worker::work() {
    thread = std::make_unique<std::thread>( [this]() {
        curr_job = g_ctx->get_free_job();
        while ( !finish ) {
            while ( curr_job ) { // handle open jobs
                try {
                    curr_job->run( *this );
                }
#pragma warning( push )
#pragma warning( disable : 4101 )
                catch ( AbortCompilationError &err ) {
                    break; // Just abort compilation
                }
#pragma warning( pop )
                curr_job = g_ctx->get_free_job();
            }

            {
                UniqueLock lk( mtx );
                cv.wait( lk, [this] { return finish || ( curr_job = g_ctx->get_free_job() ); } );
            }
        }
    } );
}
void Worker::stop() {
    if ( thread && thread->joinable() ) { // the main thread has not explicit thread object
        {
            Lock lk( mtx );
            finish = true;
        }
        cv.notify_all();

        thread->join();
    }
}

void Worker::notify() {
    cv.notify_all();
}

void Worker::set_curr_job_volatile() {
    g_ctx->set_volatile_job( *curr_job->query_sig );
}

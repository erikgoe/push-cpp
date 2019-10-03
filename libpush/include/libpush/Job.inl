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

template <typename T>
bool JobCollection<T>::is_finished() {
    for ( auto &job : jobs ) {
        if ( job->status != BasicJob::STATUS_FIN )
            return false;
    }
    g_ctx->finish_job( fn_sig ); // is finished now
    return true;
}

template <typename T>
std::shared_ptr<JobCollection<T>> JobCollection<T>::wait() {
    g_ctx->wait_job_collection_finished( *this );
    return as_jc_ptr<T>();
}

template <typename T>
std::shared_ptr<JobCollection<T>> JobCollection<T>::execute( Worker &w_ctx, bool prevent_idle ) {
    // handle open jobs
    for ( auto &job : jobs ) {
        w_ctx.curr_job = job;
        job->run( w_ctx ); // the job itself will check if it's free
        if ( !g_ctx->jobs_allowed() )
            throw AbortCompilationError();
    }
    w_ctx.curr_job = nullptr;

    // prevent idle when jobs are still executed
    if ( prevent_idle ) {
        while ( !is_finished() ) {
            auto tmp_job = g_ctx->get_free_job();
            if ( tmp_job ) {
                w_ctx.curr_job = tmp_job;
                tmp_job->run( w_ctx );
                if ( !g_ctx->jobs_allowed() )
                    throw AbortCompilationError();
            } else
                break; // Return because there are no more free jobs
        }
        w_ctx.curr_job = nullptr;
    }

    return as_jc_ptr<T>();
}

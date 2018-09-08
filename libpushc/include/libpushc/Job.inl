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

template <typename T>
bool JobCollection<T>::is_finished() {
    for ( auto &job : jobs ) {
        if ( job->status != BasicJob::STATUS_FIN )
            return false;
    }
    return true;
}

template <typename T>
void JobCollection<T>::wait() {
    UniqueLock lk( query_mgr->job_mtx );
    query_mgr->jobs_cv.wait( lk, [this] {
        if ( query_mgr->abort_new_jobs ) {
            throw AbortCompilationError();
        }
        return is_finished();
    } );
}

template <typename T>
std::shared_ptr<JobCollection<T>> JobCollection<T>::execute( Worker &w_ctx, bool prevent_idle ) {
    // handle open jobs
    for ( auto &job : jobs ) {
        job->run( w_ctx ); // the job itself will check if it's free
        if ( !query_mgr->jobs_allowed() )
            throw AbortCompilationError();
    }

    // prevent idle when jobs are still executed
    if ( prevent_idle ) {
        while ( !is_finished() ) {
            auto tmp_job = query_mgr->get_free_job();
            if ( tmp_job ) {
                tmp_job->run( w_ctx );
                if ( !query_mgr->jobs_allowed() )
                    throw AbortCompilationError();
            } else
                break; // Return because there are no more free jobs
        }
    }

    return shared_from_this();
}

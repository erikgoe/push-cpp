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

template <typename FuncT, typename... Args>
auto QueryMgr::query_impl( FuncT fn, bool volatile_query, const Args &... args ) -> decltype( auto ) {
    // TODO impl volatile_query with caching of querries

    JobsBuilder jb;
    auto jc = std::make_shared<JobCollection<decltype( fn( args..., JobsBuilder(), QueryMgr() ) )>>();

    if ( abort_new_jobs ) // Abort because some other thread has stopped execution
        throw AbortCompilationError();

    jc->result.wrap( fn, args..., jb, *this );

    jc->jobs = jb.jobs;
    jc->query_mgr = shared_from_this();
    if ( !jb.jobs.empty() )
        jb.jobs.front()->id = job_ctr++;

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

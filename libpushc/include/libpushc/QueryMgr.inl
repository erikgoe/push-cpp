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
std::shared_ptr<JobCollection> QueryMgr::query_impl( std::function<FuncT> fn, bool volatile_query,
                                                     const Args &... args ) {
    // TODO impl volatile_query with caching of querries
    
    JobsBuilder jb;
    fn( args..., jb, *this );

    std::shared_ptr<JobCollection> jc;
    jc->jobs = jb.jobs;
    jc->query_mgr = shared_from_this();

    if ( jb.jobs.size() > 0 ) {
        jb.front()->status = 1;
        reserved_jobs.push( jb.front() );
        std::lock_guard( job_mtx );
        if( jb.jobs.size() > 1 ) {
            auto itr = ++jb.jobs.begin();
            open_jobs.push( *itr );
        }

        if( no_jobs ) { // wake threads
            no_jobs = false;
            for ( auto &w : worker ) {
                w->notify();
            }
        }
    }

    return jc;
}

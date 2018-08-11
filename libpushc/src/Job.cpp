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
#include "libpushc/Job.h"

bool JobCollection::is_finished() {
    for ( auto &job : jobs ) {
        if ( job->status != BasicJob::STATUS_FIN )
            return false;
    }
    return true;
}

std::shared_ptr<JobCollection> JobCollection::execute( Worker &w_ctx, bool prevent_idle ) {
    // handle open jobs
    for ( auto &job : jobs ) {
        if ( job->run( w_ctx ) ) // the job which check if he is free itself
            LOG( "Thread " + to_string( w_ctx.id ) + " (linear) executed job(" + to_string( job->id ) + ")." );
    }

    // prevent idle when jobs are still executed
    if ( prevent_idle ) {
        while ( !is_finished() ) {
            auto tmp_job = query_mgr->get_free_job();
            if ( tmp_job ) {
                if ( tmp_job->run( w_ctx ) )
                    LOG( "Thread " + to_string( w_ctx.id ) + " (prevent_idle) executed job(" +
                         to_string( tmp_job->id ) + ")." );
            } else
                break; // Return because there are no more free jobs
        }
    }

    return shared_from_this();
}

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
    for ( auto job : jobs ) {
        if ( job->status != 3 )
            return false;
    }
    return true;
}

JobCollection &JobCollection::execute( bool execute_reserved_first, bool prevent_idle ) {
    if ( execute_reserved_first ) { // first job is reserved
        jobs.front()->run();
    }
    // handle open jobs
    for ( auto job : jobs ) {
        int test_val = 0;
        if ( job->status.compare_exchange_strong( test_val, 2 ) ) {
            job->run();
        }
    }

    // prevent idle when jobs are still executed
    if ( prevent_idle ) {
        while ( !is_finished() ) {
            auto tmp_job = query_mgr->get_free_job();
            if ( tmp_job)
                tmp_job->run();
            else
                break; // Return because there are no more free jobs
        }
    }

    return *this;
}

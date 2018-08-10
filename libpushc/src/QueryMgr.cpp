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

std::shared_ptr<Worker> QueryMgr::setup( size_t thread_count ) {
    if ( thread_count < 1 ) {
        std::cerr << "error: must be at least one worker.";
    }
    // TODO
}

void QueryMgr::wait_finished() {
    // TODO
}

std::shared_ptr<BasicJob> get_free_job() {
    // TODO
}

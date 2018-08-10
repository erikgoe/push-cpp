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

#pragma once
#include "libpushc/Base.h"

// Executes a job on its thread
class Worker {
    std::unique_ptr<std::thread> thread;
    std::atomic_bool finish; // will stop if is set to true
    std::shared_ptr<QueryMgr> qm;

    std::mutex mtx;
    std::condition_variable cv;

public:
    // starts a new thread and executes free jobs from the QueryMgr
    void work( std::shared_ptr<QueryMgr> qm );

    // The thread will wait for new jobs until this method is called
    void stop();

    // notifies the thread when new jobs occur
    void notify();
};

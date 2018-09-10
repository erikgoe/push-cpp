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
#include "libpushc/util/String.h"
#include "libpushc/Message.h"
#include "libpushc/Job.h"

// Executes a job on its thread
class Worker : public std::enable_shared_from_this<Worker> {
    std::unique_ptr<std::thread> thread;
    std::atomic_bool finish; // will stop if is set to true
    std::shared_ptr<QueryMgr> qm;

    Mutex mtx;
    ConditionVariable cv;

public:
    // Context data
    size_t id; // id of this worker
    std::shared_ptr<BasicJob> curr_job;


    // Basic constructor
    Worker( std::shared_ptr<QueryMgr> qm, size_t id );

    // starts a new thread and executes free jobs from the QueryMgr
    void work();

    // The thread will wait for new jobs until this method is called. Blocks until the thread finished.
    void stop();

    // notifies the thread when new jobs occur
    void notify();

    // Returns the query manager used by this worker
    std::shared_ptr<QueryMgr> get_query_mgr() { return qm; }

    // Call this method in a job which does access volatile resources
    void set_curr_job_volatile();

    // Prints a message to the user
    template <MessageType MesT, typename... Args>
    constexpr void print_msg( const MessageInfo &message, const std::vector<MessageInfo> &notes, Args... head_args ) {
        qm->print_msg<MesT>( shared_from_this(), message, notes, head_args... );
    }
};

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
#include "libpush/Base.h"
#include "libpush/util/String.h"
#include "libpush/Message.h"
#include "libpush/Job.h"

// Executes a job on its thread
class Worker : public std::enable_shared_from_this<Worker> {
    std::unique_ptr<std::thread> thread;
    std::atomic_bool finish; // will stop if is set to true
    sptr<GlobalCtx> g_ctx;

    Mutex mtx;
    ConditionVariable cv;

public:
    // Context data
    size_t id; // id of this worker
    sptr<BasicJob> curr_job;


    // Basic constructor
    Worker( sptr<GlobalCtx> g_ctx, size_t id );

    // starts a new thread and executes free jobs from the GlobalCtx
    void work();

    // The thread will wait for new jobs until this method is called. Blocks until the thread finished.
    void stop();

    // notifies the thread when new jobs occur
    void notify();

    // Creates a new query (see GlobalCtx::query())
    template <typename FuncT, typename... Args>
    auto query( FuncT fn, const Args &... args ) -> decltype( auto );

    // Shortcut for query()->execute()->wait()
    template <typename FuncT, typename... Args>
    auto do_query( FuncT fn, const Args &... args ) -> decltype( auto );

    // Returns the global context used by this worker
    sptr<GlobalCtx> global_ctx() { return g_ctx; }

    // Returns the unit context for the current job
    sptr<UnitCtx> unit_ctx() { return curr_job->ctx; }

    // Call this method in a job which does access volatile resources
    void set_curr_job_volatile();

    // Prints a message to the user
    template <MessageType MesT, typename... Args>
    constexpr void print_msg( const MessageInfo &message,
                              const std::vector<MessageInfo> &notes = std::vector<MessageInfo>(), Args... head_args );
};

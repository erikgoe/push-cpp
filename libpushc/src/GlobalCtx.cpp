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
#include "libpushc/GlobalCtx.h"
#include "libpushc/Worker.h"
#include "libpushc/Message.h"
#include "libpushc/UnitCtx.h"

std::shared_ptr<Worker> GlobalCtx::setup( size_t thread_count, size_t cache_map_reserve ) {
    if ( thread_count < 1 ) {
        LOG_ERR( "Must be at least one worker." );
    }
    update_global_prefs();
    error_count = 0;
    warning_count = 0;
    notification_count = 0;

    // Preferences
    set_default_preferences( prefs );

    // Query cache
    query_cache.reserve( cache_map_reserve );

    // Worker
    std::shared_ptr<Worker> main_worker = std::make_shared<Worker>( shared_from_this(), 0 );
    worker.push_back( main_worker );

    for ( size_t i = 1; i < thread_count; i++ ) {
        std::shared_ptr<Worker> w = std::make_shared<Worker>( shared_from_this(), i );
        w->work();
        worker.push_back( w );
    }

    reset();
    return main_worker;
}

std::shared_ptr<UnitCtx> GlobalCtx::get_global_unit_ctx() {
    return std::make_shared<UnitCtx>( std::make_shared<String>( "" ) );
}

void GlobalCtx::wait_finished() {
    for ( auto &w : worker ) {
        w->stop();
    }
}

std::shared_ptr<BasicJob> GlobalCtx::get_free_job() {
    std::shared_ptr<BasicJob> ret_job;

    Lock lock( job_mtx );
    while ( !open_jobs.empty() ) {
        if ( open_jobs.top()->status == BasicJob::STATUS_FREE ) { // found free job
            ret_job = open_jobs.top();
            open_jobs.pop();
            break;
        } else if ( open_jobs.top()->status == BasicJob::STATUS_EXE ) { // found a executing job => remove
            LOG_WARN( "Found executing job(" + to_string( open_jobs.top()->id ) + ") in open_jobs stack." );
            open_jobs.pop();
        } else if ( open_jobs.top()->status == BasicJob::STATUS_FIN ) { // found a finished job => delete
            LOG_WARN( "Found finished job(" + to_string( open_jobs.top()->id ) + ") in open_jobs stack." );
            open_jobs.pop();
        }
    }

    jobs_cv.notify_all(); // another job was propably finished before, so notify waiting threads

    if ( !ret_job )
        no_jobs = true;
    return ret_job;
}

void GlobalCtx::abort_compilation() {
    Lock lock( job_mtx );
    if ( !open_jobs.empty() )
        open_jobs.pop();
    abort_new_jobs = true;
}

bool requires_run( QueryCacheHead &head ) {
    if ( head.state >= QueryCacheHead::STATE_GREEN ) {
        return false;
    } else if ( head.state >= QueryCacheHead::STATE_RED ) {
        return true;
    } else { // Undecided
        for ( auto &sub : head.sub_dag ) {
            if ( requires_run( *sub ) ) {
                head.state &= 0b011; // set red
                return true;
            }
        }
        return false;
    }
}

void GlobalCtx::update_global_prefs() {
    String::TAB_WIDTH = get_pref_or_set<SizeSV>( PrefType::tab_size, 4 );
    max_allowed_errors = get_pref_or_set<SizeSV>( PrefType::max_errors, 256 );
    max_allowed_warnings = get_pref_or_set<SizeSV>( PrefType::max_warnings, 256 );
    max_allowed_notifications = get_pref_or_set<SizeSV>( PrefType::max_notifications, 256 );
}

String GlobalCtx::get_triplet_elem_name( const String &value ) {
    if ( value == "x86" || value == "x86_64" || value == "arm" || value == "mips" || value == "8051" ||
         value == "avr" || value == "aarch64" || value == "powerpc" ) {
        return "arch";
    } else if ( value == "windows" || value == "linux" || value == "darwin" || value == "bsd" || value == "fuchsia" ||
                value == "webasm" || value == "dos" ) {
        return "os";
    } else if ( value == "pc" || value == "android" || value == "ios" || value == "macos" ) {
        return "plattform";
    } else if ( value == "pe" || value == "elf" || value == "macho" ) {
        return "format";
    } else if ( value == "llvm" || value == "gcc" || value == "msvc" || value == "pushbnd" || value == "ctrans" ) {
        return "backend";
    } else if ( value == "glibc" || value == "musl" || value == "msvcrt" ) {
        return "runtime";
    } else if ( value == "static" || value == "dynamic" ) {
        return "linkage";
    } else if ( value == "debug" || value == "release" || value == "minsizerel" || value == "reldebinfo" ) {
        return "build";
    } else
        return "";
}
size_t GlobalCtx::get_triplet_pos( const String &name ) {
    if ( name == "arch" )
        return 0;
    else if ( name == "os" )
        return 1;
    else if ( name == "plattform" )
        return 2;
    else if ( name == "format" )
        return 3;
    else if ( name == "backend" )
        return 4;
    else if ( name == "runtime" )
        return 5;
    else if ( name == "linkage" )
        return 6;
    else if ( name == "build" )
        return 7;
    else
        return 8;
}

// Copyright 2020 Erik Götzfried
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
#include "libpush/stdafx.h"

#define PUSH_VERSION_MAJOR 0
#define PUSH_VERSION_MINOR 1
#define PUSH_VERSION_PATCH 0

using u8 = unsigned char;
using u16 = unsigned short;
using u32 = unsigned int;
using u64 = unsigned long long;

using i8 = signed char;
using i16 = signed short;
using i32 = signed int;
using i64 = signed long long;

using f32 = float;
using f64 = double;

using StringW = std::wstring;
using std::stod;
using std::stof;
using std::stoi;
using std::stol;
using std::stold;
using std::stoll;
using std::stoul;
using std::stoull;
using std::to_string;

using Mutex = std::mutex;
using Lock = std::lock_guard<Mutex>;
using UniqueLock = std::unique_lock<Mutex>;
using ConditionVariable = std::condition_variable;

static Mutex log_mtx;

#define SCILENT_LOG 0

// TODO pipe directly to the user somehow (e. g. through compiler error messages)
static void log( const std::string &msg ) {
#if !SCILENT_LOG
    log_mtx.lock();
    std::cout << msg << std::endl;
    log_mtx.unlock();
#endif
}
// Use this loggers for internal errors etc.
#define LOG( msg ) log( std::string( "MSG: " ) + msg )
#define LOG_ERR( msg ) log( std::string( "ERROR: " ) + msg + " (" + __FILE__ + ":" + to_string( __LINE__ ) + ")" )
#define LOG_WARN( msg ) log( std::string( "WARNING: " ) + msg + " (" + __FILE__ + ":" + to_string( __LINE__ ) + ")" )

#if defined( _WIN32 )
namespace fs = std::experimental::filesystem;
#else
namespace fs = std::filesystem;
#endif

template <typename T>
using sptr = std::shared_ptr<T>;

using std::make_shared;

inline void Sleep( f64 ms_duration ) {
    std::this_thread::sleep_for( std::chrono::duration<f64, std::milli>( ms_duration ) );
}

class Worker;
class GlobalCtx;
class UnitCtx;

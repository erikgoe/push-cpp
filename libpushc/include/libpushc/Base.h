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
#include "libpushc/stdafx.h"

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

using String = std::string;
using StringW = std::wstring;
using std::to_string;

using Mutex = std::recursive_mutex;
using Lock = std::lock_guard<Mutex>;

static Mutex log_mtx;

#define SCILENT_LOG 1

static void log( const String &msg ) {
    #if ! SCILENT_LOG
    log_mtx.lock();
    std::cout << msg << std::endl;
    log_mtx.unlock();
    #endif
}
#define LOG( msg ) log( String( "MSG: " ) + msg )
#define LOG_ERR( msg ) log( String( "ERROR: " ) + msg + " (" + to_string( __LINE__ ) + " \"" + __FILE__ + "\")" )
#define LOG_WARN( msg ) log( String( "WARNING: " ) + msg + " (" + to_string( __LINE__ ) + " \"" + __FILE__ + "\")" )

#if defined( _WIN32 )
namespace fs = std::experimental::filesystem;
#elif
namespace fs = sf::filesystem;
#endif

inline void Sleep( f64 ms_duration ) {
    std::this_thread::sleep_for( std::chrono::duration<f64, std::milli>( ms_duration ) );
}

class Worker;
class QueryMgr;

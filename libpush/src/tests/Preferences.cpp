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

#include "libpush/tests/stdafx.h"
#include "libpush/GlobalCtx.h"

TEST_CASE( "Prefs usage", "[prefs]" ) {
    GlobalCtx ctx;

    ctx.set_pref<BoolSV>( PrefType::release_speed_optimization, false );
    CHECK( ctx.get_pref<BoolSV>( PrefType::release_speed_optimization ) == false );

    ctx.set_pref<IntSV>( PrefType::release_speed_optimization, 1234 );
    ctx.set_pref<IntSV>( PrefType::release_speed_optimization, 3210 );
    CHECK( ctx.get_pref<IntSV>( PrefType::release_speed_optimization ) == 3210 );

    CHECK( ctx.get_pref<StringSV>( PrefType::platform ) == "" );
    
    ctx.set_pref<StringSV>( PrefType::backend, "llvm" );
    ctx.set_pref<StringSV>( PrefType::platform, "pc" );
    CHECK( ctx.get_pref<StringSV>( PrefType::platform ) == "pc" );
    CHECK( ctx.get_pref<StringSV>( PrefType::backend ) == "llvm" );
    
    CHECK( ctx.get_pref<IntSV>( PrefType::release_speed_optimization ) == 3210 );
    CHECK( ctx.get_pref<StringSV>( PrefType::backend ) == "llvm" );
    CHECK( ctx.get_pref<StringSV>( PrefType::platform ) == "pc" );
}

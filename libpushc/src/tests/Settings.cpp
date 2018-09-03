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

#include "libpushc/tests/stdafx.h"
#include "libpushc/Context.h"

TEST_CASE( "Settings usage", "[settings]" ) {
    Context ctx;

    ctx.set_setting<BoolSV>( SettingType::release_optimization, false );
    CHECK( ctx.get_setting<BoolSV>( SettingType::release_optimization ) == false );

    ctx.set_setting<IntSV>( SettingType::release_optimization, 1234 );
    ctx.set_setting<IntSV>( SettingType::release_optimization, 3210 );
    CHECK( ctx.get_setting<IntSV>( SettingType::release_optimization ) == 3210 );

    ctx.set_setting<StringSV>( SettingType::backend, "llvm" );
    ctx.set_setting<StringSV>( SettingType::platform, "pc" );
    CHECK( ctx.get_setting<StringSV>( SettingType::platform ) == "pc" );
    CHECK( ctx.get_setting<StringSV>( SettingType::backend ) == "llvm" );
    
    CHECK( ctx.get_setting<IntSV>( SettingType::release_optimization ) == 3210 );
    CHECK( ctx.get_setting<StringSV>( SettingType::backend ) == "llvm" );
    CHECK( ctx.get_setting<StringSV>( SettingType::platform ) == "pc" );
}

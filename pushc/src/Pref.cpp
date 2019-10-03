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

#include "pushc/stdafx.h"
#include "pushc/CLI.h"

// Check if the value is a boolean toggle
bool check_boolean( const String& value ) {
    return value == "y" || value == "yes" || value == "n" || value == "no" || value == "on" || value == "off" ||
           value == "true" || value == "false";
}
// Check if the value is a boolean toggle or flag
bool check_boolean_flag( const String& value ) {
    return value == "" || check_boolean( value );
}

// Returns the boolean value of a string
bool get_boolean( const String& str ) {
    return str == "y" || str == "yes" || str == "on" || str == "true";
}
// Returns the boolean value of a string
bool get_boolean_flag( const String& str ) {
    return str == "" || get_boolean( str );
}


bool CLI::find_pref( const String& pref ) {
    return pref == "lto";
}
bool CLI::find_flag( const String& flag ) {
    return flag == "lto" || flag == "no_lto";
}

bool CLI::store_config( GlobalCtx& g_ctx, const String& name, const String& value ) {
    if ( name == "lto" ) {
        if ( !check_boolean_flag( value ) )
            return false;
        g_ctx.set_pref<BoolSV>( PrefType::lto, get_boolean_flag( value ) );
    }
    return false;
}

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
#include "libpushc/Context.h"

void Context::update_global_prefs() {
    String::TAB_WIDTH = get_pref_or_set<SizeSV>( PrefType::tab_size, 4 );
    max_allowed_errors = get_pref_or_set<SizeSV>( PrefType::max_errors, 256 );
    max_allowed_warnings = get_pref_or_set<SizeSV>( PrefType::max_warnings, 256 );
    max_allowed_notifications = get_pref_or_set<SizeSV>( PrefType::max_notifications, 256 );
}

String Context::get_triplet_elem_name( const String &value ) {
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
size_t Context::get_triplet_pos( const String &name ) {
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

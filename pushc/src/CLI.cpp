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
#include "pushc/stdafx.h"
#include "pushc/CLI.h"

int main( int argc, char** argv ) {
    auto cli = CLI();
    auto ret = cli.setup( argc, argv );
    if ( !ret )
        ret = cli.execute();
    return ret;
}

int CLI::setup( int argc, char** argv ) {
    // extract arguments
    for ( int i = 1; i < argc; i++ ) {
        auto str = String( argv[i] );
        if ( str.size() > 2 && str.slice( 0, 2 ) == String( "--" ) ) { // single option
            args[str];
            if ( i + 1 < argc ) { // get value
                auto str2 = String( argv[i + 1] );
                if ( !str2.empty() && str2[0] != '-' ) { // if not the next parameter
                    args[str].push_back( str2 );
                    i++; // was consumed
                }
            }
        } else if ( str.size() > 1 && str[0] == '-' ) { // multi option
            for ( size_t j = 1; j < str.size(); j++ )
                args["-" + String( 1, str[j] )];
            if ( i + 1 < argc ) { // get value
                auto str2 = String( argv[i + 1] );
                if ( !str2.empty() && str2[0] != '-' ) { // if not the next parameter
                    for ( size_t j = 1; j < str.size(); j++ )
                        args["-" + String( 1, str[j] )].push_back( str2 );
                    i++; // was consumed
                }
            }
        } else { // file
            files.push_back( str );
        }
    }
    return 0;
}

int CLI::execute() {
    if ( has_par( "--help" ) || has_par( "-h" ) ) {
        print_help_text();
    } else if ( has_par( "--version" ) || has_par( "-v" ) ) {
        std::cout << "Push infrastructure version " + to_string( PUSH_VERSION_MAJOR ) + "." +
                         to_string( PUSH_VERSION_MINOR ) + "." + to_string( PUSH_VERSION_PATCH );
    }

    return 0;
}

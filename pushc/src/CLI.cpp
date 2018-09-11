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
        std::cout << "Compiler for the Push programming language.\n";
        std::cout << "  pushc [--option/-o [value] ...] [file ...]\n"
                     "  pushc --help/-h/--version/-v\n"
                     "\n";

        std::cout << "Compiles the passed file(s) and writes the output in the\n"
                     "file/directory defined by \"--output\" or if not specified, in the same\n"
                     "directory. If you don't specify any files to compile, pushc will search for\n"
                     "a .proj or .prj file in the current directory and compile it. If there are no\n"
                     "such files, pushc will compile all .push files in the current directory.\n"
                     "\n";
        std::cout << "Available options:\n";
        std::cout << "  -h --help                  Print this help text.\n";
        std::cout << "  -v --version               Print some version information.\n";
        std::cout << "  -o --output <file(s)>*     Output file or directory. See below.\n";
        std::cout << "  -r --run                   Execute after successful build (not for libs).\n";
        std::cout << "  -t --triplet <triplet>*    Defines the used triplet. See below.\n";
        std::cout << "  -c --config <flag/pref>*   Comma-separated list of flags or preference-pairs\n"
                     "                               in the form of <name>=<value>. Overwrites -t.\n";
        std::cout << "  --prelude <file>           Overwrites default or in-file prelude definition.\n";
        std::cout << "  --threads <count>          Used parallel threads. 0 = let pushc decide.\n";
        std::cout << "  --color <auto|always|never>\n"
                     "                             (De-)Activate coloring of the output messages.\n";
        std::cout << "  --clean [global]           Deletes the build output and cache. With \"global\"\n"
                     "                               the user-global cache is deleted too.\n";
        std::cout << "\n";
        std::cout << "Any of the above options may be passed in any order. The files may also be\n"
                     "passed in between two or more options or before an option. Every option\n"
                     "expects exactly zero or one argument values. Values of combined options,\n"
                     "like \"-or file.push\" will be applied to all flags (in this case -o and -r).\n"
                     "If an option does expect no values, all passed values are ignored. You may use\n"
                     "the same option multiple times. In this case some argument values are appended,\n"
                     "others overwrite the previous value. In the above list all options which\n"
                     "appending values are marked with an asterisk (*). Options are case sensitive.\n"
                     "\n"
                     "For multiple input files the output files must be passed in the same order.\n"
                     "If there are more input files than output files, the remaining files will be\n"
                     "written into the last passed directory.\n"
                     "\n"
                     "The push-triplet contains all required information to identify a target \n"
                     "configuration. The complete signature is:\n"
                     "  <architecture>-<os/kernel/framework;specification>-<plattform/vendor>-\n"
                     "  <output_format>-<backend>-<runtime>-<linkage>-<build_configuration> \n"
                     "You may configure parts of the triplet and leave the remaining defaults with\n"
                     "a <name>=<value> pair.\n";
    } else if ( has_par( "--version" ) || has_par( "-v" ) ) {
        std::cout << "Push infrastructure version " + to_string( PUSH_VERSION_MAJOR ) + "." +
                         to_string( PUSH_VERSION_MINOR ) + "." + to_string( PUSH_VERSION_PATCH );
    }

    return 0;
}

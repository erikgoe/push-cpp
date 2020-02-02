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

#include "pushc/stdafx.h"
#include "pushc/CLI.h"
#include "libpushc/Compiler.h"

int main( int argc, char** argv ) {
    auto cli = CLI();
    auto ret = cli.setup( argc, argv );
    if ( !ret )
        ret = cli.execute();
    return ret;
}

// Check if the option contains a parameter, if not print a message and return false
bool check_par( const std::pair<String, std::list<String>>& arg ) {
    if ( arg.second.empty() ) {
        std::cout << arg.first << " expects a parameter.\n ";
        return false;
    }
    return true;
}

void split_str_at_char( const String& str, char delimiter, std::list<String>& fill_list ) {
    size_t itr, itr_begin = 0;
    do {
        itr = str.find( delimiter, itr_begin );
        fill_list.push_back( str.substr( itr_begin, itr - itr_begin ) );
        itr_begin = itr + 1;
    } while ( itr != str.npos );
}
void CLI::store_triplet_elem( GlobalCtx& g_ctx, const String& name, const String& value ) {
    if ( name == "arch" )
        g_ctx.set_pref<StringSV>( PrefType::architecture, value );
    else if ( name == "os" )
        g_ctx.set_pref<StringSV>( PrefType::os, value );
    else if ( name == "platform" )
        g_ctx.set_pref<StringSV>( PrefType::platform, value );
    else if ( name == "format" )
        g_ctx.set_pref<StringSV>( PrefType::output_format, value );
    else if ( name == "backend" )
        g_ctx.set_pref<StringSV>( PrefType::backend, value );
    else if ( name == "runtime" )
        g_ctx.set_pref<StringSV>( PrefType::runtime, value );
    else if ( name == "linkage" )
        g_ctx.set_pref<BoolSV>( PrefType::dynamic_linkage, value == "dynamic" );
    else if ( name == "build" ) {
        if ( value == "debug" ) {
            g_ctx.set_pref<BoolSV>( PrefType::release_speed_optimization, false );
            g_ctx.set_pref<BoolSV>( PrefType::release_size_optimization, false );
            g_ctx.set_pref<BoolSV>( PrefType::debug_symbols, true );
        }
        if ( value == "release" ) {
            g_ctx.set_pref<BoolSV>( PrefType::release_speed_optimization, true );
            g_ctx.set_pref<BoolSV>( PrefType::release_size_optimization, false );
            g_ctx.set_pref<BoolSV>( PrefType::debug_symbols, false );
        }
        if ( value == "minsizerel" ) {
            g_ctx.set_pref<BoolSV>( PrefType::release_speed_optimization, false );
            g_ctx.set_pref<BoolSV>( PrefType::release_size_optimization, true );
            g_ctx.set_pref<BoolSV>( PrefType::debug_symbols, false );
        }
        if ( value == "reldebinfo" ) {
            g_ctx.set_pref<BoolSV>( PrefType::release_speed_optimization, true );
            g_ctx.set_pref<BoolSV>( PrefType::release_size_optimization, false );
            g_ctx.set_pref<BoolSV>( PrefType::debug_symbols, true );
        }
    }
}
int CLI::fill_triplet( std::map<String, String>& triplet_list, const String& arg_name,
                       const std::list<String>& arg_value ) {
    for ( auto& s : arg_value ) { // split all elements
        std::list<String> splitted;
        split_str_at_char( s, ',', splitted );
        bool is_paired = false; // if a sinple pair or a list of pairs

        if ( splitted.size() == 1 ) { // single parameter
            std::list<String> splitted2;
            split_str_at_char( s, '-', splitted2 );

            if ( splitted2.size() == 1 ) {
                is_paired = true;
            } else { // contains regular triplet
                size_t t_pos = 0; // triplet element number
                for ( auto& elem : splitted2 ) {
                    if ( t_pos >= 8 ) // last triplet was not found
                        break;
                    while ( t_pos < 8 ) {
                        if ( GlobalCtx::get_triplet_pos( GlobalCtx::get_triplet_elem_name( elem ) ) ==
                             t_pos ) { // found right position
                            triplet_list[GlobalCtx::get_triplet_elem_name( elem )] = elem;
                            break;
                        }
                        t_pos++;
                    }
                    t_pos++;
                }
                if ( t_pos >= 8 ) { // last triplet was not found
                    std::cout << "Was not able to resolve triplet\n";
                    return RET_COMMAND_ERROR;
                }
            }
        } else
            is_paired = true;

        if ( is_paired ) { // parameterlist
            for ( auto& s2 : splitted ) { // check pairs
                std::list<String> splitted2;
                split_str_at_char( s2, '=', splitted2 );

                if ( splitted2.size() != 2 ) {
                    std::cout << arg_name << ": requires pairs in form of <name>=<value>\n";
                    return RET_COMMAND_ERROR;
                } else { // right format
                    if ( GlobalCtx::get_triplet_pos( splitted2.front() ) >= 8 ) {
                        std::cout << "Unknown triplet element name \"" + splitted2.front() + "\".\n";
                        return RET_COMMAND_ERROR;
                    } else if ( GlobalCtx::get_triplet_elem_name( splitted2.back() ) != splitted2.front() ) {
                        std::cout << "Unknown triplet value \"" + splitted2.back() + "\" for \"" + splitted2.front() +
                                         "\".\n";
                        if ( !GlobalCtx::get_triplet_elem_name( splitted2.back() ).empty() )
                            std::cout << "Did you mean \"" + GlobalCtx::get_triplet_elem_name( splitted2.back() ) +
                                             "=" + splitted2.back() + "\"?\n";
                        return RET_COMMAND_ERROR;
                    } else { // found matching values
                        triplet_list[splitted2.front()] = splitted2.back();
                    }
                }
            }
        }
    }
    return RET_SUCCESS;
}
int CLI::fill_config( std::map<String, String>& config_list, const String& arg_name,
                      const std::list<String>& arg_value ) {
    for ( auto& s : arg_value ) { // split all elements
        std::list<String> splitted;
        split_str_at_char( s, ',', splitted );

        for ( auto& s2 : splitted ) {
            std::list<String> splitted2;
            split_str_at_char( s2, '=', splitted2 );
            if ( splitted2.size() == 1 ) { // just a flag
                if ( find_flag( splitted2.front() ) ) {
                    config_list[splitted2.front()];
                } else {
                    std::cout << "Unknown flag \"" + splitted2.front() + "\".\n";
                    return RET_COMMAND_ERROR;
                }
            } else if ( splitted2.size() == 2 ) { // a config
                if ( find_pref( splitted2.front() ) ) {
                    config_list[splitted2.front()] = splitted2.back();
                } else {
                    std::cout << "Unknown config \"" + splitted2.front() + "\".\n";
                    if ( find_flag( splitted2.front() ) )
                        std::cout << "Did you mean the flag \"" + splitted2.front() + "\"?\n";
                    return RET_COMMAND_ERROR;
                }
            } else {
                std::cout << "Wrong config format \"" + s2 + "\". Must be a <name>=<value> pair or a flag.\n";
                return RET_COMMAND_ERROR;
            }
        }
    }
    return RET_SUCCESS;
}

size_t CLI::get_cpu_count() {
    // TODO
    return 4;
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
                         to_string( PUSH_VERSION_MINOR ) + "." + to_string( PUSH_VERSION_PATCH ) + "\n";
    } else { // Regular compilation call
        std::list<String> output_files; // TODO
        bool run_afterwards = false;
        bool clean_build = false;
        String explicit_prelude; // TODO
        size_t thread_count = 0;
        String color = "auto"; // TODO
        std::map<String, String> triplet_list;
        std::map<String, String> config_list;

        // Extract args
        for ( auto& arg : args ) {
            if ( arg.first == "--output" || arg.first == "-o" ) {
                if ( !check_par( arg ) )
                    return RET_COMMAND_ERROR;
                for ( auto& s : arg.second ) { // split all files
                    split_str_at_char( s, ',', output_files );
                }
            } else if ( arg.first == "--run" || arg.first == "-r" ) {
                run_afterwards = true;
            } else if ( arg.first == "--triplet" || arg.first == "-t" ) {
                if ( !check_par( arg ) )
                    return RET_COMMAND_ERROR;
                int ret = fill_triplet( triplet_list, arg.first, arg.second );
                if ( ret != RET_SUCCESS )
                    return ret;
            } else if ( arg.first == "--config" || arg.first == "-c" ) {
                if ( !check_par( arg ) )
                    return RET_COMMAND_ERROR;
                int ret = fill_triplet( config_list, arg.first, arg.second );
                if ( ret != RET_SUCCESS )
                    return ret;
            } else if ( arg.first == "--prelude" ) {
                if ( !check_par( arg ) )
                    return RET_COMMAND_ERROR;
                explicit_prelude = arg.second.back();
            } else if ( arg.first == "--threads" ) {
                if ( !check_par( arg ) )
                    return RET_COMMAND_ERROR;
                thread_count = stoi( arg.second.back() );
            } else if ( arg.first == "--color" ) {
                if ( !check_par( arg ) )
                    return RET_COMMAND_ERROR;
                if ( arg.second.back() != "auto" && arg.second.back() != "always" && arg.second.back() != "never" ) {
                    std::cout << "--color: \"" + arg.second.back() + "\" wrong parameter.\n";
                    return RET_COMMAND_ERROR;
                }
                color = arg.second.back();
            } else if ( arg.first == "--clean" ) {
                clean_build = true;
            } else if ( arg.first != "--help" && arg.first != "-h" && arg.first != "--version" && arg.first != "-v" ) {
                std::cout << "Unknown option \"" + arg.first + "\"\n";
                return RET_COMMAND_ERROR;
            }
        }

        // Decide how many threads to use
        if ( thread_count == 0 )
            thread_count = get_cpu_count() * 2;

        // Create the compilation contexts
        auto g_ctx = make_shared<GlobalCtx>();
        auto w_ctx = g_ctx->setup( thread_count );

        // Set configs & triplet
        for ( auto& cfg : config_list ) {
            if ( !store_config( *g_ctx, cfg.first, cfg.second ) ) {
                // Malformed value was found
                std::cout << "Malformed value \"" + cfg.second + "\" for flag \"" + cfg.first + "\".\n";
                return RET_COMMAND_ERROR;
            }
        }
        for ( auto& t : triplet_list ) {
            store_triplet_elem( *g_ctx, t.first, t.second );
        }

        // Do some preparation
        if ( files.empty() ) { // find project file or .push files TODO
        }
        if ( clean_build ) { // clean before TODO
        }

        // Create initial queries
        for ( auto& file : files ) { // TODO first create all queries with no reserved jobs, then execute
            w_ctx->do_query( compile_new_unit, file );
        }

        if ( run_afterwards ) { // execute now TODO
        }
    }

    return 0;
}

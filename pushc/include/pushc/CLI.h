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

#pragma once
#include "pushc/stdafx.h"

// Basic Command Line Interface driver
class CLI {
    // Return values
    constexpr static int RET_SUCCESS = 0;
    constexpr static int RET_UNKNOWN_ERROR = -1;
    constexpr static int RET_COMMAND_ERROR = 1;

    std::map<String, std::list<String>> args;
    std::list<String> files;

    // Returns true if the parameter is provided
    bool has_par( const String& parameter_name ) { return args.find( parameter_name ) != args.end(); }

    // Prints the help text into the console
    void print_help_text();

    // Returns true if the CLI has this preference name
    static bool find_pref( const String& pref );
    // Returns true if the CLI has this flag name
    static bool find_flag( const String& flag );

    // Stores a specific configuration setting in a GlobalCtx. Returns false if value is malformed
    bool store_config( GlobalCtx& g_ctx, const String& name, const String& value );

    // Stores a triplet element in a GlobalCtx
    void store_triplet_elem( GlobalCtx& g_ctx, const String& name, const String& value );

    // Helper function to fill the triplet_list
    int fill_triplet( std::map<String, String>& triplet_list, const String& arg_name,
                      const std::list<String> &arg_value );
    // Helper function to fill the config_list
    int fill_config( std::map<String, String>& config_list, const String& arg_name, const std::list<String> &arg_value );

    // Returns how many cores this machine has
    size_t get_cpu_count();

public:
    // Initializes the driver
    int setup( int argc, char** argv );
    // Executes the given arguments
    int execute();
};

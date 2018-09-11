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

// Basic Command Line Interface driver
class CLI {
    // Return values
    constexpr static int RET_SUCCESS = 0;
    constexpr static int RET_UNKNOWN_ERROR = -1;
    constexpr static int RET_COMMAND_ERROR = 1;

    std::map<String, std::list<String>> args;
    std::list<String> files;

public:
    // Initializes the driver
    int setup( int argc, char** argv );
    // Executes the given arguments
    int execute();
};

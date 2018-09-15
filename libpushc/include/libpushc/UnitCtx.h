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
#include "libpushc/Base.h"
#include "libpushc/GlobalCtx.h"

// The context of a compilation unit
class UnitCtx {
    // Stores all compilation unit files used in this compilation pass
    static std::vector<String> known_files;
    static Mutex known_files_mtx;

    std::shared_ptr<GlobalCtx> g_ctx;

public:
    std::shared_ptr<String> root_file; // main file of this compilation unit
    size_t id; // uniquely identifies this compilation unit

    // Create a new unit context
    UnitCtx( std::shared_ptr<String> &filepath, std::shared_ptr<GlobalCtx> g_ctx ) {
        this->g_ctx = g_ctx;
        root_file = filepath;

        Lock lock( known_files_mtx );
        size_t new_id = 0;
        for ( auto &f : known_files ) {
            if ( f == *filepath )
                break;
            new_id++;
        }
        id = new_id;
    }

    // Returns the global context
    std::shared_ptr<GlobalCtx> get_global_ctx() { return g_ctx; }
};

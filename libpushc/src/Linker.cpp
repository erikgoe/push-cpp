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

#include "libpushc/stdafx.h"
#include "libpushc/Linker.h"
#include "libpushc/Compiler.h"
#include "libpushc/Backend.h"

// Local linker query, to build a new unit in its own context
void build_unit( const String &unit_path, JobsBuilder &jb, UnitCtx &parent_ctx ) {
    auto ctx = std::make_shared<UnitCtx>( std::make_shared<String>( unit_path ), parent_ctx.global_ctx() );
    jb.switch_context( ctx );

    jb.add_job<void>( []( Worker &w_ctx ) {
        //w_ctx.do_query( get_object_file );
    } );
}

void link_binary( const String &out_file, JobsBuilder &jb, UnitCtx &parent_ctx ) {
    jb.add_job<void>( [out_file]( Worker &w_ctx ) {
        /*auto jc = w_ctx.do_query( get_compilation_units );
        auto units = jc->jobs.front()->to<std::vector<String>>();

        for ( auto &unit : units ) {
            w_ctx.do_query( build_unit, unit );
        }*/
    } );
}

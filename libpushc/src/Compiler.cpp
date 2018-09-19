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

#include "libpushc/stdafx.h"
#include "libpushc/Compiler.h"
#include "libpushc/Prelude.h"

void compile_new_unit( const String &file, JobsBuilder &jb, UnitCtx &parent_ctx ) {
    auto ctx = std::make_shared<UnitCtx>( std::make_shared<String>( file ), parent_ctx.global_ctx() );
    jb.switch_context( ctx );
    jb.add_job<void>( [file]( Worker &w_ctx ) {
        auto extension = fs::path( file ).extension();
        if ( extension == ".proj" || extension == ".prj" ) { // compile project file
            w_ctx.do_query( load_prelude, std::make_shared<String>( "project" ) );
        } else { // regular push file
            w_ctx.do_query( load_prelude, std::make_shared<String>( "push" ) );
        }
        
    } );
}

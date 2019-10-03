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
#include "libpushc/Linker.h"

void compile_new_unit( const String &file, JobsBuilder &jb, UnitCtx &parent_ctx ) {
    auto ctx = std::make_shared<UnitCtx>( std::make_shared<String>( file ), parent_ctx.global_ctx() );
    jb.switch_context( ctx );
    jb.add_job<void>( [file]( Worker &w_ctx ) {
        // w_ctx.do_query( link_binary, file.to_path().replace_extension( ".exe" ) );
        // w_ctx.query( link_binary, file.to_path().replace_extension( ".exe" ) )->execute( w_ctx );
    } );
}

void get_compilation_units( JobsBuilder &jb, UnitCtx &parent_ctx ) {
    jb.add_job<std::vector<String>>( []( Worker &w_ctx ) {
        auto file = *w_ctx.unit_ctx()->root_file;
        auto extension = file.to_path().extension();
        if ( extension == ".proj" || extension == ".prj" ) { // compile project file
            // TODO
        } else { // regular push file
            std::vector<String> result;
            result.push_back( file );
            return result;
        }
    } );
}

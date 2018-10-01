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
#include "libpush/stdafx.h"
#include "libpush/basic_queries/FileQueries.h"

std::shared_ptr<SourceInput> get_source_input( const String file, Worker &w_ctx ) {
    std::shared_ptr<SourceInput> source_input;
    auto input_pref = w_ctx.global_ctx()->get_pref<StringSV>( PrefType::input_source );
    if ( input_pref == "file" ) {
        if ( !FileInput::file_exists( file ) ) {
            w_ctx.print_msg<MessageType::ferr_file_not_found>( MessageInfo(), {}, file );
        }
        source_input = std::make_shared<FileInput>( file, 8192, 4096, w_ctx.shared_from_this() );
    } else {
        LOG_ERR( "Unknown input type pref." );
        w_ctx.print_msg<MessageType::err_unknown_source_input_pref>( MessageInfo(), {}, input_pref, file );
    }
    return source_input;
}

std::shared_ptr<String> get_std_dir() {
    // TODO
    return std::make_shared<String>( CMAKE_PROJECT_ROOT "/libstd" );
}

void get_source_lines( const String file, size_t line_begin, size_t line_end, JobsBuilder &jb, UnitCtx &ctx ) {
    jb.add_job<std::list<String>>( [&, file, line_begin, line_end]( Worker &w_ctx ) {
        auto source = get_source_input( file, w_ctx );
        return source->get_lines( line_begin, line_end, w_ctx );
    } );
}
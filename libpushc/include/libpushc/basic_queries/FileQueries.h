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
#include "libpushc/stdafx.h"
#include "libpushc/input/FileInput.h"
#include "libpushc/QueryMgr.h"

// NOT A QUERY! Returns a source input defined by the current settings
std::shared_ptr<SourceInput> get_source_input( const String file, QueryMgr &qm ) {
    std::shared_ptr<SourceInput> source_input;
    auto input_setting = qm.get_global_context()->get_setting<StringSV>( SettingType::input_source );
    if ( input_setting == "file" ) {
        source_input = std::make_shared<FileInput>( file, 8192, 4096 );
    } else {
        LOG_ERR( "Unknown input type setting." );
        // TODO print error
    }
    return source_input;
}

// Returns
void get_source_lines( const String file, size_t line_begin, size_t line_end, JobsBuilder &jb, QueryMgr &qm ) {
    jb.add_job<std::list<String>>( [&, file, line_begin, line_end]( Worker &w_ctx ) {
        auto source = get_source_input( file, qm );
        return source->get_lines( line_begin, line_end );
    } );
}
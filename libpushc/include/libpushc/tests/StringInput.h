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

#pragma once
#include "libpush/Base.h"
#include "libpush/input/StreamInput.h"

// Provides token input from a string
class StringInput : public StreamInput {
public:
    StringInput( sptr<String> file, sptr<Worker> w_ctx, const String &data )
            : StreamInput( make_shared<std::basic_istringstream<char>>( data ), file, w_ctx ) {}

    sptr<SourceInput> open_new_file( sptr<String> file, sptr<Worker> w_ctx ) {
        return make_shared<StringInput>( file, w_ctx, "" );
    }

    static bool file_exists( const String &file ) { return false; }
};

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
#include "libpushc/util/String.h"

// Returns the length of the string in code points
template <typename T>
size_t length_of_string( const T &str ) {
    size_t code_points = 0;
    const char *c_str = str.c_str();

    for ( size_t i = 0 ; i < str.size() ; i++ ) {
        if ( ( c_str[i] & 0xC0 ) != 0x80 )
            code_points++;
    }
    return code_points;
}


const StringSlice &String::slice( size_t pos, size_t size ) {
    if ( last_slice == nullptr ) {
        return *( last_slice = new StringSlice( *this, pos, size ) );
    } else
        return last_slice->reslice( *this, pos, size );
}
StringSlice String::slice( size_t pos, size_t size ) const {
    return StringSlice( *this, pos, size );
}
size_t String::length_cp() const {
    return length_of_string( *this );
}

size_t StringSlice::length_cp() const {
    return length_of_string( *this );
}
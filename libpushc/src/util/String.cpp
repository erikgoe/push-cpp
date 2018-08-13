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

const StringSlice &String::slice( size_t pos, size_t size ) {
    if ( last_slice == nullptr ) {
        return *(last_slice = new StringSlice( *this, pos, size ));
    } else
        return last_slice->reslice( *this, pos, size );
}
StringSlice String::slice( size_t pos, size_t size ) const {
    return StringSlice( *this, pos, size );
}

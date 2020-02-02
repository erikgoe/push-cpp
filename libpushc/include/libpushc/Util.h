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
#include "libpushc/stdafx.h"

// NOT A QUERY! Consumes a comment till the end
void consume_comment( sptr<SourceInput> &input, TokenConfig &conf );

// Parse the content of a string
String parse_string( sptr<SourceInput> &input, Worker &w_ctx );

// Representation of any integer
using Number = u64;

// Pares a value
Number parse_number( sptr<SourceInput> &input, Worker &w_ctx );

// Returns true if @param element is in @param collection
template <typename T, typename Collection>
bool element_of( const T &element, const Collection &collection ) {
    for ( auto &c : collection )
        if ( c == element )
            return true;
    return false;
}

// Whether a token is a operator or a keyword
bool is_operator_token( const String &token );

// Hash for std::vector
namespace std {
template <typename T>
struct hash<vector<T>> {
    size_t operator()( vector<T> const &in ) const {
        size_t h = 0;
        for ( size_t i = 0; i < in.size(); i++ )
            hash_combine( h, in[i] );
        return h;
    }
};

// using boost::hash_combine
template <class T>
inline void hash_combine( std::size_t &seed, T const &v ) {
    seed ^= std::hash<T>()( v ) + 0x9e3779b9 + ( seed << 6 ) + ( seed >> 2 );
}
} // namespace std

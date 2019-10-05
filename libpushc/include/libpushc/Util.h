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

#pragma once
#include "libpushc/stdafx.h"

// NOT A QUERY! Consumes a comment till the end
void consume_comment( sptr<SourceInput> &input, TokenConfig &conf );

// Parse the content of a string
String parse_string( sptr<SourceInput> &input, Worker &w_ctx );

// Representation of any number
struct Number {
    enum class Type { integer, unsigned_int, floating_p } type;
    union Value {
        i64 integer;
        u64 unsigned_int;
        f64 floating_p;
    } value;
    String type_postfix;

    f64 as_float() {
        if ( type == Type::integer )
            return static_cast<f64>( value.integer );
        else if ( type == Type::unsigned_int )
            return static_cast<f64>( value.unsigned_int );
        else if ( type == Type::floating_p )
            return value.floating_p;
        else
            return 0.;
    }
    i64 as_int() {
        if ( type == Type::integer )
            return value.integer;
        else if ( type == Type::unsigned_int )
            return static_cast<i64>( value.unsigned_int );
        else if ( type == Type::floating_p )
            return static_cast<i64>( value.floating_p );
        else
            return 0;
    }
    u64 as_uint() {
        if ( type == Type::integer )
            return static_cast<u64>( value.unsigned_int );
        else if ( type == Type::unsigned_int )
            return value.unsigned_int;
        else if ( type == Type::floating_p )
            return static_cast<u64>( value.floating_p );
        else
            return 0;
    }
};

// Pares a value
Number parse_number( sptr<SourceInput> &input, Worker &w_ctx, sptr<PreludeConfig> &conf );

// Returns true if \param element is in \param collection
template <typename T, typename Collection>
bool element_of( const T &element, const Collection &collection ) {
    for ( auto &c : collection )
        if ( c == element )
            return true;
    return false;
}

// Whether a token is a operator or a keyword
bool is_operator_token( const String &token );

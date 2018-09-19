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
#include "libpush/Base.h"
#include "libpush/util/String.h"
#include "libpush/input/SourceInput.h"
#undef pascal

// Format case for identifiers
enum class IdentifierCase {
    snake,
    pascal,
    camel,

    count
};
// The rules for one type of string
struct StringRule {
    String begin;
    String end;
    String prefix;
    String rep_begin; // limits raw strings
    String rep_end; // limits raw strings
    bool escaped = true; // may contain escape sequences
    bool block = false; // use a whole block as single literal
    bool utf8 = true; // otherwise 32bit
};
// The rules for one type of character
struct CharacterRule {
    String begin;
    String end;
    String prefix;
    bool escaped = true; // may contain escape sequences
    bool bit32 = true; // otherwise ascii
};

// Represents any list with type -> name pairs or just keywords.
using Syntax = std::list<std::pair<String, String>>;

// Defines syntax rules for any operator (binary or unary)
struct Operator {
    size_t precedence = 0; // how operators are combined
    bool ltr = true; // left to right or right to left
    Syntax syntax; // left type -> name pair
};

// Operator which is implementaed in a library through a trait
struct TraitOperator {
    Operator op;
    String trait;
    String fn; // function in the trait
};

// A function rule
struct FunctionDefinition {
    String function_trait;
    String function_fn; // function in the trait
    Syntax syntax;
};

// Operator to describe a range
struct RangeOperator {
    // Defines which range it describes
    enum class Type {
        exclude,
        exclude_from,
        exclude_to,
        include,
        include_to,

        count
    } type;
    Operator op;
};

// The configuration which defines most compiler rules
struct PreludeConfig {
    bool is_prelude = false; // whether special prelude options are activated
    bool is_prelude_library = false; // if was included by a prelude file
    TokenConfig token_conf;

    bool exclude_operators = false; // from identifiers
    bool exclude_keywords = false; // from identifiers
    bool spaces_bind_identifiers = false; // if space combines two identifiers
    IdentifierCase function_case = IdentifierCase::snake;
    IdentifierCase method_case = IdentifierCase::snake;
    IdentifierCase variable_case = IdentifierCase::snake;
    IdentifierCase module_case = IdentifierCase::snake;
    IdentifierCase struct_case = IdentifierCase::pascal;
    IdentifierCase trait_case = IdentifierCase::pascal;
    std::list<String> unused_prefix; // prefix for unused variables

    std::list<String> ascii_octal_prefix; // prefix for octal ascii encoded chars
    std::list<String> ascii_hex_prefix; // prefix for hex ascii encoded chars
    std::list<String> unicode_hex_prefix; // prefix for hex unicode encoded chars
    bool truncate_unicode_hex_prefix = false; // whether leading zeros may be omitted
    std::list<StringRule> string_rules;
    std::list<std::pair<String, String>> value_extract; // escape limiter for value extraction
    bool allow_multi_value_extract = false; // whether multiple values in one extraction are allowed
    std::list<CharacterRule> char_rules;

    std::list<String> bin_int_prefix; // prefix for binary integers
    std::list<String> oct_int_prefix; // prefix for octal integers
    std::list<String> hex_int_prefix; // prefix for hex integers
    std::list<String> kilo_int_delimiter; // delimiter which is allowed every three chars in an decimal integer
    bool allow_int_type_postfix = false; // allow an postfix for integers which defines the type
    std::list<String> kilo_float_delimiter; // delimiter which is allowed every three chars in an decimal floats
    std::list<String> fraction_delimiter; // usually the period before the fractional part of the float value
    std::list<String> expo_delimiter; // usually the "E"
    std::list<String> expo_positive;
    std::list<String> expo_negative;
    bool allow_float_type_postfix = false; // allow an postfix for floats which defines the type

    // TODO special statements
    // TODO OOP constructs
    // TODO control flow

    std::list<FunctionDefinition> fn_definitions; // Function syntax

    // TODO templates

    std::list<Operator> scope_access_op; // how other scopes may be accessed
    std::list<Operator> member_access_op; // how sub-elements may be accessed
    // TODO array access

    std::map<String, Operator> subtypes; // these subtypes may occur in type -> name pairs
    std::list<TraitOperator> operators; // all general operators
    std::list<Operator> reference_op; // used for borrowing
    std::list<Operator> type_of_op; // returns the type of a expression
    std::list<Operator> struct_to_tuple_op; // translates a struct into a tuple
    std::list<Operator> type_op; // special type defining operator
    std::list<RangeOperator> range_op; // any possible range

    std::list<std::pair<String, String>> type_literals; // literals which describe types
    std::list<std::pair<String, bool>> bool_literals; // literals which describe a boolean
    std::list<std::pair<String, i64>> int_literals; // literals which describe an integer
    std::list<std::pair<String, f64>> float_literals; // literals which describe an double
    std::list<std::pair<String, std::pair<i64, i64>>> range_literals; // literals which describe an range
};

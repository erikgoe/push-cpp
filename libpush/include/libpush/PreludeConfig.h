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

// Represents any list with type -> name pairs or just keywords.
using Syntax = std::list<std::pair<String, String>>;

constexpr u32 NO_BIAS_VALUE = 0;

// Defines syntax rules for any operator (binary or unary)
struct Operator {
    u32 precedence = 0; // how operators are combined
    bool ltr = true; // left to right or right to left
    bool ambiguous = false; // whether this operator has an ambiguous interpregation
    std::pair<u32, u32> prec_class = std::make_pair(
        UINT32_MAX, UINT32_MAX ); // precedence-update class to a path as class-from-pair (if not UINT32_MAX)
    u32 prec_bias = NO_BIAS_VALUE; // optional value to prefer one syntax over another despite the precedence
    Syntax syntax; // left type -> name pair
};

// Operator which is implementaed in a library through a trait
struct TraitOperator {
    Operator op;
    String fn; // function in the trait
};

// A function rule
struct FunctionDefinition {
    String function_trait;
    String function_fn; // function in the trait
    Operator op;
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

    bool spaces_bind_identifiers = false; // if space combines two identifiers
    IdentifierCase function_case = IdentifierCase::snake;
    IdentifierCase method_case = IdentifierCase::snake;
    IdentifierCase variable_case = IdentifierCase::snake;
    IdentifierCase module_case = IdentifierCase::snake;
    IdentifierCase struct_case = IdentifierCase::pascal;
    IdentifierCase trait_case = IdentifierCase::pascal;
    std::list<String> unused_prefix; // prefix for unused variables
    std::list<StringRule> string_rules;

    std::list<Operator> alias_bindings; // use keyword
    std::list<Operator> simple_bindings; // let keyword

    // OOP constructs
    std::list<Operator> structs; // struct definition
    std::list<Operator> trait; // struct definition
    std::list<Operator> impl; // struct definition

    // Control flow
    std::list<Operator> if_condition; // if keyword (without else)
    std::list<Operator> if_else_condition; // if keyword (with else)
    std::list<std::pair<Operator, bool>> pre_loop; // loop with a pre-condition (including the required evaluation)
    std::list<std::pair<Operator, bool>> post_loop; // loop with a post-condition (including the required evaluation)
    std::list<Operator> inf_loop; // infinite loop with no condition
    std::list<Operator> interator_loop; // loop which iterates over a range/collection
    std::list<Operator> matching; // pattern matching construct

    // Functions
    std::list<Operator> fn_head; // Function head syntax
    std::list<FunctionDefinition> fn_definitions; // Function syntax
    std::list<Operator> fn_call; // Function call

    // Special access
    String scope_access_operator; // THE scope access operator
    std::list<Operator> scope_access_op; // how other scopes may be accessed
    std::list<Operator> member_access_op; // how sub-elements may be accessed
    std::list<Operator> array_access_op; // access with a index

    // Special operations
    std::list<TraitOperator> operators; // all general operators
    std::list<Operator> comma_op; // comma-linking
    std::list<Operator> reference_op; // used for borrowing
    std::list<Operator> type_of_op; // returns the type of a expression
    std::list<Operator> struct_to_tuple_op; // translates a struct into a tuple
    std::list<Operator> type_op; // special type defining operator
    std::list<RangeOperator> range_op; // any possible range

    std::list<Operator> modules; // module specifier (mod keyword)
    std::list<Operator> declaration; // declaration syntax
    std::list<Operator> public_attr; // public attribute syntax
    std::list<Operator> static_statements; // static statement
    std::list<Operator> compiler_annotations; // compiler annotations
    std::list<Operator> macros; // macros
    std::list<Operator> unsafe; // unsafe blocks
    std::list<Operator> templates; // generic types

    String integer_trait; // Basic trait to define an integer
    String string_trait; // Basic trait to define a string
    String implication_trait; // Basic trait to define an implication
    String never_trait; // Basic trait to define the never type
    std::map<String, String> special_types; // maps special type keywords/operators to their meaning
    std::map<String, u8> memblob_types; // maps type names to their memory size
    std::map<String, std::pair<String, u64>> literals; // each literal keyword is mapped to its type and memory_value
};

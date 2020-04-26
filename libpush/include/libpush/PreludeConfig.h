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
using Syntax = std::vector<std::pair<String, String>>;

constexpr u32 NO_BIAS_VALUE = 0;

// Defines syntax rules for any operator (binary or unary)
struct Operator {
    u32 precedence = 0; // how operators are combined
    bool ltr = true; // left to right or right to left
    bool ambiguous = false; // whether this operator has an ambiguous interpregation
    std::pair<u32, u32> prec_class = std::make_pair(
        UINT32_MAX, UINT32_MAX ); // precedence-update class to a path as class-from-pair (if not UINT32_MAX)
    u32 prec_bias = NO_BIAS_VALUE; // optional value to prefer one syntax over another despite the precedence

    Syntax syntax; // type -> name pairs

    String fn; // function to call for this operator
    // Defines which range it describes
    enum class RangeOperatorType {
        exclude,
        exclude_from,
        exclude_to,
        include,
        include_to,

        count
    } range = RangeOperatorType::count;
};

// Defines available types of syntaxes
enum class SyntaxType {
    op, // general operator syntax
    scope_access,
    module_spec,
    member_access,
    array_access,
    func_head,
    func_def,
    macro,
    annotation,
    unsafe_block,
    static_statement,
    reference_attr,
    mutable_attr,
    typed,
    type_of,
    range,
    assignment,
    implication,
    decl_attr,
    public_attr,
    comma,
    structure,
    trait,
    implementation,
    simple_binding,
    alias_binding,
    if_cond,
    if_else,
    pre_cond_loop_continue,
    pre_cond_loop_abort,
    post_cond_loop_continue,
    post_cond_loop_abort,
    inf_loop,
    itr_loop,
    match,
    template_postfix,

    count
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
    std::vector<String> unused_prefix; // prefix for unused variables
    std::vector<StringRule> string_rules;

    std::map<SyntaxType, std::vector<Operator>> syntaxes; // available syntaxes
    String scope_access_operator; // THE scope access operator (for easier access)

    String integer_trait; // Basic trait to define an integer
    String string_trait; // Basic trait to define a string
    String implication_trait; // Basic trait to define an implication
    String never_trait; // Basic trait to define the never type
    std::map<String, String> special_types; // maps special type keywords/operators to their meaning
    std::map<String, u8> memblob_types; // maps type names to their memory size
    std::map<String, std::pair<String, u64>> literals; // each literal keyword is mapped to its type and memory_value
};

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

// Identifies a type
using TypeId = u32;

// Identifies a symbol
using SymbolId = u32;
constexpr SymbolId ROOT_SYMBOL = 1; // the global root symbol

// Identifies a function body
using FunctionBodyId = u32;

// Constants
constexpr TypeId TYPE_UNIT = 1; // The initial unit type
constexpr TypeId TYPE_NEVER = 2; // The initial never type
constexpr TypeId TYPE_TYPE = 3; // The initial type type
constexpr TypeId MODULE_TYPE = 4; // The initial module type
constexpr TypeId LAST_FIX_TYPE = MODULE_TYPE; // The last not variable type

class Expr;

// Checks if a token list matches a specific expression and translates it
struct SyntaxRule {
    u32 precedence = 0; // precedence of this syntax matching
    bool ltr = true; // associativity
    bool ambiguous = false; // whether this symtax has an ambiguous interpregation
    std::pair<u32, u32> prec_class = std::make_pair(
        UINT32_MAX, UINT32_MAX ); // precedence-update class to a path as class-from-pair (if not UINT32_MAX)
    u32 prec_bias = NO_BIAS_VALUE; // optional value to prefer one syntax over another despite the precedence
    std::vector<sptr<Expr>> expr_list; // list which has to be matched against

    // Checks if a reversed expression list matches this syntax rule
    bool matches_reversed( std::vector<sptr<Expr>> &rev_list );

    // Create a new expression according to this rule.
    std::function<sptr<Expr>( std::vector<sptr<Expr>> &, Worker &w_ctx )> create;
};

// Maps syntax item labels to their position in a syntax
using LabelMap = std::map<String, size_t>;

using TypeMemSize = u64; // stores size of a type in bytes

// Represents the value of an expression which had been evaluated at compile time
struct ConstValue {
    u8 *data; // the data blob of the value
    size_t data_size; // size of the data array
};

// Identifies a local symbol (must be chained to get a full symbol identification)
struct SymbolIdentifier {
    String name; // the local symbol name (empty means anonymous scope)

    TypeId eval_type = 0; // the type which is returned when the symbol is evaluated (return type of functions)
    std::vector<std::pair<TypeId, String>> parameters; // type-name pairs of parameters

    std::vector<std::pair<TypeId, ConstValue>> template_values; // type-value pairs of template parameters
};

// Used to substitute symbol paths
struct SymbolSubstitution {
    sptr<std::vector<SymbolIdentifier>> from, to;
};

// A node in the Symbol graph, representing a symbol
struct SymbolGraphNode {
    SymbolId parent = 0; // parent of this graph node
    std::vector<SymbolId> sub_nodes; // children of this graph node
    std::vector<sptr<Expr>> original_expr; // Expressions which define this symbol

    SymbolIdentifier identifier; // identifies this symbol (may be partially defined)
    std::vector<std::pair<TypeId, String>> template_params; // type-name pairs of template parameters
    bool pub = false; // whether this symbol is public

    TypeId value = 0; // type/value of this symbol (every function has its own type; for structs this is struct body;
                      // for (local) variables this is 0)
    TypeId type = 0; // the type behind the value of this symbol
};

// An entry in the type table, representing a type
struct TypeTableEntry {
    SymbolId symbol = 0;

    TypeMemSize additional_mem_size = 0; // additional blob of memory bytes (e. g. for primitive types)
    std::vector<SymbolGraphNode> members; // list of members of this type (not pointers)
    std::vector<TypeId> supertypes; // basically traits
    std::vector<TypeId> subtypes; // the types which implement this trait (so this must be a trait)

    FunctionBodyId function_body = 0; // the function body, if it's a function
};

// Represents the content of a function
struct FunctionBody {
    TypeId type = 0;
};


// Contains context while building the crate
struct CrateCtx {
    sptr<Expr> ast; // the current Abstract Syntax Tree
    std::vector<SymbolGraphNode> symbol_graph; // contains all graph nodes, idx 0 is the global root node
    std::vector<TypeTableEntry> type_table; // contains all types
    std::vector<FunctionBody> functions; // contains all function implementations (MIR)

    TypeId struct_type = 0; // internal struct type
    TypeId trait_type = 0; // internal trait type
    TypeId fn_type = 0; // internal function type
    TypeId mod_type = 0; // internal module type
    TypeId int_type = 0; // type of the integer trait
    TypeId str_type = 0; // type of the string trait

    SymbolId current_scope = ROOT_SYMBOL; // new symbols are created on top of this one
    std::vector<std::vector<SymbolSubstitution>> current_substitutions; // Substitution rules for each new scope

    std::unordered_map<String, std::pair<TypeId, u64>> literals_map; // maps literals to their typeid and mem_value

    std::vector<SyntaxRule> rules;

    CrateCtx() {
        symbol_graph.resize( 2 );
        type_table.resize( LAST_FIX_TYPE + 1 );
        functions.resize( 2 );
    }
};

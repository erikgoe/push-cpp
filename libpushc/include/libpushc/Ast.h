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
#include "libpushc/Expression.h"

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


// A node in the Symbol graph, representing a symbol
struct SymbolGraphNode {
    SymbolId parent = 0;
    std::vector<SymbolId> sub_nodes;
    TypeId type = 0;
    String name;
    bool pub = false;
};

// A entry in the type table, representing a type
struct TypeTableEntry {
    SymbolId symbol = 0;
    TypeMemSize additional_mem_size = 0; // additional blob of memory bytes (e. g. for primitive types)
    std::vector<SymbolGraphNode> members; // list of members of this type (not pointers)
    std::vector<TypeId> subtypes; // basically traits
    FunctionBodyId function_body = 0;
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

    TypeId int_type = 0; // type of the integer trait
    TypeId str_type = 0; // type of the string trait

    SymbolId current_scope = 1; // new symbols are created on top of this one

    std::unordered_map<String, std::pair<TypeId, u64>> literals_map; // maps literals to their typeid and mem_value

    std::vector<SyntaxRule> rules;

    CrateCtx() {
        symbol_graph.resize( 2 );
        type_table.resize( LAST_FIX_TYPE + 1 );
        functions.resize( 2 );
    }
};

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
#include "libpushc/Expression.h"

// Checks if a token list matches a specific expression and translates it
struct SyntaxRule {
    u32 precedence = 0; // precedence of this syntax matching
    bool ltr = true; // associativity
    std::vector<sptr<Expr>> expr_list; // list which has to be matched against
    sptr<Expr> matching_expr; // the expression to translate into

    // Checks if the end of a expression list matches this syntax rule (excluding the first element)
    bool end_matches( std::vector<sptr<Expr>> &list );

    // Checks if the first element of the syntax rule matches a element
    bool front_matches( sptr<Expr> &front );

    // Create a new expression according to this rule.
    std::function<sptr<Expr>( std::vector<sptr<Expr>> & )> create;
};

// Maps syntax item labels to their position in a syntax
using LabelMap = std::map<String, size_t>;

using TypeMemSize = u64; // stores how many bytes a type has in memory

// Contains information about a type
struct TypeInfo {
    TypeId id = 0;
    SymbolId symbol = 0;
    TypeMemSize mem_size;
};

// Contains information about a symbol
struct SymbolInfo {
    SymbolId id = 0;
    std::vector<String> name_chain;
};

// Abstract Syntax Tree
struct Ast {
    sptr<Expr> block; // global block
    std::vector<TypeInfo> type_map; // Maps typeids to their data
    std::vector<SymbolInfo> symbol_map; // Maps symbolids to their data
};

// Contains context while building the ast
struct AstCtx {
    Ast ast; // the current ast (so far)

    SymbolInfo next_symbol; // contains next id and current name_chain
    TypeId next_type = LAS_FIX_TYPE + 1; // the next type id

    TypeId unit_type = LAS_FIX_TYPE;
    TypeId int_type = 0; // type of the integer trait
    TypeId str_type = 0; // type of the string trait

    std::unordered_map<String, std::pair<TypeId, u64>> literals_map; // maps literals to their typeid and mem_value

    std::unordered_map<std::vector<String>, TypeId> type_id_map; // maps symbol chains to their typeid

    std::vector<SyntaxRule> rules;
};

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
    u32 precedence; // precedence of this syntax matching
    std::vector<sptr<Expr>> expr_list; // list which has to be matched against
    sptr<Expr> matching_expr; // the expression to translate into

    // Checks if the end of a expression list matches this syntax rule
    bool matches_end( std::vector<sptr<Expr>> &list );

    // Create a new expression according to this rule.
    std::function<sptr<Expr>( std::vector<sptr<Expr>> & )> create;
};

// Contains information about a type
struct TypeInfo {
    TypeId id = 0;
    SymbolId symbol = 0;
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
    TypeId next_type = TYPE_UNIT + 1; // the next type id

    std::vector<SyntaxRule> rules;
};
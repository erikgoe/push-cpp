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

// Identifies a type
using TypeId = u32;

// Identifies a symbol
using SymbolId = u32;

// Constants
constexpr TypeId TYPE_UNIT = 0;

// Base class for expressions in the AST
class Expr {
public:
    virtual ~Expr() {}

    // Get the return type of the expression
    virtual TypeId get_type() = 0;
};

// A Block with multiple expressions
class BlockExpr : public Expr {
    std::vector<sptr<Expr>> sub_expr;

public:
    TypeId get_type() { return sub_expr.empty() ? TYPE_UNIT : sub_expr.back()->get_type(); }
};

// A simple symbol/identifier (variable, function, etc.)
class SymbolExpr : public Expr {
    TypeId type;
    SymbolId symbol;

public:
    TypeId get_type() { return type; }
};

// Base class for a simple literal
class LiteralExpr : public Expr {};

// A literal type with a trivial memory layout
template <u8 Bytes, TypeId type>
class BlobLiteralExpr : public LiteralExpr {
    std::array<u8, Bytes> blob;

public:
    TypeId get_type() { return type; }
};

// Specifies a new funcion
class FuncExpr : public Expr {
    TypeId type; // Every funcion has its own type
    sptr<SymbolExpr> symbol;
    sptr<BlockExpr> body;

public:
    TypeId get_type() { return type; }
};

// Assigns a rvalue to a lvalue
class AssignExpr : public Expr {
    sptr<Expr> lvalue, rvalue;

public:
    TypeId get_type() { return lvalue->get_type(); }
};

// Specifies a new variable binding without anything extra
class SimpleBindExpr : public Expr {
    sptr<AssignExpr> assign;

public:
    TypeId get_type() { return TYPE_UNIT; }
};

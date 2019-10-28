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
constexpr TypeId TYPE_UNIT = 1;

// Base class for expressions in the AST
class Expr {
public:
    virtual ~Expr() {}

    // Get the return type of the expression
    virtual TypeId get_type() { return 0; };

    // Checks if matches the expression
    virtual bool matches( sptr<Expr> other ) { return std::dynamic_pointer_cast<Expr>( other ) != nullptr; }

    virtual String get_debug_repr() { return "EXPR"; }
};

// Used internally to handle a single token as expr. Must be resolved to other expressions
class TokenExpr : public Expr {
public:
    Token t;

    TokenExpr( const Token &token ) : t( token ) {}

    // Has no type because it is not a real AST node
    TypeId get_type() override { return 0; }

    bool matches( sptr<Expr> other ) override {
        auto o = std::dynamic_pointer_cast<TokenExpr>( other );
        return o != nullptr && t.content == o->t.content;
    }

    String get_debug_repr() override { return "TOKEN \"" + t.content + "\""; }
};

// Normally the global scope as root-expression
class DeclExpr : public Expr {
public:
    std::vector<sptr<Expr>> sub_expr;

    bool matches( sptr<Expr> other ) override { return std::dynamic_pointer_cast<DeclExpr>( other ) != nullptr; }

    String get_debug_repr() override {
        String str = "GLOBAL { ";
        for ( auto &s : sub_expr )
            str += s->get_debug_repr() + ", ";
        return str + " }";
    }
};

// A Block with multiple expressions
class BlockExpr : public Expr {
public:
    std::vector<sptr<Expr>> sub_expr;

    TypeId get_type() override { return sub_expr.empty() ? TYPE_UNIT : sub_expr.back()->get_type(); }

    bool matches( sptr<Expr> other ) override { return std::dynamic_pointer_cast<BlockExpr>( other ) != nullptr; }

    String get_debug_repr() override {
        String str = "BLOCK { ";
        for ( auto &s : sub_expr )
            str += s->get_debug_repr() + ", ";
        return str + " }";
    }
};

// A simple symbol/identifier (variable, function, etc.)
class SymbolExpr : public Expr {
public:
    TypeId type;
    SymbolId symbol;

    TypeId get_type() override { return type; }

    bool matches( sptr<Expr> other ) override { return std::dynamic_pointer_cast<SymbolExpr>( other ) != nullptr; }

    String get_debug_repr() override { return "SYM(" + to_string( symbol ) + ")"; }
};

// Base class for a simple literal
class LiteralExpr : public Expr {};

// A literal type with a trivial memory layout
template <u8 Bytes, TypeId type>
class BlobLiteralExpr : public LiteralExpr {
    std::array<u8, Bytes> blob;

public:
    TypeId get_type() override { return type; }

    bool matches( sptr<Expr> other ) override { return std::dynamic_pointer_cast<LiteralExpr>( other ) != nullptr; }

    String get_debug_repr() override {
        String str = "BLOB_LITERAL(";
        for ( auto &b : blob )
            str += to_string( b->get_debug_repr() ) + "-"; // TODO hex representation
        return str + ")";
    }
};

// An expression which can be broken into multiple sub-expressions by other rvalues/operators
class SeparableExpr : public Expr {
protected:
    std::vector<sptr<Expr>> original_list;

public:
    virtual ~SeparableExpr() {}

    // Separate the expression into its parts
    const std::vector<sptr<Expr>> &split() { return original_list; };
    // Returns the precedence of this Expression binding. Lower values bind stronger
    virtual u32 prec() { return 0; }

    virtual bool matches( sptr<Expr> other ) override {
        return std::dynamic_pointer_cast<SeparableExpr>( other ) != nullptr;
    }
};

// Specifies a new funcion signature
class FuncDecExpr : public SeparableExpr {
    TypeId type; // Every funcion has its own type
    sptr<SymbolExpr> symbol;

public:
    FuncDecExpr() {}
    FuncDecExpr( sptr<SymbolExpr> symbol, TypeId type, std::vector<sptr<Expr>> &original_list ) {
        this->symbol = symbol;
        this->type = type;
        this->original_list = original_list;
    }

    TypeId get_type() override { return type; }

    bool matches( sptr<Expr> other ) override { return std::dynamic_pointer_cast<FuncDecExpr>( other ) != nullptr; }

    String get_debug_repr() override { return "FUNC_DEC(" + to_string( type ) + " " + symbol->get_debug_repr() + ")"; }
};

// Specifies a new funcion
class FuncExpr : public SeparableExpr {
    TypeId type; // Every funcion has its own type
    sptr<SymbolExpr> symbol;
    sptr<BlockExpr> body;

public:
    FuncExpr() {}
    FuncExpr( sptr<SymbolExpr> symbol, TypeId type, sptr<BlockExpr> block, std::vector<sptr<Expr>> &original_list ) {
        this->symbol = symbol;
        this->type = type;
        body = block;
        this->original_list = original_list;
    }

    TypeId get_type() override { return type; }

    bool matches( sptr<Expr> other ) override { return std::dynamic_pointer_cast<FuncExpr>( other ) != nullptr; }
    String get_debug_repr() override {
        return "FUNC(" + to_string( type ) + " " + symbol->get_debug_repr() + " " + body->get_debug_repr() + ")";
    }
};

class OperatorExpr : public SeparableExpr {
protected:
    sptr<Expr> lvalue, rvalue;
    String op;
    u32 precedence = 0;

public:
    OperatorExpr() {}
    OperatorExpr( String op, sptr<Expr> lvalue, sptr<Expr> rvalue, u32 precedence,
                  std::vector<sptr<Expr>> &original_list ) {
        this->op = op;
        this->lvalue = lvalue;
        this->rvalue = rvalue;
        this->precedence = precedence;
        this->original_list = original_list;
    }

    TypeId get_type() override { return lvalue->get_type(); }

    bool matches( sptr<Expr> other ) override { return std::dynamic_pointer_cast<OperatorExpr>( other ) != nullptr; }

    u32 prec() override { return precedence; }

    String get_debug_repr() override {
        return "OP(" + lvalue->get_debug_repr() + " " + op + " " + rvalue->get_debug_repr() + ")";
    }
};

// Assigns a rvalue to a lvalue
class AssignExpr : public OperatorExpr {
public:
    bool matches( sptr<Expr> other ) override { return std::dynamic_pointer_cast<AssignExpr>( other ) != nullptr; }
};

// Specifies a new variable binding without doing anything with it
class SimpleBindExpr : public SeparableExpr {
    sptr<AssignExpr> assign;

public:
    TypeId get_type() override { return TYPE_UNIT; }

    bool matches( sptr<Expr> other ) override { return std::dynamic_pointer_cast<SimpleBindExpr>( other ) != nullptr; }

    String get_debug_repr() override { return "BINDING(" + assign->get_debug_repr() + ")"; }
};

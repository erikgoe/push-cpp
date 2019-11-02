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
#include "libpushc/Util.h"

// Identifies a type
using TypeId = u32;

// Identifies a symbol
using SymbolId = u32;

// Constants
constexpr TypeId TYPE_UNIT = 1; // The initial unit type

// Base class for expressions in the AST
class Expr {
public:
    PosInfo pos_info;

    virtual ~Expr() {}

    // Get the return type of the expression
    virtual TypeId get_type() { return 0; };

    // Checks if matches the expression
    virtual bool matches( sptr<Expr> other ) { return std::dynamic_pointer_cast<Expr>( other ) != nullptr; }

    virtual String get_debug_repr() { return "EXPR"; }

    // Returns the position information of this expression
    virtual PosInfo get_position_info() { return pos_info; }
};

// Used internally to handle a single token as expr. Must be resolved to other expressions
class TokenExpr : public Expr {
public:
    Token t;

    TokenExpr( const Token &token ) : t( token ) { pos_info = PosInfo{ t.file, t.line, t.column, t.length }; }

    // Has no type because it is not a real AST node
    TypeId get_type() override { return 0; }

    bool matches( sptr<Expr> other ) override {
        auto o = std::dynamic_pointer_cast<TokenExpr>( other );
        return o != nullptr && t.content == o->t.content;
    }

    String get_debug_repr() override {
        return "TOKEN " + to_string( static_cast<int>( t.type ) ) + " \"" + t.content + "\"";
    }
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

// A block or semicolon-terminated expression
class CompletedExpr : public Expr {
public:
    bool matches( sptr<Expr> other ) override { return std::dynamic_pointer_cast<CompletedExpr>( other ) != nullptr; }

    String get_debug_repr() override { return "COMPLETED"; }
};

// A semicolon-terminated expression
class SingleCompletedExpr : public CompletedExpr {
public:
    sptr<Expr> sub_expr;

    TypeId get_type() override { return sub_expr->get_type(); }

    bool matches( sptr<Expr> other ) override {
        return std::dynamic_pointer_cast<SingleCompletedExpr>( other ) != nullptr;
    }

    String get_debug_repr() override { return "SC " + sub_expr->get_debug_repr() + ";\n "; }
};

// A comma-separated expression
class SingleListedExpr : public CompletedExpr {
public:
    sptr<Expr> sub_expr;

    TypeId get_type() override { return sub_expr->get_type(); }

    bool matches( sptr<Expr> other ) override {
        return std::dynamic_pointer_cast<SingleListedExpr>( other ) != nullptr;
    }

    String get_debug_repr() override { return "SL " + sub_expr->get_debug_repr() + ", "; }
};

// A block with multiple expressions
class BlockExpr : public CompletedExpr {
public:
    std::vector<sptr<Expr>> sub_expr;

    TypeId get_type() override {
        if ( sub_expr.empty() || !std::dynamic_pointer_cast<SingleCompletedExpr>( sub_expr.back() ) ) {
            return TYPE_UNIT;
        } else {
            return sub_expr.back()->get_type();
        }
    }

    bool matches( sptr<Expr> other ) override { return std::dynamic_pointer_cast<BlockExpr>( other ) != nullptr; }

    String get_debug_repr() override {
        String str = "BLOCK { ";
        for ( auto &s : sub_expr )
            str += s->get_debug_repr() + ", ";
        return str + " }";
    }
};

// A tuple with multiple elements
class TupleExpr : public Expr {
public:
    std::vector<sptr<Expr>> sub_expr;
    TypeId type = 0;

    TypeId get_type() override { return type; }

    bool matches( sptr<Expr> other ) override { return std::dynamic_pointer_cast<TupleExpr>( other ) != nullptr; }

    String get_debug_repr() override {
        String str = "TUPLE ( ";
        for ( auto &s : sub_expr )
            str += s->get_debug_repr();
        return str + " )";
    }
};

// A term with a sub expressions
class TermExpr : public Expr {
public:
    sptr<Expr> sub_expr;

    TypeId get_type() override { return sub_expr->get_type(); }

    bool matches( sptr<Expr> other ) override { return std::dynamic_pointer_cast<TermExpr>( other ) != nullptr; }

    String get_debug_repr() override { return "TERM( " + sub_expr->get_debug_repr() + " )"; }
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

// Base class for all Blob literals
class BasicBlobLiteralExpr : public Expr {
public:
    virtual bool matches( sptr<Expr> other ) override {
        return std::dynamic_pointer_cast<BasicBlobLiteralExpr>( other ) != nullptr;
    }
};

// A literal type with a trivial memory layout
template <u8 Bytes>
class BlobLiteralExpr : public BasicBlobLiteralExpr {
public:
    std::array<u8, Bytes> blob;
    TypeId type;

    TypeId get_type() override { return type; }

    bool matches( sptr<Expr> other ) override {
        return std::dynamic_pointer_cast<BlobLiteralExpr<Bytes>>( other ) != nullptr;
    }

    String get_debug_repr() override {
        bool non_zero = false;
        String str = "BLOB_LITERAL(";
        for ( auto b = blob.rbegin(); b != blob.rend(); b++ ) {
            non_zero = non_zero || *b != 0;
            if ( non_zero ) {
                u8 nibble = *b >> 4;
                str += nibble < 10 ? nibble + '0' : nibble + 'a' - 10;
                nibble = *b & 0xf;
                str += nibble < 10 ? nibble + '0' : nibble + 'a' - 10;
            }
        }
        if ( !non_zero )
            str += "00";
        return str + ":" + to_string( type ) + ")";
    }

    // Loads the blob with a low endian representation of a number
    void load_from_number( const Number &num, u8 max_mem_size = Bytes ) {
        max_mem_size = std::min( max_mem_size, static_cast<u8>( sizeof( Number ) ) );
        const u8 *tmp = reinterpret_cast<const u8 *>( &num );
        for ( size_t i = 0; i < max_mem_size; i++ )
            blob[i] = tmp[i];
    }
};

// A literal containing a string
class StringLiteralExpr : public LiteralExpr {
public:
    String str;
    TypeId type;

    TypeId get_type() override { return type; }

    bool matches( sptr<Expr> other ) override {
        return std::dynamic_pointer_cast<StringLiteralExpr>( other ) != nullptr;
    }

    String get_debug_repr() override { return "STR \"" + str + "\""; }
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

    PosInfo get_position_info() override { return original_list.front()->get_position_info(); }
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
    sptr<TupleExpr> parameters;
    sptr<SymbolExpr> symbol;
    sptr<CompletedExpr> body;

public:
    FuncExpr() {}
    FuncExpr( sptr<SymbolExpr> symbol, TypeId type, sptr<TupleExpr> parameters, sptr<CompletedExpr> block,
              std::vector<sptr<Expr>> &original_list ) {
        this->symbol = symbol;
        this->type = type;
        this->parameters = parameters;
        body = block;
        this->original_list = original_list;
    }

    TypeId get_type() override { return type; }

    bool matches( sptr<Expr> other ) override { return std::dynamic_pointer_cast<FuncExpr>( other ) != nullptr; }
    String get_debug_repr() override {
        return "FUNC(" + to_string( type ) + " " + ( parameters ? parameters->get_debug_repr() + " " : "" ) +
               symbol->get_debug_repr() + " " + body->get_debug_repr() + ")";
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
        return "OP(" + ( lvalue ? lvalue->get_debug_repr() + " " : "" ) + op +
               ( rvalue ? " " + rvalue->get_debug_repr() : "" ) + ")";
    }
};

// Specifies a new variable binding without doing anything with it
class SimpleBindExpr : public SeparableExpr {
protected:
    u32 precedence = 0;

public:
    sptr<Expr> expr;

    SimpleBindExpr() {}
    SimpleBindExpr( sptr<Expr> expr, u32 precedence, std::vector<sptr<Expr>> &original_list ) {
        this->expr = expr;
        this->precedence = precedence;
        this->original_list = original_list;
    }

    TypeId get_type() override { return TYPE_UNIT; }

    bool matches( sptr<Expr> other ) override { return std::dynamic_pointer_cast<SimpleBindExpr>( other ) != nullptr; }

    u32 prec() override { return precedence; }

    String get_debug_repr() override { return "BINDING(" + expr->get_debug_repr() + ")"; }
};

// Specifies a new symbol alias
class AliasBindExpr : public SeparableExpr {
protected:
    u32 precedence = 0;

public:
    sptr<Expr> expr;

    AliasBindExpr() {}
    AliasBindExpr( sptr<Expr> expr, u32 precedence, std::vector<sptr<Expr>> &original_list ) {
        this->expr = expr;
        this->precedence = precedence;
        this->original_list = original_list;
    }

    TypeId get_type() override { return TYPE_UNIT; }

    bool matches( sptr<Expr> other ) override { return std::dynamic_pointer_cast<AliasBindExpr>( other ) != nullptr; }

    u32 prec() override { return precedence; }

    String get_debug_repr() override { return "ALIAS(" + expr->get_debug_repr() + ")"; }
};

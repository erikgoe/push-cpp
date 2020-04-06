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
#include "libpushc/Util.h"
#include "libpushc/Ast.h"
#include "libpushc/MirTranslation.h"

// Defines the type of a visitor pass
enum class VisitorPassType {
    BASIC_SEMANTIC_CHECK, // checks some basic semantic requirements for each expression
    FIRST_TRANSFORMATION, // transformations which can be done without symbol information
    SYMBOL_DISCOVERY, // discover all symbols in the global declarative scope
    SECOND_TRANSFORMATION, // transformations which require with symbol information

    count
};

class SymbolExpr;
class PublicAttrExpr;
class StaticStatementExpr;

// Dispatcher for preparations for visitor passes
template <typename T>
bool visit_impl( CrateCtx &c_ctx, Worker &w_ctx, VisitorPassType vpt, T &expr, sptr<Expr> &anchor, sptr<Expr> parent );

// Dispatcher for visitor passes. Returns false if the pass failed
template <typename T>
bool post_visit_impl( CrateCtx &c_ctx, Worker &w_ctx, VisitorPassType vpt, T &expr, sptr<Expr> &anchor,
                      sptr<Expr> parent );

// Creates a symbol chain from an expression which contains symbols or scoped symbols
sptr<std::vector<SymbolIdentifier>> get_symbol_chain_from_expr( sptr<SymbolExpr> expr );

#include "Expression.inl"

// Base class for expressions in the AST
class Expr : public std::enable_shared_from_this<Expr> {
public:
    PosInfo pos_info;
    std::vector<sptr<Expr>> static_statements;
    std::vector<sptr<Expr>> annotations;

    virtual ~Expr() {}

    // AST methods

    // Get the return type of the expression
    virtual TypeId get_type() { return 0; };

    // Checks if matches the expression
    virtual bool matches( sptr<Expr> other ) { return std::dynamic_pointer_cast<Expr>( other ) != nullptr; }

    // Symbol methods

    // Copies data common to all exprs from another expr
    virtual void copy_from_other( sptr<Expr> other ) {
        pos_info = other->pos_info;
        static_statements = other->static_statements;
        annotations = other->annotations;
    }

    // This method is used to minimize the needed code to implement a visitor pattern using templates (see
    // post_visit_impl()). Returns false if the pass failed
    virtual bool visit( CrateCtx &c_ctx, Worker &w_ctx, VisitorPassType vpt, sptr<Expr> &anchor,
                        sptr<Expr> parent ) = 0;

    // Checks very basic semantic conditions on an expr. Returns false when an error has been found
    virtual bool basic_semantic_check( CrateCtx &c_ctx, Worker &w_ctx ) { return true; }

    // Does basic transformations, which don't require symbol information
    virtual bool first_transformation( CrateCtx &c_ctx, Worker &w_ctx, sptr<Expr> &anchor, sptr<Expr> parent ) {
        return true;
    }

    // Prepares the symbol discovery for this expr
    virtual bool symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) { return true; }

    // Used in the symbol discovery pass
    virtual bool post_symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) { return true; }

    // Does basic transformations, which require symbol information
    virtual bool second_transformation( CrateCtx &c_ctx, Worker &w_ctx ) { return true; }

    // Mir methods

    // Parses the expr and appends the generated instructions to the function. Returns the result variable
    virtual MirVarId parse_mir( CrateCtx &c_ctx, Worker &w_ctx, FunctionImplId func ) {
        LOG_ERR( "Not implemented!" );
        return 0;
    }

    // Debugging & message methods

    // Returns a string representation of this ast node for debugging purposes
    virtual String get_debug_repr() { return "EXPR"; }

    // Returns additional information like static statements
    String get_additional_debug_data();

    // Returns the position information of this expression
    virtual PosInfo get_position_info() { return pos_info; }
};

// Nearly every expression is also an OperandExpr. Exceptions are SingleCompletedExpr.
class OperandExpr : public virtual Expr {
public:
    bool matches( sptr<Expr> other ) override { return std::dynamic_pointer_cast<OperandExpr>( other ) != nullptr; }

    bool visit( CrateCtx &c_ctx, Worker &w_ctx, VisitorPassType vpt, sptr<Expr> &anchor, sptr<Expr> parent ) override {
        bool result = visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
        if ( anchor != shared_from_this() ) // replaced itself (object is now invalid)
            return anchor->visit( c_ctx, w_ctx, vpt, anchor, parent );
        return result && post_visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
    }
};

// Used internally to handle a single token as expr. Must be resolved to other expressions
class TokenExpr : public Expr {
public:
    Token t;

    TokenExpr( const Token &token ) : t( token ) { pos_info = PosInfo{ t.file, t.line, t.column, t.length }; }

    // Has no type because it is not a real AST node
    TypeId get_type() override { return 0; }

    bool visit( CrateCtx &c_ctx, Worker &w_ctx, VisitorPassType vpt, sptr<Expr> &anchor, sptr<Expr> parent ) override {
        bool result = visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
        if ( anchor != shared_from_this() ) // replaced itself (object is now invalid)
            return anchor->visit( c_ctx, w_ctx, vpt, anchor, parent );
        return result && post_visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
    }

    bool basic_semantic_check( CrateCtx &c_ctx, Worker &w_ctx ) override;

    bool matches( sptr<Expr> other ) override {
        auto o = std::dynamic_pointer_cast<TokenExpr>( other );
        return o != nullptr && t.content == o->t.content;
    }

    String get_debug_repr() override {
        return "TOKEN " + to_string( static_cast<int>( t.type ) ) + " \"" + t.content + "\" " +
               get_additional_debug_data();
    }
};

// Normally the global scope as root-expression
class DeclExpr : public Expr {
public:
    std::vector<sptr<Expr>> sub_expr;
    std::vector<SymbolSubstitution> substitutions;

    bool matches( sptr<Expr> other ) override { return std::dynamic_pointer_cast<DeclExpr>( other ) != nullptr; }

    bool visit( CrateCtx &c_ctx, Worker &w_ctx, VisitorPassType vpt, sptr<Expr> &anchor, sptr<Expr> parent ) override {
        bool result = visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
        if ( anchor != shared_from_this() ) // replaced itself (object is now invalid)
            return anchor->visit( c_ctx, w_ctx, vpt, anchor, parent );
        for ( auto &e : sub_expr ) {
            if ( !e->visit( c_ctx, w_ctx, vpt, e, shared_from_this() ) )
                result = false;
        }
        return result && post_visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
    }

    bool basic_semantic_check( CrateCtx &c_ctx, Worker &w_ctx ) override;

    bool first_transformation( CrateCtx &c_ctx, Worker &w_ctx, sptr<Expr> &anchor, sptr<Expr> parent ) override;

    bool symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) override;

    bool post_symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) override;

    String get_debug_repr() override {
        String str = "GLOBAL {\n ";
        for ( auto &s : sub_expr )
            str += s->get_debug_repr() + "\n ";
        return str + " }" + get_additional_debug_data();
    }

    PosInfo get_position_info() override {
        return merge_pos_infos( sub_expr.front()->get_position_info(), sub_expr.back()->get_position_info() );
    }
};

// A block or semicolon-terminated expression
class CompletedExpr : public virtual Expr {
public:
    bool matches( sptr<Expr> other ) override { return std::dynamic_pointer_cast<CompletedExpr>( other ) != nullptr; }

    bool visit( CrateCtx &c_ctx, Worker &w_ctx, VisitorPassType vpt, sptr<Expr> &anchor, sptr<Expr> parent ) override {
        bool result = visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
        if ( anchor != shared_from_this() ) // replaced itself (object is now invalid)
            return anchor->visit( c_ctx, w_ctx, vpt, anchor, parent );
        return result && post_visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
    }
};

// An expression which can create some kind of list
class ListedExpr : public virtual Expr {
public:
    virtual std::vector<sptr<Expr>> get_list() = 0;
};

// A semicolon-terminated expression
class SingleCompletedExpr : public CompletedExpr, public ListedExpr {
public:
    sptr<Expr> sub_expr;

    TypeId get_type() override { return sub_expr->get_type(); }

    bool matches( sptr<Expr> other ) override {
        return std::dynamic_pointer_cast<SingleCompletedExpr>( other ) != nullptr;
    }

    bool visit( CrateCtx &c_ctx, Worker &w_ctx, VisitorPassType vpt, sptr<Expr> &anchor, sptr<Expr> parent ) override {
        bool result = visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
        if ( anchor != shared_from_this() ) // replaced itself (object is now invalid)
            return anchor->visit( c_ctx, w_ctx, vpt, anchor, parent );
        return result && sub_expr->visit( c_ctx, w_ctx, vpt, sub_expr, shared_from_this() ) &&
               post_visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
    }

    bool basic_semantic_check( CrateCtx &c_ctx, Worker &w_ctx ) override;

    bool first_transformation( CrateCtx &c_ctx, Worker &w_ctx, sptr<Expr> &anchor, sptr<Expr> parent ) override;

    String get_debug_repr() override { return "SC " + sub_expr->get_debug_repr() + ";" + get_additional_debug_data(); }

    PosInfo get_position_info() override { return merge_pos_infos( sub_expr->get_position_info(), pos_info ); }

    std::vector<sptr<Expr>> get_list() override {
        if ( auto sub_listed = std::dynamic_pointer_cast<ListedExpr>( sub_expr ); sub_listed != nullptr ) {
            return sub_listed->get_list();
        } else {
            return std::vector<sptr<Expr>>( 1, sub_expr );
        }
    }
};

// A block with multiple expressions
class BlockExpr : public OperandExpr, public CompletedExpr, public ListedExpr {
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

    bool visit( CrateCtx &c_ctx, Worker &w_ctx, VisitorPassType vpt, sptr<Expr> &anchor, sptr<Expr> parent ) override {
        bool result = visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
        if ( anchor != shared_from_this() ) // replaced itself (object is now invalid)
            return anchor->visit( c_ctx, w_ctx, vpt, anchor, parent );
        for ( auto &e : sub_expr ) {
            if ( !e->visit( c_ctx, w_ctx, vpt, e, shared_from_this() ) )
                result = false;
        }
        return result && post_visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
    }

    bool basic_semantic_check( CrateCtx &c_ctx, Worker &w_ctx ) override;

    bool first_transformation( CrateCtx &c_ctx, Worker &w_ctx, sptr<Expr> &anchor, sptr<Expr> parent ) override;

    bool symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) override;

    bool post_symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) override;

    MirVarId parse_mir( CrateCtx &c_ctx, Worker &w_ctx, FunctionImplId func ) override;

    String get_debug_repr() override {
        String str = "BLOCK {\n ";
        for ( auto &s : sub_expr )
            str += s->get_debug_repr() + "\n ";
        return str + " }" + get_additional_debug_data();
    }

    std::vector<sptr<Expr>> get_list() override {
        if ( !sub_expr.empty() ) {
            if ( auto sub_listed = std::dynamic_pointer_cast<ListedExpr>( sub_expr.back() ); sub_listed != nullptr ) {
                return sub_listed->get_list();
            } else {
                return std::vector<sptr<Expr>>( 1, sub_expr.back() );
            }
        } else {
            return std::vector<sptr<Expr>>();
        }
    }
};

// Super class for UnitExpr, TermExpr and TupleExpr
class ParenthesisExpr : public virtual Expr {
public:
    virtual std::vector<sptr<Expr>> get_list() = 0;
};

// The unit type
class UnitExpr : public OperandExpr, public ParenthesisExpr {
public:
    TypeId get_type() override { return TYPE_UNIT; }

    bool matches( sptr<Expr> other ) override { return std::dynamic_pointer_cast<UnitExpr>( other ) != nullptr; }

    bool visit( CrateCtx &c_ctx, Worker &w_ctx, VisitorPassType vpt, sptr<Expr> &anchor, sptr<Expr> parent ) override {
        bool result = visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
        if ( anchor != shared_from_this() ) // replaced itself (object is now invalid)
            return anchor->visit( c_ctx, w_ctx, vpt, anchor, parent );
        return result && post_visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
    }

    MirVarId parse_mir( CrateCtx &c_ctx, Worker &w_ctx, FunctionImplId func ) override { return 0; }

    String get_debug_repr() override { return "UNIT()"; }

    std::vector<sptr<Expr>> get_list() override { return std::vector<sptr<Expr>>(); }
};

// A tuple with multiple elements
class TupleExpr : public OperandExpr, public ParenthesisExpr {
public:
    std::vector<sptr<Expr>> sub_expr;
    TypeId type = 0;

    TypeId get_type() override { return type; }

    bool matches( sptr<Expr> other ) override { return std::dynamic_pointer_cast<TupleExpr>( other ) != nullptr; }

    bool visit( CrateCtx &c_ctx, Worker &w_ctx, VisitorPassType vpt, sptr<Expr> &anchor, sptr<Expr> parent ) override {
        bool result = visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
        if ( anchor != shared_from_this() ) // replaced itself (object is now invalid)
            return anchor->visit( c_ctx, w_ctx, vpt, anchor, parent );
        for ( auto &e : sub_expr ) {
            if ( !e->visit( c_ctx, w_ctx, vpt, e, shared_from_this() ) )
                result = false;
        }
        return result && post_visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
    }

    String get_debug_repr() override {
        String str = "TUPLE( ";
        for ( auto &s : sub_expr )
            str += s->get_debug_repr() + ", ";
        return str + ")" + get_additional_debug_data();
    }

    std::vector<sptr<Expr>> get_list() override { return sub_expr; }
};

// A set with multiple elements
class SetExpr : public OperandExpr, public CompletedExpr, public ListedExpr {
public:
    std::vector<sptr<Expr>> sub_expr;
    TypeId type = 0;

    TypeId get_type() override { return type; }

    bool matches( sptr<Expr> other ) override { return std::dynamic_pointer_cast<SetExpr>( other ) != nullptr; }

    bool visit( CrateCtx &c_ctx, Worker &w_ctx, VisitorPassType vpt, sptr<Expr> &anchor, sptr<Expr> parent ) override {
        bool result = visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
        if ( anchor != shared_from_this() ) // replaced itself (object is now invalid)
            return anchor->visit( c_ctx, w_ctx, vpt, anchor, parent );
        for ( auto &e : sub_expr ) {
            if ( !e->visit( c_ctx, w_ctx, vpt, e, shared_from_this() ) )
                result = false;
        }
        return result && post_visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
    }

    bool first_transformation( CrateCtx &c_ctx, Worker &w_ctx, sptr<Expr> &anchor, sptr<Expr> parent ) override;

    String get_debug_repr() override {
        String str = "SET { ";
        for ( auto &s : sub_expr )
            str += s->get_debug_repr() + ", ";
        return str + "}" + get_additional_debug_data();
    }

    std::vector<sptr<Expr>> get_list() override { return sub_expr; }
};

// A term with a sub expressions
class TermExpr : public OperandExpr, public ParenthesisExpr {
public:
    sptr<Expr> sub_expr;

    TypeId get_type() override { return sub_expr->get_type(); }

    bool matches( sptr<Expr> other ) override { return std::dynamic_pointer_cast<TermExpr>( other ) != nullptr; }

    bool visit( CrateCtx &c_ctx, Worker &w_ctx, VisitorPassType vpt, sptr<Expr> &anchor, sptr<Expr> parent ) override {
        bool result = visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
        if ( anchor != shared_from_this() ) // replaced itself (object is now invalid)
            return anchor->visit( c_ctx, w_ctx, vpt, anchor, parent );
        return result && sub_expr->visit( c_ctx, w_ctx, vpt, sub_expr, shared_from_this() ) &&
               post_visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
    }

    String get_debug_repr() override {
        return "TERM( " + sub_expr->get_debug_repr() + " )" + get_additional_debug_data();
    }

    std::vector<sptr<Expr>> get_list() override { return std::vector<sptr<Expr>>( 1, sub_expr ); }
};

// A array specifier with multiple expressions
class ArraySpecifierExpr : public OperandExpr {
public:
    std::vector<sptr<Expr>> sub_expr;

    bool matches( sptr<Expr> other ) override {
        return std::dynamic_pointer_cast<ArraySpecifierExpr>( other ) != nullptr;
    }

    bool visit( CrateCtx &c_ctx, Worker &w_ctx, VisitorPassType vpt, sptr<Expr> &anchor, sptr<Expr> parent ) override {
        bool result = visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
        if ( anchor != shared_from_this() ) // replaced itself (object is now invalid)
            return anchor->visit( c_ctx, w_ctx, vpt, anchor, parent );
        for ( auto &e : sub_expr ) {
            if ( !e->visit( c_ctx, w_ctx, vpt, e, shared_from_this() ) )
                result = false;
        }
        return result && post_visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
    }

    bool basic_semantic_check( CrateCtx &c_ctx, Worker &w_ctx ) override;

    String get_debug_repr() override {
        String str = "ARRAY[ ";
        for ( auto &s : sub_expr )
            str += s->get_debug_repr();
        return str + " ]" + get_additional_debug_data();
    }
};

// Base class for symbols
class SymbolExpr : public virtual OperandExpr {
public:
    // Types of attributes a symbol can have. This only applies to the local symbol, not the type nor SymbolGraphNode
    enum class Attribute {
        pub, // public
        mut, // mutable
        ref, // borrowed

        count
    };

    bool matches( sptr<Expr> other ) override { return std::dynamic_pointer_cast<SymbolExpr>( other ) != nullptr; }

    bool visit( CrateCtx &c_ctx, Worker &w_ctx, VisitorPassType vpt, sptr<Expr> &anchor, sptr<Expr> parent ) override {
        bool result = visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
        if ( anchor != shared_from_this() ) // replaced itself (object is now invalid)
            return anchor->visit( c_ctx, w_ctx, vpt, anchor, parent );
        return result && post_visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
    }

    // Updates the internal symbol id reference
    virtual void update_symbol_id( SymbolId new_id ) { LOG_ERR( "Virtual function!" ); }
    // Returns the internal symbol id reference
    virtual SymbolId get_symbol_id() {
        LOG_ERR( "Virtual function!" );
        return 0;
    }
    virtual void update_left_symbol_id( SymbolId new_id ) { update_symbol_id( new_id ); }
    // Returns the symbol id of the leftmost sub-expr (used to find the actual parent)
    virtual SymbolId get_left_symbol_id() { return get_symbol_id(); }

    // Returns if the symbol has a specific attribute
    virtual bool has_attribute( Attribute attr ) {
        LOG_ERR( "Virtual function!" );
        return false;
    }

    // Set or clear a specific attribute
    virtual void set_attribute( Attribute attr, bool value = true ) { LOG_ERR( "Virtual function!" ); }
};

// A simple symbol/identifier (variable, function, etc.)
class AtomicSymbolExpr : public SymbolExpr {
public:
    TypeId type;
    String symbol_name;
    SymbolId symbol;
    bool attrs[static_cast<size_t>( Attribute::count )];

    TypeId get_type() override { return type; }

    bool matches( sptr<Expr> other ) override {
        return std::dynamic_pointer_cast<AtomicSymbolExpr>( other ) != nullptr;
    }

    bool visit( CrateCtx &c_ctx, Worker &w_ctx, VisitorPassType vpt, sptr<Expr> &anchor, sptr<Expr> parent ) override {
        bool result = visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
        if ( anchor != shared_from_this() ) // replaced itself (object is now invalid)
            return anchor->visit( c_ctx, w_ctx, vpt, anchor, parent );
        return result && post_visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
    }

    MirVarId parse_mir( CrateCtx &c_ctx, Worker &w_ctx, FunctionImplId func ) override;

    void update_symbol_id( SymbolId new_id ) override { symbol = new_id; }

    SymbolId get_symbol_id() override { return symbol; }

    String get_debug_repr() override { return "SYM(" + to_string( symbol ) + ")" + get_additional_debug_data(); }

    virtual bool has_attribute( Attribute attr ) { return attrs[static_cast<size_t>( attr )]; }

    virtual void set_attribute( Attribute attr, bool value ) { attrs[static_cast<size_t>( attr )] = value; }
};

// Base class for a simple literal
class LiteralExpr : public OperandExpr {};

// Base class for all Blob literals
class BasicBlobLiteralExpr : public LiteralExpr {
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

    bool visit( CrateCtx &c_ctx, Worker &w_ctx, VisitorPassType vpt, sptr<Expr> &anchor, sptr<Expr> parent ) override {
        bool result = visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
        if ( anchor != shared_from_this() ) // replaced itself (object is now invalid)
            return anchor->visit( c_ctx, w_ctx, vpt, anchor, parent );
        return result && post_visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
    }

    MirVarId parse_mir( CrateCtx &c_ctx, Worker &w_ctx, FunctionImplId func ) override {
        static_assert( Bytes <= sizeof( MirLiteral ) );

        auto &op = create_operation( c_ctx, w_ctx, func, shared_from_this(), MirEntry::Type::literal, 0, {} );
        store_in_literal( op.data );
        c_ctx.functions[func].vars[op.ret].value_type = type;
        c_ctx.functions[func].vars[op.ret].type = MirVariable::Type::rvalue;

        return op.ret;
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
        return str + ":" + to_string( type ) + ")" + get_additional_debug_data();
    }

    // Loads the blob with a low endian representation of a number
    void load_from_number( const Number &num, u8 max_mem_size = Bytes ) {
        max_mem_size = std::min( max_mem_size, static_cast<u8>( sizeof( Number ) ) );
        const u8 *tmp = reinterpret_cast<const u8 *>( &num );
        for ( size_t i = 0; i < max_mem_size; i++ )
            blob[i] = tmp[i];
    }

    // Stores the blob in a low endian representation of a mir literal
    void store_in_literal( MirLiteral &literal, u8 max_mem_size = Bytes ) {
        max_mem_size = std::min( max_mem_size, static_cast<u8>( sizeof( MirLiteral ) ) );
        u8 *tmp = reinterpret_cast<u8 *>( &literal );
        for ( size_t i = 0; i < max_mem_size; i++ )
            tmp[i] = blob[i];
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

    bool visit( CrateCtx &c_ctx, Worker &w_ctx, VisitorPassType vpt, sptr<Expr> &anchor, sptr<Expr> parent ) override {
        bool result = visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
        if ( anchor != shared_from_this() ) // replaced itself (object is now invalid)
            return anchor->visit( c_ctx, w_ctx, vpt, anchor, parent );
        return result && post_visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
    }

    String get_debug_repr() override { return "STR \"" + str + "\"" + get_additional_debug_data(); }
};

// An expression which can be broken into multiple sub-expressions by other rvalues/operators
class SeparableExpr : public virtual OperandExpr {
protected:
    std::vector<sptr<Expr>> original_list;
    u32 precedence = 0;

public:
    virtual ~SeparableExpr() {}

    // Separates the expression and all its sub expressions depending on their precedence.
    // Also adds all static statements recursively
    void split_prepend_recursively( std::vector<sptr<Expr>> &rev_list, std::vector<sptr<Expr>> &stst_set, u32 prec,
                                    bool ltr, u8 rule_length ) {
        stst_set.insert( stst_set.end(), static_statements.begin(), static_statements.end() );
        for ( auto expr_itr = original_list.rbegin(); expr_itr != original_list.rend(); expr_itr++ ) {
            auto s_expr = std::dynamic_pointer_cast<SeparableExpr>( *expr_itr );
            if ( rev_list.size() < rule_length && s_expr &&
                 ( prec < s_expr->prec() || ( !ltr && prec == s_expr->prec() ) ) ) {
                s_expr->split_prepend_recursively( rev_list, stst_set, prec, ltr, rule_length );
            } else {
                rev_list.push_back( *expr_itr );
            }
        }
    }

    // Returns the precedence of this Expression binding. Lower values bind stronger
    virtual u32 prec() { return precedence; }

    // Updates the precedence of this expression (avoid this)
    void update_precedence( u32 prec ) { precedence = prec; }

    virtual bool matches( sptr<Expr> other ) override {
        return std::dynamic_pointer_cast<SeparableExpr>( other ) != nullptr;
    }

    virtual void copy_from_other( sptr<Expr> other ) override {
        Expr::copy_from_other( other );
        if ( auto other_separable = std::dynamic_pointer_cast<SeparableExpr>( other ); other_separable != nullptr ) {
            original_list = other_separable->original_list;
            precedence = other_separable->precedence;
        }
    }

    PosInfo get_position_info() override {
        return merge_pos_infos( original_list.front()->get_position_info(), original_list.back()->get_position_info() );
    }
};

// Combines one ore multiple expressions with commas
class CommaExpr : public SeparableExpr, public ListedExpr {
public:
    std::vector<sptr<Expr>> sub_expr;

    CommaExpr() {}
    CommaExpr( sptr<Expr> lvalue, sptr<Expr> rvalue, u32 precedence, std::vector<sptr<Expr>> &original_list ) {
        auto lvalue_list = std::dynamic_pointer_cast<CommaExpr>( lvalue );
        if ( lvalue_list ) { // Is a comma expression
            sub_expr = lvalue_list->sub_expr;
        } else if ( lvalue ) {
            sub_expr.push_back( lvalue );
        }
        auto rvalue_list = std::dynamic_pointer_cast<CommaExpr>( rvalue );
        if ( rvalue_list ) { // Is a comma expression
            sub_expr.insert( sub_expr.end(), rvalue_list->sub_expr.begin(), rvalue_list->sub_expr.end() );
        } else if ( rvalue ) {
            sub_expr.push_back( rvalue );
        }
        this->precedence = precedence;
        this->original_list = original_list;
    }

    TypeId get_type() override { return sub_expr.empty() ? TYPE_UNIT : sub_expr.back()->get_type(); }

    bool matches( sptr<Expr> other ) override { return std::dynamic_pointer_cast<CommaExpr>( other ) != nullptr; }

    bool visit( CrateCtx &c_ctx, Worker &w_ctx, VisitorPassType vpt, sptr<Expr> &anchor, sptr<Expr> parent ) override {
        bool result = visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
        if ( anchor != shared_from_this() ) // replaced itself (object is now invalid)
            return anchor->visit( c_ctx, w_ctx, vpt, anchor, parent );
        for ( auto &e : sub_expr ) {
            if ( !e->visit( c_ctx, w_ctx, vpt, e, shared_from_this() ) )
                result = false;
        }
        return result && post_visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
    }

    bool first_transformation( CrateCtx &c_ctx, Worker &w_ctx, sptr<Expr> &anchor, sptr<Expr> parent ) override;

    String get_debug_repr() override {
        String str = "COMMA( ";
        for ( auto &s : sub_expr )
            str += s->get_debug_repr() + ", ";
        return str + ")" + get_additional_debug_data();
    }

    std::vector<sptr<Expr>> get_list() override { return sub_expr; }
};

// The head of a function. Must be resolved into other expressions
class FuncHeadExpr : public SeparableExpr {
public:
    sptr<Expr> symbol;
    sptr<Expr> parameters;

    FuncHeadExpr() {}
    FuncHeadExpr( sptr<Expr> symbol, sptr<Expr> parameters, u32 precedence, std::vector<sptr<Expr>> &original_list ) {
        this->symbol = symbol;
        this->parameters = parameters;
        this->precedence = precedence;
        this->original_list = original_list;
    }

    TypeId get_type() override { return 0; }

    bool matches( sptr<Expr> other ) override { return std::dynamic_pointer_cast<FuncHeadExpr>( other ) != nullptr; }

    bool visit( CrateCtx &c_ctx, Worker &w_ctx, VisitorPassType vpt, sptr<Expr> &anchor, sptr<Expr> parent ) override {
        bool result = visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
        if ( anchor != shared_from_this() ) // replaced itself (object is now invalid)
            return anchor->visit( c_ctx, w_ctx, vpt, anchor, parent );
        return result && symbol->visit( c_ctx, w_ctx, vpt, symbol, shared_from_this() ) &&
               ( parameters ? parameters->visit( c_ctx, w_ctx, vpt, parameters, shared_from_this() ) : true ) &&
               post_visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
    }

    bool basic_semantic_check( CrateCtx &c_ctx, Worker &w_ctx ) override;

    bool first_transformation( CrateCtx &c_ctx, Worker &w_ctx, sptr<Expr> &anchor, sptr<Expr> parent ) override;

    bool symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) override;

    bool post_symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) override;

    String get_debug_repr() override {
        return "FUNC_HEAD(" + ( parameters ? parameters->get_debug_repr() + " " : "" ) + symbol->get_debug_repr() +
               ")" + get_additional_debug_data();
    }
};

// Specifies a new funcion
class FuncExpr : public SeparableExpr, public CompletedExpr {
public:
    TypeId type; // Every funcion has its own type
    sptr<Expr> parameters;
    sptr<Expr> return_type;
    sptr<Expr> symbol;
    sptr<Expr> body;

    FuncExpr() {}
    FuncExpr( sptr<Expr> symbol, TypeId type, sptr<Expr> parameters, sptr<Expr> return_type, sptr<CompletedExpr> block,
              u32 precedence, std::vector<sptr<Expr>> &original_list ) {
        this->symbol = symbol;
        this->type = type;
        this->parameters = parameters;
        this->return_type = return_type;
        body = block;
        this->precedence = precedence;
        this->original_list = original_list;
    }

    TypeId get_type() override { return type; }

    bool matches( sptr<Expr> other ) override { return std::dynamic_pointer_cast<FuncExpr>( other ) != nullptr; }

    bool visit( CrateCtx &c_ctx, Worker &w_ctx, VisitorPassType vpt, sptr<Expr> &anchor, sptr<Expr> parent ) override {
        bool result = visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
        if ( anchor != shared_from_this() ) // replaced itself (object is now invalid)
            return anchor->visit( c_ctx, w_ctx, vpt, anchor, parent );
        return result &&
               ( parameters ? parameters->visit( c_ctx, w_ctx, vpt, parameters, shared_from_this() ) : true ) &&
               ( return_type ? return_type->visit( c_ctx, w_ctx, vpt, return_type, shared_from_this() ) : true ) &&
               symbol->visit( c_ctx, w_ctx, vpt, symbol, shared_from_this() ) &&
               body->visit( c_ctx, w_ctx, vpt, body, shared_from_this() ) &&
               post_visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
    }

    bool basic_semantic_check( CrateCtx &c_ctx, Worker &w_ctx ) override;

    bool first_transformation( CrateCtx &c_ctx, Worker &w_ctx, sptr<Expr> &anchor, sptr<Expr> parent ) override;

    bool symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) override;

    bool post_symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) override;

    String get_debug_repr() override {
        return "FUNC(" + to_string( type ) + " " + ( parameters ? parameters->get_debug_repr() + " " : "" ) +
               ( symbol ? symbol->get_debug_repr() : "<anonymous>" ) +
               ( return_type ? " -> " + return_type->get_debug_repr() : "" ) + " " + body->get_debug_repr() + ")" +
               get_additional_debug_data();
    }
};

// Specifies a call to a funcion
class FuncCallExpr : public SeparableExpr {
public:
    TypeId type; // Every funcion has its own type
    sptr<Expr> parameters;
    sptr<Expr> symbol;

    FuncCallExpr() {}
    FuncCallExpr( sptr<Expr> symbol, TypeId type, sptr<Expr> parameters, u32 precedence,
                  std::vector<sptr<Expr>> &original_list ) {
        this->symbol = symbol;
        this->type = type;
        this->parameters = parameters;
        this->precedence = precedence;
        this->original_list = original_list;
    }

    TypeId get_type() override { return type; }

    bool matches( sptr<Expr> other ) override { return std::dynamic_pointer_cast<FuncCallExpr>( other ) != nullptr; }

    bool visit( CrateCtx &c_ctx, Worker &w_ctx, VisitorPassType vpt, sptr<Expr> &anchor, sptr<Expr> parent ) override {
        bool result = visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
        if ( anchor != shared_from_this() ) // replaced itself (object is now invalid)
            return anchor->visit( c_ctx, w_ctx, vpt, anchor, parent );
        return result &&
               ( parameters ? parameters->visit( c_ctx, w_ctx, vpt, parameters, shared_from_this() ) : true ) &&
               symbol->visit( c_ctx, w_ctx, vpt, symbol, shared_from_this() ) &&
               post_visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
    }

    bool basic_semantic_check( CrateCtx &c_ctx, Worker &w_ctx ) override;

    MirVarId parse_mir( CrateCtx &c_ctx, Worker &w_ctx, FunctionImplId func ) override;

    String get_debug_repr() override {
        return "FN_CALL(" + to_string( type ) + " " + ( parameters ? parameters->get_debug_repr() + " " : "" ) +
               symbol->get_debug_repr() + ")" + get_additional_debug_data();
    }
};

class OperatorExpr : public SeparableExpr {
public:
    sptr<Expr> lvalue, rvalue;
    String op;
    sptr<String> fn;

    OperatorExpr() {}
    OperatorExpr( String op, sptr<String> fn, sptr<Expr> lvalue, sptr<Expr> rvalue, u32 precedence,
                  std::vector<sptr<Expr>> &original_list ) {
        this->op = op;
        this->fn = fn;
        this->lvalue = lvalue;
        this->rvalue = rvalue;
        this->precedence = precedence;
        this->original_list = original_list;
    }

    TypeId get_type() override { return lvalue->get_type(); }

    bool matches( sptr<Expr> other ) override { return std::dynamic_pointer_cast<OperatorExpr>( other ) != nullptr; }

    bool visit( CrateCtx &c_ctx, Worker &w_ctx, VisitorPassType vpt, sptr<Expr> &anchor, sptr<Expr> parent ) override {
        bool result = visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
        if ( anchor != shared_from_this() ) // replaced itself (object is now invalid)
            return anchor->visit( c_ctx, w_ctx, vpt, anchor, parent );
        return result && ( lvalue ? lvalue->visit( c_ctx, w_ctx, vpt, lvalue, shared_from_this() ) : true ) &&
               ( rvalue ? rvalue->visit( c_ctx, w_ctx, vpt, rvalue, shared_from_this() ) : true ) &&
               post_visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
    }

    MirVarId parse_mir( CrateCtx &c_ctx, Worker &w_ctx, FunctionImplId func ) override;

    String get_debug_repr() override {
        return "OP(" + ( lvalue ? lvalue->get_debug_repr() + " " : "" ) + op +
               ( rvalue ? " " + rvalue->get_debug_repr() : "" ) + ")" + get_additional_debug_data();
    }
};

// Specifies a new variable binding without doing anything with it
class SimpleBindExpr : public SeparableExpr {
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

    bool visit( CrateCtx &c_ctx, Worker &w_ctx, VisitorPassType vpt, sptr<Expr> &anchor, sptr<Expr> parent ) override {
        bool result = visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
        if ( anchor != shared_from_this() ) // replaced itself (object is now invalid)
            return anchor->visit( c_ctx, w_ctx, vpt, anchor, parent );
        return result && expr->visit( c_ctx, w_ctx, vpt, expr, shared_from_this() ) &&
               post_visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
    }

    bool basic_semantic_check( CrateCtx &c_ctx, Worker &w_ctx ) override;

    MirVarId parse_mir( CrateCtx &c_ctx, Worker &w_ctx, FunctionImplId func ) override;

    String get_debug_repr() override { return "BINDING(" + expr->get_debug_repr() + ")" + get_additional_debug_data(); }
};

// Specifies a new symbol alias
class AliasBindExpr : public SeparableExpr {
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

    bool visit( CrateCtx &c_ctx, Worker &w_ctx, VisitorPassType vpt, sptr<Expr> &anchor, sptr<Expr> parent ) override {
        bool result = visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
        if ( anchor != shared_from_this() ) // replaced itself (object is now invalid)
            return anchor->visit( c_ctx, w_ctx, vpt, anchor, parent );
        return result && expr->visit( c_ctx, w_ctx, vpt, expr, shared_from_this() ) &&
               post_visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
    }

    bool basic_semantic_check( CrateCtx &c_ctx, Worker &w_ctx ) override;

    // Returns the list of the subsitutions rules from this alias expr
    std::vector<SymbolSubstitution> get_substitutions();

    String get_debug_repr() override { return "ALIAS(" + expr->get_debug_repr() + ")" + get_additional_debug_data(); }
};


// If condition expression
class IfExpr : public SeparableExpr, public CompletedExpr {
public:
    sptr<Expr> cond, expr_t;

    IfExpr() {}
    IfExpr( sptr<Expr> cond, sptr<Expr> expr_t, u32 precedence, std::vector<sptr<Expr>> &original_list ) {
        this->cond = cond;
        this->expr_t = expr_t;
        this->precedence = precedence;
        this->original_list = original_list;
    }

    TypeId get_type() override { return TYPE_UNIT; }

    bool matches( sptr<Expr> other ) override { return std::dynamic_pointer_cast<IfExpr>( other ) != nullptr; }

    bool visit( CrateCtx &c_ctx, Worker &w_ctx, VisitorPassType vpt, sptr<Expr> &anchor, sptr<Expr> parent ) override {
        bool result = visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
        if ( anchor != shared_from_this() ) // replaced itself (object is now invalid)
            return anchor->visit( c_ctx, w_ctx, vpt, anchor, parent );
        return result && cond->visit( c_ctx, w_ctx, vpt, cond, shared_from_this() ) &&
               expr_t->visit( c_ctx, w_ctx, vpt, expr_t, shared_from_this() ) &&
               post_visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
    }

    bool first_transformation( CrateCtx &c_ctx, Worker &w_ctx, sptr<Expr> &anchor, sptr<Expr> parent ) override;

    bool symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) override;

    bool post_symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) override;

    String get_debug_repr() override {
        return "IF(" + cond->get_debug_repr() + " THEN " + expr_t->get_debug_repr() + " )" +
               get_additional_debug_data();
    }
};

// If condition expression with a else clause
class IfElseExpr : public SeparableExpr, public CompletedExpr {
public:
    sptr<Expr> cond, expr_t, expr_f;

    IfElseExpr() {}
    IfElseExpr( sptr<Expr> cond, sptr<Expr> expr_t, sptr<Expr> expr_f, u32 precedence,
                std::vector<sptr<Expr>> &original_list ) {
        this->cond = cond;
        this->expr_t = expr_t;
        this->expr_f = expr_f;
        this->precedence = precedence;
        this->original_list = original_list;
    }

    TypeId get_type() override { return expr_f->get_type(); }

    bool matches( sptr<Expr> other ) override { return std::dynamic_pointer_cast<IfElseExpr>( other ) != nullptr; }

    bool visit( CrateCtx &c_ctx, Worker &w_ctx, VisitorPassType vpt, sptr<Expr> &anchor, sptr<Expr> parent ) override {
        bool result = visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
        if ( anchor != shared_from_this() ) // replaced itself (object is now invalid)
            return anchor->visit( c_ctx, w_ctx, vpt, anchor, parent );
        return result && cond->visit( c_ctx, w_ctx, vpt, cond, shared_from_this() ) &&
               expr_t->visit( c_ctx, w_ctx, vpt, expr_t, shared_from_this() ) &&
               expr_f->visit( c_ctx, w_ctx, vpt, expr_f, shared_from_this() ) &&
               post_visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
    }

    bool first_transformation( CrateCtx &c_ctx, Worker &w_ctx, sptr<Expr> &anchor, sptr<Expr> parent ) override;

    bool symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) override;

    bool post_symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) override;

    String get_debug_repr() override {
        return "IF(" + cond->get_debug_repr() + " THEN " + expr_t->get_debug_repr() + " ELSE " +
               expr_f->get_debug_repr() + " )" + get_additional_debug_data();
    }
};

// Pre-condition loop expression
class PreLoopExpr : public SeparableExpr, public CompletedExpr {
public:
    sptr<Expr> cond, expr;
    bool evaluation = true;

    PreLoopExpr() {}
    PreLoopExpr( sptr<Expr> cond, sptr<Expr> expr, bool evaluation, u32 precedence,
                 std::vector<sptr<Expr>> &original_list ) {
        this->cond = cond;
        this->expr = expr;
        this->evaluation = evaluation;
        this->precedence = precedence;
        this->original_list = original_list;
    }

    TypeId get_type() override { return TYPE_UNIT; }

    bool matches( sptr<Expr> other ) override { return std::dynamic_pointer_cast<PreLoopExpr>( other ) != nullptr; }

    bool visit( CrateCtx &c_ctx, Worker &w_ctx, VisitorPassType vpt, sptr<Expr> &anchor, sptr<Expr> parent ) override {
        bool result = visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
        if ( anchor != shared_from_this() ) // replaced itself (object is now invalid)
            return anchor->visit( c_ctx, w_ctx, vpt, anchor, parent );
        return result && cond->visit( c_ctx, w_ctx, vpt, cond, shared_from_this() ) &&
               expr->visit( c_ctx, w_ctx, vpt, expr, shared_from_this() ) &&
               post_visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
    }

    bool first_transformation( CrateCtx &c_ctx, Worker &w_ctx, sptr<Expr> &anchor, sptr<Expr> parent ) override;

    bool symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) override;

    bool post_symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) override;

    String get_debug_repr() override {
        return "PRE_LOOP(" + String( evaluation ? "TRUE: " : "FALSE: " ) + cond->get_debug_repr() + " DO " +
               expr->get_debug_repr() + " )" + get_additional_debug_data();
    }
};

// Post-condition loop expression
class PostLoopExpr : public SeparableExpr, public CompletedExpr {
public:
    sptr<Expr> cond, expr;
    bool evaluation = true;

    PostLoopExpr() {}
    PostLoopExpr( sptr<Expr> cond, sptr<Expr> expr, bool evaluation, u32 precedence,
                  std::vector<sptr<Expr>> &original_list ) {
        this->cond = cond;
        this->expr = expr;
        this->evaluation = evaluation;
        this->precedence = precedence;
        this->original_list = original_list;
    }

    TypeId get_type() override { return expr->get_type(); }

    bool matches( sptr<Expr> other ) override { return std::dynamic_pointer_cast<PostLoopExpr>( other ) != nullptr; }

    bool visit( CrateCtx &c_ctx, Worker &w_ctx, VisitorPassType vpt, sptr<Expr> &anchor, sptr<Expr> parent ) override {
        bool result = visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
        if ( anchor != shared_from_this() ) // replaced itself (object is now invalid)
            return anchor->visit( c_ctx, w_ctx, vpt, anchor, parent );
        return result && cond->visit( c_ctx, w_ctx, vpt, cond, shared_from_this() ) &&
               expr->visit( c_ctx, w_ctx, vpt, expr, shared_from_this() ) &&
               post_visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
    }

    bool first_transformation( CrateCtx &c_ctx, Worker &w_ctx, sptr<Expr> &anchor, sptr<Expr> parent ) override;

    bool symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) override;

    bool post_symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) override;

    String get_debug_repr() override {
        return "POST_LOOP(" + String( evaluation ? "TRUE: " : "FALSE: " ) + cond->get_debug_repr() + " DO " +
               expr->get_debug_repr() + " )" + get_additional_debug_data();
    }
};

// Infinite loop expression
class InfLoopExpr : public SeparableExpr, public CompletedExpr {
public:
    sptr<Expr> expr;

    InfLoopExpr() {}
    InfLoopExpr( sptr<Expr> expr, u32 precedence, std::vector<sptr<Expr>> &original_list ) {
        this->expr = expr;
        this->precedence = precedence;
        this->original_list = original_list;
    }

    TypeId get_type() override { return TYPE_NEVER; }

    bool matches( sptr<Expr> other ) override { return std::dynamic_pointer_cast<InfLoopExpr>( other ) != nullptr; }

    bool visit( CrateCtx &c_ctx, Worker &w_ctx, VisitorPassType vpt, sptr<Expr> &anchor, sptr<Expr> parent ) override {
        bool result = visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
        if ( anchor != shared_from_this() ) // replaced itself (object is now invalid)
            return anchor->visit( c_ctx, w_ctx, vpt, anchor, parent );
        return result && expr->visit( c_ctx, w_ctx, vpt, expr, shared_from_this() ) &&
               post_visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
    }

    bool first_transformation( CrateCtx &c_ctx, Worker &w_ctx, sptr<Expr> &anchor, sptr<Expr> parent ) override;

    bool symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) override;

    bool post_symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) override;

    String get_debug_repr() override {
        return "INF_LOOP(" + expr->get_debug_repr() + " )" + get_additional_debug_data();
    }
};

// Iterator loop expression
class ItrLoopExpr : public SeparableExpr, public CompletedExpr {
public:
    sptr<Expr> itr_expr, expr;

    ItrLoopExpr() {}
    ItrLoopExpr( sptr<Expr> itr_expr, sptr<Expr> expr, u32 precedence, std::vector<sptr<Expr>> &original_list ) {
        this->itr_expr = itr_expr;
        this->expr = expr;
        this->precedence = precedence;
        this->original_list = original_list;
    }

    TypeId get_type() override { return TYPE_UNIT; }

    bool matches( sptr<Expr> other ) override { return std::dynamic_pointer_cast<ItrLoopExpr>( other ) != nullptr; }

    bool visit( CrateCtx &c_ctx, Worker &w_ctx, VisitorPassType vpt, sptr<Expr> &anchor, sptr<Expr> parent ) override {
        bool result = visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
        if ( anchor != shared_from_this() ) // replaced itself (object is now invalid)
            return anchor->visit( c_ctx, w_ctx, vpt, anchor, parent );
        return result && itr_expr->visit( c_ctx, w_ctx, vpt, itr_expr, shared_from_this() ) &&
               expr->visit( c_ctx, w_ctx, vpt, expr, shared_from_this() ) &&
               post_visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
    }

    bool first_transformation( CrateCtx &c_ctx, Worker &w_ctx, sptr<Expr> &anchor, sptr<Expr> parent ) override;

    bool symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) override;

    bool post_symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) override;

    String get_debug_repr() override {
        return "ITR_LOOP(" + itr_expr->get_debug_repr() + " DO " + expr->get_debug_repr() + " )" +
               get_additional_debug_data();
    }
};

// Pattern matching expression
class MatchExpr : public SeparableExpr, public CompletedExpr {
public:
    sptr<Expr> selector, cases;

    MatchExpr() {}
    MatchExpr( sptr<Expr> selector, sptr<Expr> cases, u32 precedence, std::vector<sptr<Expr>> &original_list ) {
        this->selector = selector;
        this->cases = cases;
        this->precedence = precedence;
        this->original_list = original_list;
    }

    TypeId get_type() override { return TYPE_UNIT; } // TODO update

    bool matches( sptr<Expr> other ) override { return std::dynamic_pointer_cast<MatchExpr>( other ) != nullptr; }

    bool visit( CrateCtx &c_ctx, Worker &w_ctx, VisitorPassType vpt, sptr<Expr> &anchor, sptr<Expr> parent ) override {
        bool result = visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
        if ( anchor != shared_from_this() ) // replaced itself (object is now invalid)
            return anchor->visit( c_ctx, w_ctx, vpt, anchor, parent );
        return result && selector->visit( c_ctx, w_ctx, vpt, selector, shared_from_this() ) &&
               cases->visit( c_ctx, w_ctx, vpt, cases, shared_from_this() ) &&
               post_visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
    }

    bool basic_semantic_check( CrateCtx &c_ctx, Worker &w_ctx ) override;

    bool first_transformation( CrateCtx &c_ctx, Worker &w_ctx, sptr<Expr> &anchor, sptr<Expr> parent ) override;

    bool symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) override;

    bool post_symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) override;

    String get_debug_repr() override {
        return "MATCH(" + selector->get_debug_repr() + " WITH " + cases->get_debug_repr() + ")" +
               get_additional_debug_data();
    }
};

// Relative index access
class ArrayAccessExpr : public SeparableExpr {
public:
    sptr<Expr> value, index;

    ArrayAccessExpr() {}
    ArrayAccessExpr( sptr<Expr> value, sptr<Expr> index, u32 precedence, std::vector<sptr<Expr>> &original_list ) {
        this->value = value;
        this->index = index;
        this->precedence = precedence;
        this->original_list = original_list;
    }

    TypeId get_type() override { return TYPE_UNIT; } // TODO update

    bool matches( sptr<Expr> other ) override { return std::dynamic_pointer_cast<ArrayAccessExpr>( other ) != nullptr; }

    bool visit( CrateCtx &c_ctx, Worker &w_ctx, VisitorPassType vpt, sptr<Expr> &anchor, sptr<Expr> parent ) override {
        bool result = visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
        if ( anchor != shared_from_this() ) // replaced itself (object is now invalid)
            return anchor->visit( c_ctx, w_ctx, vpt, anchor, parent );
        return result && value->visit( c_ctx, w_ctx, vpt, value, shared_from_this() ) &&
               index->visit( c_ctx, w_ctx, vpt, index, shared_from_this() ) &&
               post_visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
    }

    bool basic_semantic_check( CrateCtx &c_ctx, Worker &w_ctx ) override;

    String get_debug_repr() override {
        return "ARR_ACC " + value->get_debug_repr() + "[" + index->get_debug_repr() + "]" + get_additional_debug_data();
    }
};

// Define a range of values
class RangeExpr : public SeparableExpr {
public:
    sptr<Expr> from, to;
    RangeOperator::Type range_type;
    TypeId type; // TODO update

    RangeExpr() {}
    RangeExpr( sptr<Expr> from, sptr<Expr> to, RangeOperator::Type range_type, u32 precedence,
               std::vector<sptr<Expr>> &original_list ) {
        this->from = from;
        this->to = to;
        this->range_type = range_type;
        this->precedence = precedence;
        this->original_list = original_list;
    }

    TypeId get_type() override { return type; }

    bool matches( sptr<Expr> other ) override { return std::dynamic_pointer_cast<RangeExpr>( other ) != nullptr; }

    bool visit( CrateCtx &c_ctx, Worker &w_ctx, VisitorPassType vpt, sptr<Expr> &anchor, sptr<Expr> parent ) override {
        bool result = visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
        if ( anchor != shared_from_this() ) // replaced itself (object is now invalid)
            return anchor->visit( c_ctx, w_ctx, vpt, anchor, parent );
        return result && ( from ? from->visit( c_ctx, w_ctx, vpt, from, shared_from_this() ) : true ) &&
               ( to ? to->visit( c_ctx, w_ctx, vpt, to, shared_from_this() ) : true ) &&
               post_visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
    }

    String get_debug_repr() override {
        String rt = range_type == RangeOperator::Type::exclude
                        ? "EXCLUDE"
                        : range_type == RangeOperator::Type::exclude_from
                              ? "EXCLUDE_FROM"
                              : range_type == RangeOperator::Type::exclude_to
                                    ? "EXCLUDE_TO"
                                    : range_type == RangeOperator::Type::include
                                          ? "INCLUDE"
                                          : range_type == RangeOperator::Type::include_to ? "INCLUDE_TO" : "INVALID";
        return "RANGE " + rt + " " + ( from ? from->get_debug_repr() : "" ) + ( from && to ? ".." : "" ) +
               ( to ? to->get_debug_repr() : "" ) + get_additional_debug_data();
    }
};

// Struct definition/usage
class StructExpr : public SeparableExpr, public CompletedExpr {
public:
    sptr<Expr> name;
    sptr<Expr> body;

    StructExpr() {}
    StructExpr( sptr<Expr> name, sptr<CompletedExpr> body, u32 precedence, std::vector<sptr<Expr>> &original_list ) {
        this->name = name;
        this->body = body;
        this->precedence = precedence;
        this->original_list = original_list;
    }

    TypeId get_type() override { return TYPE_UNIT; } // TODO update

    bool matches( sptr<Expr> other ) override { return std::dynamic_pointer_cast<StructExpr>( other ) != nullptr; }

    bool visit( CrateCtx &c_ctx, Worker &w_ctx, VisitorPassType vpt, sptr<Expr> &anchor, sptr<Expr> parent ) override {
        bool result = visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
        if ( anchor != shared_from_this() ) // replaced itself (object is now invalid)
            return anchor->visit( c_ctx, w_ctx, vpt, anchor, parent );
        return result && ( name ? name->visit( c_ctx, w_ctx, vpt, name, shared_from_this() ) : true ) &&
               ( body ? body->visit( c_ctx, w_ctx, vpt, body, shared_from_this() ) : true ) &&
               post_visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
    }

    bool basic_semantic_check( CrateCtx &c_ctx, Worker &w_ctx ) override;

    bool first_transformation( CrateCtx &c_ctx, Worker &w_ctx, sptr<Expr> &anchor, sptr<Expr> parent ) override;

    bool symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) override;

    bool post_symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) override;

    String get_debug_repr() override {
        return "STRUCT " + ( name ? name->get_debug_repr() : "<anonymous>" ) + " " +
               ( body ? body->get_debug_repr() : "<undefined>" ) + get_additional_debug_data();
    }
};

// Trait definition
class TraitExpr : public SeparableExpr, public CompletedExpr {
public:
    sptr<Expr> name;
    sptr<Expr> body;

    TraitExpr() {}
    TraitExpr( sptr<Expr> name, sptr<CompletedExpr> body, u32 precedence, std::vector<sptr<Expr>> &original_list ) {
        this->name = name;
        this->body = body;
        this->precedence = precedence;
        this->original_list = original_list;
    }

    TypeId get_type() override { return TYPE_UNIT; } // TODO update

    bool matches( sptr<Expr> other ) override { return std::dynamic_pointer_cast<TraitExpr>( other ) != nullptr; }

    bool visit( CrateCtx &c_ctx, Worker &w_ctx, VisitorPassType vpt, sptr<Expr> &anchor, sptr<Expr> parent ) override {
        bool result = visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
        if ( anchor != shared_from_this() ) // replaced itself (object is now invalid)
            return anchor->visit( c_ctx, w_ctx, vpt, anchor, parent );
        return result && name->visit( c_ctx, w_ctx, vpt, name, shared_from_this() ) &&
               body->visit( c_ctx, w_ctx, vpt, body, shared_from_this() ) &&
               post_visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
    }

    bool basic_semantic_check( CrateCtx &c_ctx, Worker &w_ctx ) override;

    bool first_transformation( CrateCtx &c_ctx, Worker &w_ctx, sptr<Expr> &anchor, sptr<Expr> parent ) override;

    bool symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) override;

    bool post_symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) override;

    String get_debug_repr() override {
        return "TRAIT " + name->get_debug_repr() + " " + body->get_debug_repr() + get_additional_debug_data();
    }
};

// Struct definition/usage
class ImplExpr : public SeparableExpr, public CompletedExpr {
public:
    sptr<Expr> struct_name, trait_name;
    sptr<Expr> body;

    ImplExpr() {}
    ImplExpr( sptr<Expr> struct_name, sptr<Expr> trait_name, sptr<CompletedExpr> body, u32 precedence,
              std::vector<sptr<Expr>> &original_list ) {
        this->struct_name = struct_name;
        this->trait_name = trait_name;
        this->body = body;
        this->precedence = precedence;
        this->original_list = original_list;
    }

    TypeId get_type() override { return TYPE_UNIT; } // TODO update

    bool matches( sptr<Expr> other ) override { return std::dynamic_pointer_cast<ImplExpr>( other ) != nullptr; }

    bool visit( CrateCtx &c_ctx, Worker &w_ctx, VisitorPassType vpt, sptr<Expr> &anchor, sptr<Expr> parent ) override {
        bool result = visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
        if ( anchor != shared_from_this() ) // replaced itself (object is now invalid)
            return anchor->visit( c_ctx, w_ctx, vpt, anchor, parent );
        return result && struct_name->visit( c_ctx, w_ctx, vpt, struct_name, shared_from_this() ) &&
               ( trait_name ? trait_name->visit( c_ctx, w_ctx, vpt, trait_name, shared_from_this() ) : true ) &&
               body->visit( c_ctx, w_ctx, vpt, body, shared_from_this() ) &&
               post_visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
    }

    bool basic_semantic_check( CrateCtx &c_ctx, Worker &w_ctx ) override;

    bool first_transformation( CrateCtx &c_ctx, Worker &w_ctx, sptr<Expr> &anchor, sptr<Expr> parent ) override;

    bool symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) override;

    bool post_symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) override;

    String get_debug_repr() override {
        if ( trait_name ) {
            return "IMPL " + trait_name->get_debug_repr() + " FOR " + struct_name->get_debug_repr() + " " +
                   body->get_debug_repr() + get_additional_debug_data();
        } else {
            return "IMPL " + struct_name->get_debug_repr() + " " + body->get_debug_repr() + get_additional_debug_data();
        }
    }
};

// Access to a member of a symbol
class MemberAccessExpr : public SeparableExpr {
public:
    sptr<Expr> base, name;
    TypeId type;

    MemberAccessExpr() {}
    MemberAccessExpr( sptr<Expr> base, sptr<Expr> name, u32 precedence, std::vector<sptr<Expr>> &original_list ) {
        this->base = base;
        this->name = name;
        this->precedence = precedence;
        this->original_list = original_list;
    }

    TypeId get_type() override { return type; } // TODO update

    bool matches( sptr<Expr> other ) override {
        return std::dynamic_pointer_cast<MemberAccessExpr>( other ) != nullptr;
    }

    bool visit( CrateCtx &c_ctx, Worker &w_ctx, VisitorPassType vpt, sptr<Expr> &anchor, sptr<Expr> parent ) override {
        bool result = visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
        if ( anchor != shared_from_this() ) // replaced itself (object is now invalid)
            return anchor->visit( c_ctx, w_ctx, vpt, anchor, parent );
        return result && base->visit( c_ctx, w_ctx, vpt, base, shared_from_this() ) &&
               name->visit( c_ctx, w_ctx, vpt, name, shared_from_this() ) &&
               post_visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
    }

    MirVarId parse_mir( CrateCtx &c_ctx, Worker &w_ctx, FunctionImplId func ) override;

    String get_debug_repr() override {
        return "MEMBER(" + base->get_debug_repr() + "." + name->get_debug_repr() + ")" + get_additional_debug_data();
    }
};

// Access to a element of a scope
class ScopeAccessExpr : public SeparableExpr, public SymbolExpr {
public:
    sptr<Expr> base, name;
    TypeId type;

    ScopeAccessExpr() {}
    ScopeAccessExpr( sptr<Expr> base, sptr<Expr> name, u32 precedence, std::vector<sptr<Expr>> &original_list ) {
        this->base = base;
        this->name = name;
        this->precedence = precedence;
        this->original_list = original_list;
    }

    TypeId get_type() override { return type; } // TODO update

    bool matches( sptr<Expr> other ) override { return std::dynamic_pointer_cast<ScopeAccessExpr>( other ) != nullptr; }

    bool visit( CrateCtx &c_ctx, Worker &w_ctx, VisitorPassType vpt, sptr<Expr> &anchor, sptr<Expr> parent ) override {
        bool result = visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
        if ( anchor != shared_from_this() ) // replaced itself (object is now invalid)
            return anchor->visit( c_ctx, w_ctx, vpt, anchor, parent );
        return result && ( base ? base->visit( c_ctx, w_ctx, vpt, base, shared_from_this() ) : true ) &&
               name->visit( c_ctx, w_ctx, vpt, name, shared_from_this() ) &&
               post_visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
    }

    void update_symbol_id( SymbolId new_id ) override {
        auto name_symbol = std::dynamic_pointer_cast<SymbolExpr>( name );
        if ( name_symbol )
            name_symbol->update_symbol_id( new_id );
    }

    SymbolId get_symbol_id() override {
        auto name_symbol = std::dynamic_pointer_cast<SymbolExpr>( name );
        if ( name_symbol )
            return name_symbol->get_symbol_id();
        return 0;
    }

    void update_left_symbol_id( SymbolId new_id ) override {
        auto base_symbol = std::dynamic_pointer_cast<SymbolExpr>( base );
        if ( base_symbol )
            base_symbol->update_left_symbol_id( new_id );
    }

    SymbolId get_left_symbol_id() override {
        auto base_symbol = std::dynamic_pointer_cast<SymbolExpr>( base );
        if ( base_symbol )
            return base_symbol->get_left_symbol_id();
        return 0;
    }

    String get_debug_repr() override {
        return "SCOPE(" + ( base ? base->get_debug_repr() : "<global>" ) + "::" + name->get_debug_repr() + ")" +
               get_additional_debug_data();
    }

    virtual bool has_attribute( Attribute attr ) {
        return std::dynamic_pointer_cast<SymbolExpr>( name )->has_attribute( attr );
    }

    virtual void set_attribute( Attribute attr, bool value ) {
        std::dynamic_pointer_cast<SymbolExpr>( name )->set_attribute( attr, value );
    }
};

// Borrow a symbol
class ReferenceExpr : public SeparableExpr {
public:
    sptr<Expr> symbol;

    ReferenceExpr() {}
    ReferenceExpr( sptr<Expr> symbol, u32 precedence, std::vector<sptr<Expr>> &original_list ) {
        this->symbol = symbol;
        this->precedence = precedence;
        this->original_list = original_list;
    }

    TypeId get_type() override { return symbol->get_type(); }

    bool matches( sptr<Expr> other ) override { return std::dynamic_pointer_cast<ReferenceExpr>( other ) != nullptr; }

    bool visit( CrateCtx &c_ctx, Worker &w_ctx, VisitorPassType vpt, sptr<Expr> &anchor, sptr<Expr> parent ) override {
        bool result = visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
        if ( anchor != shared_from_this() ) // replaced itself (object is now invalid)
            return anchor->visit( c_ctx, w_ctx, vpt, anchor, parent );
        return result && symbol->visit( c_ctx, w_ctx, vpt, symbol, shared_from_this() ) &&
               post_visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
    }

    bool basic_semantic_check( CrateCtx &c_ctx, Worker &w_ctx ) override;

    bool first_transformation( CrateCtx &c_ctx, Worker &w_ctx, sptr<Expr> &anchor, sptr<Expr> parent ) override;

    String get_debug_repr() override { return "REF(" + symbol->get_debug_repr() + ")" + get_additional_debug_data(); }
};

// Specifies a symbol as mutable
class MutAttrExpr : public SeparableExpr {
public:
    sptr<Expr> symbol;

    MutAttrExpr() {}
    MutAttrExpr( sptr<Expr> symbol, u32 precedence, std::vector<sptr<Expr>> &original_list ) {
        this->symbol = symbol;
        this->precedence = precedence;
        this->original_list = original_list;
    }

    TypeId get_type() override { return symbol->get_type(); }

    bool matches( sptr<Expr> other ) override { return std::dynamic_pointer_cast<MutAttrExpr>( other ) != nullptr; }

    bool visit( CrateCtx &c_ctx, Worker &w_ctx, VisitorPassType vpt, sptr<Expr> &anchor, sptr<Expr> parent ) override {
        bool result = visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
        if ( anchor != shared_from_this() ) // replaced itself (object is now invalid)
            return anchor->visit( c_ctx, w_ctx, vpt, anchor, parent );
        return result && symbol->visit( c_ctx, w_ctx, vpt, symbol, shared_from_this() ) &&
               post_visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
    }

    bool basic_semantic_check( CrateCtx &c_ctx, Worker &w_ctx ) override;

    bool first_transformation( CrateCtx &c_ctx, Worker &w_ctx, sptr<Expr> &anchor, sptr<Expr> parent ) override;

    String get_debug_repr() override { return "MUT(" + symbol->get_debug_repr() + ")" + get_additional_debug_data(); }
};

// Type of operator
class TypeOfExpr : public SeparableExpr {
public:
    sptr<Expr> symbol;

    TypeOfExpr() {}
    TypeOfExpr( sptr<Expr> symbol, u32 precedence, std::vector<sptr<Expr>> &original_list ) {
        this->symbol = symbol;
        this->precedence = precedence;
        this->original_list = original_list;
    }

    TypeId get_type() override { return TYPE_TYPE; }

    bool matches( sptr<Expr> other ) override { return std::dynamic_pointer_cast<TypeOfExpr>( other ) != nullptr; }

    bool visit( CrateCtx &c_ctx, Worker &w_ctx, VisitorPassType vpt, sptr<Expr> &anchor, sptr<Expr> parent ) override {
        bool result = visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
        if ( anchor != shared_from_this() ) // replaced itself (object is now invalid)
            return anchor->visit( c_ctx, w_ctx, vpt, anchor, parent );
        return result && symbol->visit( c_ctx, w_ctx, vpt, symbol, shared_from_this() ) &&
               post_visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
    }

    String get_debug_repr() override {
        return "TYPE_OF(" + symbol->get_debug_repr() + ")" + get_additional_debug_data();
    }
};

// The typedef-operator
class TypedExpr : public SeparableExpr {
public:
    sptr<Expr> symbol, type;

    TypedExpr() {}
    TypedExpr( sptr<Expr> symbol, sptr<Expr> type, u32 precedence, std::vector<sptr<Expr>> &original_list ) {
        this->symbol = symbol;
        this->type = type;
        this->precedence = precedence;
        this->original_list = original_list;
    }

    TypeId get_type() override { return symbol->get_type(); }

    bool matches( sptr<Expr> other ) override { return std::dynamic_pointer_cast<TypedExpr>( other ) != nullptr; }

    bool visit( CrateCtx &c_ctx, Worker &w_ctx, VisitorPassType vpt, sptr<Expr> &anchor, sptr<Expr> parent ) override {
        bool result = visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
        if ( anchor != shared_from_this() ) // replaced itself (object is now invalid)
            return anchor->visit( c_ctx, w_ctx, vpt, anchor, parent );
        return result && symbol->visit( c_ctx, w_ctx, vpt, symbol, shared_from_this() ) &&
               type->visit( c_ctx, w_ctx, vpt, type, shared_from_this() ) &&
               post_visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
    }

    MirVarId parse_mir( CrateCtx &c_ctx, Worker &w_ctx, FunctionImplId func ) override;

    String get_debug_repr() override {
        return "TYPED(" + symbol->get_debug_repr() + ":" + type->get_debug_repr() + ")" + get_additional_debug_data();
    }
};

// Specification of a module
class ModuleExpr : public SeparableExpr, public CompletedExpr {
public:
    sptr<Expr> symbol;
    sptr<Expr> body;

    ModuleExpr() {}
    ModuleExpr( sptr<Expr> symbol, sptr<Expr> body, u32 precedence, std::vector<sptr<Expr>> &original_list ) {
        this->symbol = symbol;
        this->body = body;
        this->precedence = precedence;
        this->original_list = original_list;
    }

    TypeId get_type() override { return MODULE_TYPE; }

    bool matches( sptr<Expr> other ) override { return std::dynamic_pointer_cast<ModuleExpr>( other ) != nullptr; }

    bool visit( CrateCtx &c_ctx, Worker &w_ctx, VisitorPassType vpt, sptr<Expr> &anchor, sptr<Expr> parent ) override {
        bool result = visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
        if ( anchor != shared_from_this() ) // replaced itself (object is now invalid)
            return anchor->visit( c_ctx, w_ctx, vpt, anchor, parent );
        return result && symbol->visit( c_ctx, w_ctx, vpt, symbol, shared_from_this() ) &&
               body->visit( c_ctx, w_ctx, vpt, body, shared_from_this() ) &&
               post_visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
    }

    bool first_transformation( CrateCtx &c_ctx, Worker &w_ctx, sptr<Expr> &anchor, sptr<Expr> parent ) override;

    bool symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) override;

    bool post_symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) override;

    String get_debug_repr() override {
        return "MODULE " + symbol->get_debug_repr() + " " + body->get_debug_repr() + get_additional_debug_data();
    }
};

// Declaration of a symbol (function)
class DeclarationExpr : public SeparableExpr {
public:
    sptr<Expr> symbol;

    DeclarationExpr() {}
    DeclarationExpr( sptr<Expr> symbol, u32 precedence, std::vector<sptr<Expr>> &original_list ) {
        this->symbol = symbol;
        this->precedence = precedence;
        this->original_list = original_list;
    }

    TypeId get_type() override { return symbol->get_type(); }

    bool matches( sptr<Expr> other ) override { return std::dynamic_pointer_cast<DeclarationExpr>( other ) != nullptr; }

    bool visit( CrateCtx &c_ctx, Worker &w_ctx, VisitorPassType vpt, sptr<Expr> &anchor, sptr<Expr> parent ) override {
        bool result = visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
        if ( anchor != shared_from_this() ) // replaced itself (object is now invalid)
            return anchor->visit( c_ctx, w_ctx, vpt, anchor, parent );
        return result && symbol->visit( c_ctx, w_ctx, vpt, symbol, shared_from_this() ) &&
               post_visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
    }

    bool basic_semantic_check( CrateCtx &c_ctx, Worker &w_ctx ) override;

    String get_debug_repr() override { return "DECL(" + symbol->get_debug_repr() + ")" + get_additional_debug_data(); }
};

// "Public" attribute to a symbol
class PublicAttrExpr : public SeparableExpr {
public:
    sptr<Expr> symbol;

    PublicAttrExpr() {}
    PublicAttrExpr( sptr<Expr> symbol, u32 precedence, std::vector<sptr<Expr>> &original_list ) {
        this->symbol = symbol;
        this->precedence = precedence;
        this->original_list = original_list;
    }

    TypeId get_type() override { return symbol->get_type(); }

    bool matches( sptr<Expr> other ) override { return std::dynamic_pointer_cast<PublicAttrExpr>( other ) != nullptr; }

    bool visit( CrateCtx &c_ctx, Worker &w_ctx, VisitorPassType vpt, sptr<Expr> &anchor, sptr<Expr> parent ) override {
        bool result = visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
        if ( anchor != shared_from_this() ) // replaced itself (object is now invalid)
            return anchor->visit( c_ctx, w_ctx, vpt, anchor, parent );
        return result && symbol->visit( c_ctx, w_ctx, vpt, symbol, shared_from_this() ) &&
               post_visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
    }

    bool basic_semantic_check( CrateCtx &c_ctx, Worker &w_ctx ) override;

    bool first_transformation( CrateCtx &c_ctx, Worker &w_ctx, sptr<Expr> &anchor, sptr<Expr> parent ) override;

    String get_debug_repr() override {
        return "PUBLIC(" + symbol->get_debug_repr() + ")" + get_additional_debug_data();
    }
};

// Declaration of an static statement
class StaticStatementExpr : public Expr {
public:
    sptr<Expr> body;

    StaticStatementExpr() {}
    StaticStatementExpr( sptr<Expr> body ) { this->body = body; }

    TypeId get_type() override { return TYPE_NEVER; }

    bool matches( sptr<Expr> other ) override {
        return std::dynamic_pointer_cast<StaticStatementExpr>( other ) != nullptr;
    }

    bool visit( CrateCtx &c_ctx, Worker &w_ctx, VisitorPassType vpt, sptr<Expr> &anchor, sptr<Expr> parent ) override {
        bool result = visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
        if ( anchor != shared_from_this() ) // replaced itself (object is now invalid)
            return anchor->visit( c_ctx, w_ctx, vpt, anchor, parent );
        return result && body->visit( c_ctx, w_ctx, vpt, body, shared_from_this() ) &&
               post_visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
    }

    bool first_transformation( CrateCtx &c_ctx, Worker &w_ctx, sptr<Expr> &anchor, sptr<Expr> parent ) override;

    bool symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) override;

    bool post_symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) override;

    String get_debug_repr() override { return "STST " + body->get_debug_repr() + get_additional_debug_data(); }
};

// An annotation to give special instructions to the compiler
class CompilerAnnotationExpr : public CompletedExpr {
public:
    sptr<Expr> symbol;
    sptr<Expr> parameters;

    CompilerAnnotationExpr() {}
    CompilerAnnotationExpr( sptr<Expr> symbol, sptr<Expr> parameters, u32 precedence,
                            std::vector<sptr<Expr>> &original_list ) {
        this->symbol = symbol;
        this->parameters = parameters;
    }

    TypeId get_type() override { return TYPE_UNIT; }

    bool matches( sptr<Expr> other ) override {
        return std::dynamic_pointer_cast<CompilerAnnotationExpr>( other ) != nullptr;
    }

    bool visit( CrateCtx &c_ctx, Worker &w_ctx, VisitorPassType vpt, sptr<Expr> &anchor, sptr<Expr> parent ) override {
        bool result = visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
        if ( anchor != shared_from_this() ) // replaced itself (object is now invalid)
            return anchor->visit( c_ctx, w_ctx, vpt, anchor, parent );
        return result && symbol->visit( c_ctx, w_ctx, vpt, symbol, shared_from_this() ) &&
               parameters->visit( c_ctx, w_ctx, vpt, parameters, shared_from_this() ) &&
               post_visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
    }

    bool basic_semantic_check( CrateCtx &c_ctx, Worker &w_ctx ) override;

    String get_debug_repr() override {
        return "ANNOTATE(" + symbol->get_debug_repr() + " " + parameters->get_debug_repr() + ")" +
               get_additional_debug_data();
    }
};

// A macro usage
class MacroExpr : public SeparableExpr {
public:
    sptr<Expr> name;
    sptr<Expr> body;

    MacroExpr() {}
    MacroExpr( sptr<Expr> name, sptr<Expr> body, u32 precedence, std::vector<sptr<Expr>> &original_list ) {
        this->name = name;
        this->body = body;
        this->precedence = precedence;
        this->original_list = original_list;
    }

    TypeId get_type() override { return 0; }

    bool matches( sptr<Expr> other ) override { return std::dynamic_pointer_cast<MacroExpr>( other ) != nullptr; }

    bool visit( CrateCtx &c_ctx, Worker &w_ctx, VisitorPassType vpt, sptr<Expr> &anchor, sptr<Expr> parent ) override {
        bool result = visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
        if ( anchor != shared_from_this() ) // replaced itself (object is now invalid)
            return anchor->visit( c_ctx, w_ctx, vpt, anchor, parent );
        return result && name->visit( c_ctx, w_ctx, vpt, name, shared_from_this() ) &&
               body->visit( c_ctx, w_ctx, vpt, body, shared_from_this() ) &&
               post_visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
    }

    bool basic_semantic_check( CrateCtx &c_ctx, Worker &w_ctx ) override;

    String get_debug_repr() override {
        return "MACRO(" + name->get_debug_repr() + "! " + body->get_debug_repr() + ")" + get_additional_debug_data();
    }
};

// Specify a block or function to be unsafe
class UnsafeExpr : public SeparableExpr {
public:
    sptr<Expr> block;

    UnsafeExpr() {}
    UnsafeExpr( sptr<Expr> block, u32 precedence, std::vector<sptr<Expr>> &original_list ) {
        this->block = block;
        this->precedence = precedence;
        this->original_list = original_list;
    }
    TypeId get_type() override { return block->get_type(); }

    bool matches( sptr<Expr> other ) override { return std::dynamic_pointer_cast<UnsafeExpr>( other ) != nullptr; }

    bool visit( CrateCtx &c_ctx, Worker &w_ctx, VisitorPassType vpt, sptr<Expr> &anchor, sptr<Expr> parent ) override {
        bool result = visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
        if ( anchor != shared_from_this() ) // replaced itself (object is now invalid)
            return anchor->visit( c_ctx, w_ctx, vpt, anchor, parent );
        return result && block->visit( c_ctx, w_ctx, vpt, block, shared_from_this() ) &&
               post_visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
    }

    bool first_transformation( CrateCtx &c_ctx, Worker &w_ctx, sptr<Expr> &anchor, sptr<Expr> parent ) override;

    String get_debug_repr() override { return "UNSAFE " + block->get_debug_repr() + get_additional_debug_data(); }
};

// Specification of a symbol with generic attributes
class TemplateExpr : public SeparableExpr, public SymbolExpr {
public:
    sptr<Expr> symbol;
    std::vector<sptr<Expr>> attributes;

    TemplateExpr() {}
    TemplateExpr( sptr<Expr> symbol, sptr<Expr> attributes, u32 precedence, std::vector<sptr<Expr>> &original_list ) {
        this->symbol = symbol;
        this->attributes = std::vector<sptr<Expr>>( 1, attributes );
        this->precedence = precedence;
        this->original_list = original_list;
    }
    TypeId get_type() override { return symbol->get_type(); }

    bool matches( sptr<Expr> other ) override { return std::dynamic_pointer_cast<TemplateExpr>( other ) != nullptr; }

    bool visit( CrateCtx &c_ctx, Worker &w_ctx, VisitorPassType vpt, sptr<Expr> &anchor, sptr<Expr> parent ) override {
        bool result = visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
        if ( anchor != shared_from_this() ) // replaced itself (object is now invalid)
            return anchor->visit( c_ctx, w_ctx, vpt, anchor, parent );
        for ( auto &e : attributes ) {
            if ( !e->visit( c_ctx, w_ctx, vpt, e, shared_from_this() ) )
                result = false;
        }
        return result && symbol->visit( c_ctx, w_ctx, vpt, symbol, shared_from_this() ) &&
               post_visit_impl( c_ctx, w_ctx, vpt, *this, anchor, parent );
    }

    bool basic_semantic_check( CrateCtx &c_ctx, Worker &w_ctx ) override;

    bool first_transformation( CrateCtx &c_ctx, Worker &w_ctx, sptr<Expr> &anchor, sptr<Expr> parent ) override;

    void update_symbol_id( SymbolId new_id ) override {
        auto name_symbol = std::dynamic_pointer_cast<SymbolExpr>( symbol );
        if ( name_symbol )
            name_symbol->update_symbol_id( new_id );
    }

    SymbolId get_symbol_id() override {
        auto name_symbol = std::dynamic_pointer_cast<SymbolExpr>( symbol );
        if ( name_symbol )
            name_symbol->get_symbol_id();
        return 0;
    }

    String get_debug_repr() override {
        String str = "TEMPLATE " + symbol->get_debug_repr() + "<";
        for ( auto &s : attributes )
            str += s->get_debug_repr() + ", ";
        return str + " >" + get_additional_debug_data();
    }

    virtual bool has_attribute( Attribute attr ) {
        return std::dynamic_pointer_cast<SymbolExpr>( symbol )->has_attribute( attr );
    }

    virtual void set_attribute( Attribute attr, bool value ) {
        std::dynamic_pointer_cast<SymbolExpr>( symbol )->set_attribute( attr, value );
    }
};

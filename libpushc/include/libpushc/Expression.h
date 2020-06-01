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
#include "libpushc/CrateCtx.h"
#include "libpushc/MirTranslation.h"

struct AstNode;

// Defines the type of a visitor pass
enum class VisitorPassType {
    BASIC_SEMANTIC_CHECK, // checks some basic semantic requirements for each expression
    FIRST_TRANSFORMATION, // transformations which can be done without symbol information
    SYMBOL_DISCOVERY, // discover all symbols in the global declarative scope

    count
};

// Specifies the type of AST node expression
enum class ExprType {
    none, // only ues for patterns
    token,

    decl_scope,
    imp_scope,
    single_completed,
    block,
    set,
    unit,
    term,
    tuple,
    array_specifier,
    array_list,
    comma_list,

    numeric_literal,
    string_literal,

    atomic_symbol,
    func_head,
    func,
    func_decl,
    func_call,

    op,
    simple_bind,
    alias_bind,
    if_bind,
    if_else_bind,

    if_cond,
    if_else,
    pre_loop,
    post_loop,
    inf_loop,
    itr_loop,
    match,

    self,
    self_type,
    struct_initializer,

    structure,
    trait,
    implementation,

    member_access,
    scope_access,
    array_access,

    range,
    reference,
    mutable_attr,
    typeof_op,
    typed_op,

    module,
    declaration,
    public_attr,
    static_statement,
    compiler_annotation,
    macro_call,
    unsafe,
    template_postfix,

    count
};

// Defines some properties which different types of Expressions may have
enum class ExprProperty {
    temporary, // is only used for AST construction
    shallow, // will be resolved

    operand, // if the expr may be used as an operand
    completed, // is completed (a block or so)
    parenthesis, // surrounds its content by parenthesis '()'
    braces, // surrounds its content by braces '{}'
    brackets, // surrounds its content by brackets '[]'
    symbol, // can be used as symbol (e. g. in a symbol path)
    symbol_like, // includes specifiers like mut and ref (e. g. as return type)
    literal, // is a literal
    separable, // can be divided into its sub-exprs
    decl_parent, // children are in a decl scope
    named_scope, // creates a new named scope
    anonymous_scope, // creates a new anonymous scope

    assignment, // specialization of an operator
    implication, // specialization of an operator
    in_operator, // specialization of an operator

    pub, // public (symbol)
    mut, // mutable (symbol)
    ref, // borrowed (symbol)

    count
};

// Defines indices to access named entries in ast nodes
enum class AstChild {
    symbol,
    symbol_like, // mut
    struct_symbol, // impl
    trait_symbol, // impl
    cond, // control flow
    itr, // itr loop
    select, // match
    parameters, // func, annotation
    return_type, // func
    left_expr, // operator, typed
    right_expr, // operator, typed
    base, // array/member/scope access
    index, // array access
    member, // member/scope access
    from, // range
    to, // range

    count
};

// The nodes of which the AST is build up
struct AstNode {
    ExprType type = ExprType::none;
    std::set<ExprProperty> props;

    PosInfo pos_info;
    std::vector<AstNode> static_statements;
    std::vector<AstNode> annotations;
    std::vector<SymbolSubstitution> substitutions; // in decl scopes

    std::vector<AstNode> original_list; // if separable
    u32 precedence = 0; // for AST construction
    std::map<AstChild, AstNode> named; // see AstChild
    std::vector<AstNode> children; // unnamed children (list)

    Token token; // only for token and operator
    String symbol_name; // only for atomic symbol and operator (called function)
    SymbolId symbol = 0; // only for atomic symbol
    SymbolId scope_symbol = 0; // only for scope exprs
    TypeId literal_type = 0; // only for literals whose type is known
    Number literal_number = 0; // only for numeric/boolean literals
    String literal_string; // only for string literals
    bool continue_eval = true; // only for loops (for what value the loop is continued)
    Operator::RangeOperatorType range_type; // only for ranges

    // General management

    // Returns whether the expr has the specified property
    bool has_prop( ExprProperty prop ) { return props.find( prop ) != props.end(); }

    // AST generation

    // Checks if matches the expression
    bool matches( const AstNode &pattern ) {
        if ( pattern.type != ExprType::none && pattern.type != type )
            return false;
        if ( pattern.type == ExprType::token && pattern.token.content != token.content )
            return false;
        for ( auto &prop : pattern.props ) {
            if ( props.find( prop ) == props.end() )
                return false;
        }
        return true;
    }

    // Generates the properties to the expr type
    void generate_new_props();

    // Separates the expression and all its sub expressions depending on their precedence.
    // Also adds all static statements recursively
    void split_prepend_recursively( std::vector<AstNode> &rev_list, std::vector<AstNode> &stst_set, u32 prec, bool ltr,
                                    u8 rule_length );

    // Symbol methods

    // This method is used to minimize the needed code to implement a visitor pattern using templates (see
    // post_visit_impl()). Returns false if the pass failed
    bool visit( CrateCtx &c_ctx, Worker &w_ctx, VisitorPassType vpt, AstNode &parent, bool expect_operand );

    // Creates a symbol chain from this expression which contains symbols or scoped symbols
    sptr<std::vector<SymbolIdentifier>> get_symbol_chain( CrateCtx &c_ctx, Worker &w_ctx );

    // Checks very basic semantic conditions on an expr. Returns false when an error has been found
    bool basic_semantic_check( CrateCtx &c_ctx, Worker &w_ctx );

    // Does basic transformations, which don't require symbol information
    bool first_transformation( CrateCtx &c_ctx, Worker &w_ctx, AstNode &parent, bool &expect_operand );

    // Prepares the symbol discovery for this expr
    bool symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx );

    // Used in the symbol discovery pass
    bool post_symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx );

    // Updates the internal symbol id reference
    void update_symbol_id( SymbolId new_id );
    // Returns the internal symbol id reference
    SymbolId get_symbol_id();

    // Updates the symbol id of the leftmost sub-expr (used to find the actual parent)
    void update_left_symbol_id( SymbolId new_id );
    // Returns the symbol id of the leftmost sub-expr (used to find the actual parent)
    SymbolId get_left_symbol_id();

    // Mir methods

    // Called on structs and alike to resolve the type symbols (which requires all symbols to be discovered)
    void find_types( CrateCtx &c_ctx, Worker &w_ctx );

    // Parses the expr and appends the generated instructions to the function. Returns the result variable
    MirVarId parse_mir( CrateCtx &c_ctx, Worker &w_ctx, FunctionImplId func );

    // Create variable bindings from this expr. Returns the var, which was bound to
    MirVarId bind_vars( CrateCtx &c_ctx, Worker &w_ctx, FunctionImplId func, MirVarId in_var,
                                             AstNode &bind_expr, bool checked_deconstruction );

    // Check if a deconstruction is possible/valid. Returns the var with the check result
    MirVarId check_deconstruction( CrateCtx &c_ctx, Worker &w_ctx, FunctionImplId func,
                                                        MirVarId in_var, AstNode &bind_expr );

    // Debugging & message methods

    // Returns a string representation of this ast node for debugging purposes
    String get_debug_repr() const;
};

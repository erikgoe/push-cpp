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
#include "libpushc/Intrinsics.h"

// Identifies a type
using TypeId = u32;

// Identifies a symbol
using SymbolId = u32;
constexpr SymbolId ROOT_SYMBOL = 1; // the global root symbol

// Identifies a function body
using FunctionImplId = u32;

// Identifies a local mir variable
using MirVarId = u32;

// Stores literal values (or pointers)
using MirLiteral = u64;

// Constants
constexpr TypeId TYPE_UNIT = 1; // The initial unit type
constexpr TypeId TYPE_NEVER = 2; // The initial never type
constexpr TypeId TYPE_TYPE = 3; // The initial type type
constexpr TypeId MODULE_TYPE = 4; // The initial module type
constexpr TypeId LAST_FIX_TYPE = MODULE_TYPE; // The last not variable type

// Checks if a token list matches a specific expression and translates it
struct SyntaxRule {
    u32 precedence = 0; // precedence of this syntax matching
    bool ltr = true; // associativity
    bool ambiguous = false; // whether this symtax has an ambiguous interpregation
    std::pair<u32, u32> prec_class = std::make_pair(
        UINT32_MAX, UINT32_MAX ); // precedence-update class to a path as class-from-pair (if not UINT32_MAX)
    u32 prec_bias = NO_BIAS_VALUE; // optional value to prefer one syntax over another despite the precedence
    std::vector<AstNode> expr_list; // list which has to be matched against

    // Checks if a reversed expression list matches this syntax rule
    bool matches_reversed( std::vector<AstNode> &rev_list );

    // Create a new expression according to this rule.
    std::function<AstNode( std::vector<AstNode> &, Worker &w_ctx )> create;
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

    // Signature of a parameter or return type
    struct ParamSig {
        TypeId type = 0; // type of the parameter
        String name; // name of the parameter (not for return types)
        bool ref = false; // whether the value is borrowed
        bool mut = false; // whether the value is mutable

        bool operator==( const ParamSig &other ) const {
            return type == other.type && name == other.name && ref == other.ref && mut == other.mut;
        }
    };
    ParamSig eval_type; // the type which is returned when the symbol is evaluated (return type of functions)
    std::vector<ParamSig> parameters; // function parameters

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
    std::vector<AstNode *>
        original_expr; // Expressions which define this symbol (the ast may not be changed after setting this)

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

    FunctionImplId function_body = 0; // the function body, if it's a function
};

// Represent s a MIR instruction inside a function
struct MirEntry {
    AstNode *original_expr;

    // The type of this instruction
    enum class Type {
        nop, // no operation
        intrinsic, // some intrinsic operation
        type, // specifies the type of a variable
        literal, // literal definition
        call, // function call
        member, // member access
        label, // label declaration
        cond_jmp_z, // conditional jump if arg is zero
        cast, // type cast

        count
    } type = Type::nop;


    MirVarId ret = 0; // variable which will contain the result
    std::vector<MirVarId> params; // parameters for this instruction
    SymbolId symbol = 0; // used symbol (e. g. for calls)
    MirLiteral data; // contains literal data or a pointer to it
    MirIntrinsic intrinsic = MirIntrinsic::none; // if it's an intrinsic operation
};

// Represents a local MIR variable inside the function
struct MirVariable {
    // The type of this variable
    enum class Type {
        value, // normal owning variable
        rvalue, // rvalue
        l_ref, // local reference
        p_ref, // parameter reference
        label, // just a label specifier

        count
    } type = Type::value;

    String name; // the original variable name (temporaries have an empty name)
    TypeId value_type = 0; // type of the value of this variable
    bool mut = false; // whether this variable can be updated
    MirVarId ref = 0; // referred variable (for l_ref or for method access)
    size_t member_idx = 0; // used for member access operations
};

// Represents the content of a function
struct FunctionImpl {
    TypeId type = 0;

    std::vector<MirVarId> params;
    MirVarId ret;

    std::vector<MirEntry> ops; // instructions
    std::vector<MirVariable> vars; // variables
};


// Contains context while building the crate
struct CrateCtx {
    sptr<AstNode> ast; // the current Abstract Syntax Tree
    std::vector<SymbolGraphNode> symbol_graph; // contains all graph nodes, idx 0 is the global root node
    std::vector<TypeTableEntry> type_table; // contains all types
    std::vector<FunctionImpl> functions; // contains all function implementations (MIR)

    TypeId struct_type = 0; // internal struct type
    TypeId trait_type = 0; // internal trait type
    TypeId fn_type = 0; // internal function type
    TypeId mod_type = 0; // internal module type
    TypeId unit_type = 0; // type of the unit type
    TypeId int_type = 0; // type of the integer trait
    TypeId str_type = 0; // type of the string trait

    TypeId drop_fn = 0; // the function which is called on variable drop

    std::vector<SyntaxRule> rules;
    std::unordered_map<String, std::pair<TypeId, u64>> literals_map; // maps literals to their typeid and mem_value


    SymbolId current_scope = ROOT_SYMBOL; // new symbols are created on top of this one
    std::vector<std::vector<SymbolSubstitution>> current_substitutions; // Substitution rules for each new scope

    std::vector<std::vector<MirVarId>> curr_living_vars;
    std::vector<std::map<String, std::vector<MirVarId>>> curr_name_mapping; // mappes names to stacks of shaddowned vars

    CrateCtx();
};
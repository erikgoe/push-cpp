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

struct CrateCtx;

// Identifies a type
using TypeId = u32;

// Identifies a symbol
using SymbolId = u32;
constexpr SymbolId ROOT_SYMBOL = 1; // the global root symbol

// Identifies a function body
using FunctionImplId = u32;

// Identifies a local mir variable
using MirVarId = u32;

// Identifies a local mir variable
using MirEntryId = u32;

// Stores literal values (or pointers)
struct MirLiteral {
    bool is_inline; // true when value contains the data
    u64 value; // data or index inside CrateCtx::literal_data
    size_t size; // size in bytes (the value outside of this border is undefined)
};

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
class ConstValue {
    std::vector<u8> _data; // the data blob

public:
    // Creates an empty value
    ConstValue() {}
    // Stores some arbitrary data
    template <typename T>
    ConstValue( const T &data ) {
        _data.resize( sizeof( T ) );
        for ( size_t i = 0; i < sizeof( T ); i++ )
            _data[i] = reinterpret_cast<const u8 *>( &data )[i];
    }
    // Returns the data interpreted as a specific type if the type sizes match
    template <typename T>
    std::optional<const T *> get() const {
        if ( sizeof( T ) != _data.size() )
            return std::nullopt;
        else
            return reinterpret_cast<const T *>( _data.data() );
    }
    // Returns the raw data
    const std::vector<u8> &get_raw() const { return _data; }
    // Returns whether the value contains no data
    bool is_empty() const { return _data.empty(); }

    bool operator==( const ConstValue &other ) const { return _data == other._data; }
};

// Manages the type/value of a symbol
class TypeSelection {
    std::vector<TypeId>
        type_requirements; // requirements on the type while it is not terminated TODO make this a set(?)
    TypeId final_type = 0; // final type of the value (cannot be changed later)
    std::vector<MirVarId> type_group; // includes variables with _exactly_ the same type (not transitive)

public:
    bool is_final() const { return final_type != 0; }
    bool has_any_requirements( CrateCtx *c_ctx, FunctionImplId fn ) const {
        return final_type != 0 || !get_all_requirements( c_ctx, fn ).empty();
    }
    bool has_unfinalized_requirements( CrateCtx *c_ctx, FunctionImplId fn ) const {
        return final_type == 0 && !get_all_requirements( c_ctx, fn ).empty();
    }
    void set_final_type( CrateCtx *c_ctx, FunctionImplId fn, TypeId type );
    TypeId get_final_type() const {
        assert( type_requirements.empty() );
        return final_type;
    }
    void add_requirement( TypeId type ) {
        if ( type == final_type )
            return;
        assert( final_type == 0 );
        type_requirements.push_back( type );
    }
    void add_requirement( const TypeSelection &type );
    void add_requirements( const std::vector<TypeId> &types ) {
        if ( types.size() == 0 && types.front() == final_type )
            return;
        assert( final_type == 0 );
        type_requirements.insert( type_requirements.end(), types.begin(), types.end() );
    }
    void reserve_requirement_memory( size_t additional_size ) {
        type_requirements.reserve( type_requirements.size() + additional_size );
    }

    // Returns all type requirements including those of the type group and respecting if the type is already final
    const std::vector<TypeId> get_all_requirements( CrateCtx *c_ctx, FunctionImplId fn ) const;

    // Add a variable to the type groups
    void bind_variable( CrateCtx *c_ctx, FunctionImplId fn, MirVarId var, MirVarId own_id );
};

// Identifies a local symbol (must be chained to get a full symbol identification)
struct SymbolIdentifier {
    String name; // the local symbol name (empty means anonymous scope)

    // Signature of a parameter or return type
    struct ParamSig {
        TypeId type = 0; // type of the parameter
        size_t template_type_index = 0; // If this is set, the type is defined by a template symbol
        sptr<std::vector<SymbolIdentifier>> tmp_type_symbol; // only while symbols are discovered
        String name; // name of the parameter (not for return types)
        bool ref = false; // whether the value is borrowed
        bool mut = false; // whether the value is mutable

        bool operator==( const ParamSig &other ) const;
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
    bool signature_evaluated =
        false; // is set to true, when the full signature was evaluated (excluding template parameters)
    bool value_evaluated = false; // is set to true, when the related value (e. g. the function body) was completely
                                  // evaluated TODO set also for struct, traits, impls, templates and modules
    bool signature_evaluation_ongoing =
        false; // is used internally to detect dependency cycles (not used in analyse_function_signature())
    bool proposed = false; // if it's not sure whether this symbol is used

    std::vector<String> compiler_annotations; // additional annotations from the user
    AstNode *where_clause = nullptr; // where-clause to this symbols

    size_t template_type_index = 0; // like SymbolIdentifier::ParamSig::template_type_index
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

class ParamContainer;
// Iterator for the ParamContainer. Allows range-based for loop, etc
class ParamContainerIterator {
    const ParamContainer *container;
    size_t index;

public:
    ParamContainerIterator( const ParamContainer *container, size_t index = 0 ) {
        this->container = container;
        this->index = index;
    }
    ParamContainerIterator &operator++() {
        index++;
        return *this;
    }
    size_t operator-( const ParamContainerIterator &other ) const {
        if ( container != other.container )
            return 0;
        return index - other.index;
    }
    bool operator==( const ParamContainerIterator &other ) const {
        return container == other.container && index == other.index;
    }
    bool operator!=( const ParamContainerIterator &other ) const { return !operator==( other ); }
    const MirVarId &operator*() const;
};

// Stores parameter configurations
class ParamContainer {
    std::vector<std::pair<String, MirVarId>> params; // pairs of names and their variables

public:
    static const size_t INVALID_POSITION_VAL; // Used to specify an invalid parameter position

    ParamContainer() {}
    ParamContainer( MirVarId single_var ) { push_back( single_var ); }
    size_t size() const { return params.size(); }
    bool empty() const { return params.empty(); }
    void reserve( size_t count ) { params.reserve( count ); }
    void push_back( String &name, MirVarId var ) { params.push_back( std::make_pair( name, var ) ); }
    void push_back( MirVarId var ) { params.push_back( std::make_pair( "", var ) ); }

    ParamContainerIterator begin() const { return ParamContainerIterator( this, 0 ); }
    ParamContainerIterator end() const { return ParamContainerIterator( this, params.size() ); }
    ParamContainerIterator find( MirVarId &var ) {
        return ParamContainerIterator(
            this, std::find_if( params.begin(), params.end(), [&]( auto &pair ) { return pair.second == var; } ) -
                      params.begin() );
    }

    // @param map_invalid_to_zero is used with templates which can resolve parameters which have not been passed
    // explicitly
    MirVarId get_param( size_t index, bool map_invalid_to_zero = false ) {
        if ( index == SIZE_MAX ) {
            if ( !map_invalid_to_zero )
                LOG_ERR( "Invalid parameter permutation detected!" );
            return 0;
        }
        return params[index].second;
    }

    // Selects a matching permutation of the parameters
    bool get_param_permutation( const std::vector<String> &names, std::vector<size_t> &out_permutation,
                                bool skip_first = false );

    // Applies the permutation, so that future calls to get_param will be follow this order
    void apply_param_permutation( const std::vector<String> &names );

    friend class ParamContainerIterator;
};

// Represent s a MIR instruction inside a function
struct MirEntry {
    AstNode *original_expr;

    // The type of this instruction
    enum class Type {
        nop, // no operation
        intrinsic, // some intrinsic operation
        literal, // literal definition
        type, // type binding
        call, // function call
        bind, // assign/move a variable into another
        purge, // remove given variables
        member, // member access
        merge, // combine vars to a struct
        label, // label declaration
        cond_jmp_z, // conditional jump if arg is zero (means false)
        jmp, // unconditional jump
        inv, // binary invert a value (should only apply to machine primitives)
        cast, // type cast
        ret, // return operation

        count
    } type = Type::nop;


    MirVarId ret = 0; // variable which will contain the result
    ParamContainer params; // parameters for this instruction
    MirVarId symbol = 0; // Variables which holds symbol data
    bool inference_finished = false; // only for calls (whose symbols need to be inferred first)
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
        not_dropped, // variable which requires no dropping routine
        label, // just a label specifier
        symbol, // just a static symbol specifier
        undecided, // used with member access

        count
    } type = Type::value;

    // Ast data
    String name; // the original variable name (temporaries have an empty name)
    ParamContainer template_args; // only for symbols, with explicit template arguments
    bool mut = false; // whether this variable can be updated
    MirVarId ref = 0; // referred variable (for l_ref or for method access; should never reference a l_ref)
    SymbolIdentifier member_identifier; // used while types haven't been resolved
    MirVarId base_ref = 0; // used for a method call to specify the "self" object (may also be a l_ref)
    std::vector<SymbolId> symbol_set; // stores symbols which are identified with this variable. "One of them"
    AstNode *original_expr = nullptr; // refers to the original variable or expression

    // Mainly Mir data
    TypeSelection value_type; // "All of them"
    size_t member_idx = 0; // used for member access operations
    bool type_inference_finished = false; // when the inference is finished, calls to infer_type are ignored
};

// Represents the content of a function
struct FunctionImpl {
    TypeId type = 0;

    std::vector<MirVarId> params;
    MirVarId ret;

    std::vector<MirEntry> ops; // instructions
    std::vector<MirVariable> vars; // variables
    std::vector<std::pair<String, AstNode *>> drop_list; // stores where a variable was dropped
};


// Contains context while building the crate
struct CrateCtx {
    sptr<AstNode> ast; // the current Abstract Syntax Tree
    std::vector<SymbolGraphNode> symbol_graph; // contains all graph nodes, idx 0 is the global root node
    std::vector<TypeTableEntry> type_table; // contains all types
    std::vector<FunctionImpl> functions; // contains all function implementations (MIR)
    std::vector<u8> literal_data; // contains a huge blob with all (bigger) literals of the program

    TypeId type_type = 0; // internal type of types
    TypeId struct_type = 0; // internal struct type
    TypeId trait_type = 0; // internal trait type
    TypeId fn_type = 0; // internal function type
    TypeId template_struct_type = 0; // internal template struct type
    TypeId template_trait_type = 0; // internal template trait type
    TypeId template_fn_type = 0; // internal template function type
    TypeId mod_type = 0; // internal module type
    TypeId unit_type = 0; // type of the unit type
    TypeId int_type = 0; // type of the integer trait
    TypeId str_type = 0; // type of the string trait
    TypeId tuple_type = 0; // type of the tuple template type
    TypeId array_type = 0; // type of the array template type
    TypeId iterator_type = 0; // type of the iterator trait

    std::vector<SymbolId> drop_fn; // functions which are called on variable drop
    TypeId equals_fn = 0; // the function which is called to check if two variables are equal
    TypeId itr_valid_fn = 0; // the function which is called to check if an iterator ist still valid
    TypeId itr_get_fn = 0; // the function which is called to access the value behind a iterator
    TypeId itr_next_fn = 0; // the function which is called to increase the iterator pointer

    MirLiteral true_val = { true, 0xff, 1 }; // the representation of the boolean "true" value
    MirLiteral false_val = { true, 0, 1 }; // the representation of the boolean "false" value

    std::vector<SyntaxRule> rules;
    std::unordered_map<String, std::pair<TypeId, u64>> literals_map; // maps literals to their typeid and mem_value


    SymbolId current_scope = ROOT_SYMBOL; // new symbols are created on top of this one
    std::vector<std::vector<SymbolSubstitution>> current_substitutions; // Substitution rules for each new scope
    SymbolId first_adhoc_symbol = 0; // the first symbol which does not occur in the source code

    std::vector<std::vector<MirVarId>> curr_living_vars;
    std::vector<std::map<String, std::vector<MirVarId>>> curr_name_mapping; // mappes names to stacks of shaddowned vars
    MirVarId curr_self_var = 0; // describes the current self parameter var
    TypeId curr_self_type = 0; // describes which type is the current object type
    std::stack<sptr<std::vector<SymbolIdentifier>>>
        curr_self_type_symbol_stack; // like curr_left_type, but during symbol discovery

    CrateCtx();
};

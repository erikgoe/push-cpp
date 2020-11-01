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

#include "libpushc/stdafx.h"
#include "libpushc/SymbolUtil.h"

sptr<std::vector<SymbolIdentifier>> split_symbol_chain( const String &chained, String separator ) {
    auto ret = make_shared<std::vector<SymbolIdentifier>>();
    size_t pos = 0;
    size_t prev_pos = pos;
    while ( pos != String::npos ) {
        pos = chained.find( separator, pos );
        ret->push_back( SymbolIdentifier{ chained.slice( prev_pos, pos - prev_pos ) } );
        if ( pos != String::npos )
            pos += separator.size();
        prev_pos = pos;
    }
    return ret;
}

bool symbol_identifier_matches( const SymbolIdentifier &pattern, const SymbolIdentifier &candidate ) {
    if ( candidate.name == pattern.name ) {
        if ( candidate.eval_type == pattern.eval_type ) {
            bool matches = true;

            if ( candidate.parameters.size() < pattern.parameters.size() )
                matches = false;
            for ( size_t i = 0; i < pattern.parameters.size() && matches; i++ ) {
                if ( !( candidate.parameters[i] == pattern.parameters[i] ) )
                    matches = false;
            }

            if ( candidate.template_values.size() < pattern.template_values.size() )
                matches = false;
            for ( size_t i = 0; i < pattern.template_values.size() && matches; i++ ) {
                if ( pattern.template_values[i].first != 0 &&
                     candidate.template_values[i] != pattern.template_values[i] )
                    matches = false;
            }

            return matches;
        }
    }
    return false;
}

bool symbol_identifier_base_matches( const SymbolIdentifier &pattern, const SymbolIdentifier &candidate ) {
    if ( candidate.name == pattern.name ) {
        bool matches = true;

        if ( candidate.template_values.size() < pattern.template_values.size() )
            matches = false;
        for ( size_t i = 0; i < pattern.template_values.size() && matches; i++ ) {
            if ( pattern.template_values[i].first != 0 &&
                 candidate.template_values[i].first != pattern.template_values[i].first )
                matches = false;
        }

        return matches;
    }
    return false;
}

bool symbol_base_matches( CrateCtx &c_ctx, Worker &w_ctx, SymbolId pattern, SymbolId candidate ) {
    return c_ctx.symbol_graph[pattern].parent == c_ctx.symbol_graph[candidate].parent &&
           c_ctx.symbol_graph[pattern].identifier.name == c_ctx.symbol_graph[candidate].identifier.name;
}

std::vector<SymbolId> find_sub_symbol_by_identifier( CrateCtx &c_ctx, Worker &w_ctx, const SymbolIdentifier &identifier,
                                                     SymbolId parent ) {
    std::vector<SymbolId> ret;
    for ( auto &sub_id : c_ctx.symbol_graph[parent].sub_nodes ) {
        if ( symbol_identifier_matches( identifier, c_ctx.symbol_graph[sub_id].identifier ) )
            ret.push_back( sub_id );
    }
    return ret;
}

std::vector<SymbolId> find_global_symbol_by_identifier_chain( CrateCtx &c_ctx, Worker &w_ctx,
                                                              sptr<std::vector<SymbolIdentifier>> identifier_chain ) {
    return find_relative_symbol_by_identifier_chain( c_ctx, w_ctx, identifier_chain, ROOT_SYMBOL );
}

std::vector<SymbolId> find_relative_symbol_by_identifier_chain( CrateCtx &c_ctx, Worker &w_ctx,
                                                                sptr<std::vector<SymbolIdentifier>> identifier_chain,
                                                                SymbolId parent ) {
    SymbolId search_scope = parent;
    // Search in local scope first and then progress to outer scopes
    while ( true ) {
        std::vector<SymbolId> curr_symbols( 1, search_scope );
        for ( size_t i = 0; i < identifier_chain->size(); i++ ) {
            curr_symbols =
                find_sub_symbol_by_identifier( c_ctx, w_ctx, ( *identifier_chain )[i], curr_symbols.front() );
            if ( curr_symbols.empty() && search_scope != 0 ) {
                // Search one scope upwards
                search_scope = c_ctx.symbol_graph[search_scope].parent;
                break;
            }
            if ( i == identifier_chain->size() - 1 ) {
                return curr_symbols;
            }
            if ( curr_symbols.size() != 1 )
                return std::vector<SymbolId>();
        }
    }
}

std::vector<SymbolId> find_local_symbol_by_identifier_chain( CrateCtx &c_ctx, Worker &w_ctx,
                                                             sptr<std::vector<SymbolIdentifier>> identifier_chain ) {
    return find_relative_symbol_by_identifier_chain( c_ctx, w_ctx, identifier_chain, c_ctx.current_scope );
}

std::vector<size_t> find_member_symbol_by_identifier( CrateCtx &c_ctx, Worker &w_ctx,
                                                      const SymbolIdentifier &identifier, SymbolId parent_symbol ) {
    std::vector<size_t> ret;
    auto &parent_type = c_ctx.type_table[c_ctx.symbol_graph[parent_symbol].value];

    for ( size_t i = 0; i < parent_type.members.size(); i++ ) {
        if ( symbol_identifier_matches( identifier, parent_type.members[i].identifier ) )
            ret.push_back( i );
    }
    return ret;
}

std::vector<SymbolId> find_template_instantiations( CrateCtx &c_ctx, Worker &w_ctx, SymbolId template_symbol ) {
    // Check if it's a template
    if ( c_ctx.symbol_graph[template_symbol].identifier.template_values.empty() ) {
        LOG_ERR( "Attempted to find template instances of a non-template" );
        return std::vector<SymbolId>();
    }

    auto &identifier = c_ctx.symbol_graph[template_symbol].identifier;
    auto &parent = c_ctx.symbol_graph[c_ctx.symbol_graph[template_symbol].parent];
    std::vector<SymbolId> ret;

    for ( auto &sub : parent.sub_nodes ) {
        // Don't check template_values
        if ( c_ctx.symbol_graph[sub].identifier.name == identifier.name &&
             c_ctx.symbol_graph[sub].identifier.eval_type == identifier.eval_type &&
             c_ctx.symbol_graph[sub].identifier.parameters == identifier.parameters )
            ret.push_back( sub );
    }
    return ret;
}

String get_local_symbol_name( CrateCtx &c_ctx, Worker &w_ctx, SymbolId symbol ) {
    if ( symbol == 0 )
        return "";

    const auto &ident = c_ctx.symbol_graph[symbol].identifier;
    String ret = ident.name;

    // anonymous scope
    if ( ret.empty() ) {
        ret = "<" + to_string( symbol ) + ">";
    }

    // template arguments
    if ( !ident.template_values.empty() ) {
        ret += "<";
        for ( size_t i = 0; i < ident.template_values.size(); i++ ) {
            for ( size_t j = 0; j < ident.template_values[i].second.get_raw().size(); j++ ) {
                ret += to_string( ident.template_values[i].second.get_raw()[j] );
            }
            ret +=
                ":" + to_string( ident.template_values[i].first ) + ( i < ident.template_values.size() - 1 ? "," : "" );
        }
        ret += ">";
    }

    // parameters
    if ( !ident.parameters.empty() || ident.eval_type.type != 0 ) {
        ret += "[" + String( ident.eval_type.mut ? "mut " : "" ) + ( ident.eval_type.ref ? "&" : "" ) +
               to_string( ident.eval_type.type );
        for ( size_t i = 0; i < ident.parameters.size(); i++ ) {
            ret += "," + String( ident.parameters[i].mut ? "mut " : "" ) + ( ident.parameters[i].ref ? "&" : "" ) +
                   ident.parameters[i].name + ":" + to_string( ident.parameters[i].type );
        }
        ret += "]";
    }
    return ret;
}

String get_full_symbol_name( CrateCtx &c_ctx, Worker &w_ctx, SymbolId symbol ) {
    if ( symbol == 0 )
        return "";
    String symbol_str = get_local_symbol_name( c_ctx, w_ctx, symbol );

    SymbolId curr_symbol_id = c_ctx.symbol_graph[symbol].parent;
    while ( curr_symbol_id > ROOT_SYMBOL ) {
        symbol_str = get_local_symbol_name( c_ctx, w_ctx, curr_symbol_id ) + "::" + symbol_str;
        curr_symbol_id = c_ctx.symbol_graph[curr_symbol_id].parent;
    }
    return symbol_str;
}

sptr<std::vector<SymbolIdentifier>> get_symbol_chain_from_symbol( CrateCtx &c_ctx, Worker &w_ctx, SymbolId symbol ) {
    auto curr_symbol = symbol;
    auto ret = make_shared<std::vector<SymbolIdentifier>>();
    while ( curr_symbol != ROOT_SYMBOL ) {
        ret->insert( ret->begin(), c_ctx.symbol_graph[curr_symbol].identifier );
        curr_symbol = c_ctx.symbol_graph[curr_symbol].parent;
    }
    return ret;
}

// Merges the names of the symbol chain entries. Does not append the parameters, etc
String merge_symbol_chain( const std::vector<SymbolIdentifier> &symbol_chain ) {
    if ( symbol_chain.empty() ) {
        return "";
    } else {
        String ret = symbol_chain.front().name;
        for ( auto symbol = symbol_chain.begin() + 1; symbol != symbol_chain.end(); symbol++ ) {
            ret += "::" + symbol->name;
        }
        return ret;
    }
}

bool alias_name_chain( CrateCtx &c_ctx, Worker &w_ctx, std::vector<SymbolIdentifier> &symbol_chain,
                       const AstNode &symbol ) {
    // Tests if a given SymbolSubstitution matches the symbol_chain
    auto test = [&c_ctx, &symbol_chain]( SymbolSubstitution &ssub ) {
        if ( symbol_chain.size() < ssub.from->size() )
            return false;
        for ( size_t i = 0; i < ssub.from->size(); i++ ) {
            if ( !symbol_identifier_matches( ( *ssub.from )[i], symbol_chain[i] ) )
                return false;
        }
        return true;
    };

    for ( auto scope_itr = c_ctx.current_substitutions.rbegin(); scope_itr != c_ctx.current_substitutions.rend();
          scope_itr++ ) {
        auto first = std::find_if( scope_itr->begin(), scope_itr->end(), test );
        if ( first != scope_itr->end() ) {
            // Found a substitution rule
            auto second = std::find_if( first + 1, scope_itr->end(), test );
            if ( second != scope_itr->end() ) {
                w_ctx.print_msg<MessageType::err_ambiguous_symbol_substitution>(
                    MessageInfo( symbol, 0, FmtStr::Color::Red ), {}, merge_symbol_chain( *first->to ),
                    merge_symbol_chain( *second->to ) );
                return false;
            }

            // Apply rule and return (don't check super scopes)
            symbol_chain.erase( symbol_chain.begin(), symbol_chain.begin() + first->from->size() );
            symbol_chain.insert( symbol_chain.begin(), first->to->begin(), first->to->end() );
            return true;
        }
    }
    return true; // found no substution, but no error occurred
}

SymbolId create_new_global_symbol( CrateCtx &c_ctx, Worker &w_ctx, const String &name ) {
    if ( !find_sub_symbol_by_identifier( c_ctx, w_ctx, SymbolIdentifier{ name }, ROOT_SYMBOL ).empty() ) {
        LOG_ERR( "Attempted to create an existing global symbol '" + name + "'" );
    }
    SymbolId sym_id = c_ctx.symbol_graph.size();
    c_ctx.symbol_graph.emplace_back();
    c_ctx.symbol_graph[sym_id].identifier.name = name;
    return sym_id;
}

SymbolId create_new_relative_symbol( CrateCtx &c_ctx, Worker &w_ctx, const SymbolIdentifier &identifier,
                                     SymbolId parent_symbol ) {
    if ( !identifier.name.empty() &&
         !find_sub_symbol_by_identifier( c_ctx, w_ctx, identifier, parent_symbol ).empty() ) {
        LOG_ERR( "Attempted to create an existing non-anonymous relative symbol '" + identifier.name + "' to parent '" +
                 to_string( parent_symbol ) + "'" );
    }
    SymbolId sym_id = c_ctx.symbol_graph.size();
    c_ctx.symbol_graph.emplace_back();
    c_ctx.symbol_graph[sym_id].parent = parent_symbol;
    c_ctx.symbol_graph[parent_symbol].sub_nodes.push_back( sym_id );
    c_ctx.symbol_graph[sym_id].identifier = identifier;
    return sym_id;
}

SymbolId create_new_local_symbol( CrateCtx &c_ctx, Worker &w_ctx, const SymbolIdentifier &identifier ) {
    return create_new_relative_symbol( c_ctx, w_ctx, identifier, c_ctx.current_scope );
}

SymbolId create_new_global_symbol_from_name_chain( CrateCtx &c_ctx, Worker &w_ctx,
                                                   const sptr<std::vector<SymbolIdentifier>> symbol_chain ) {
    return create_new_relative_symbol_from_name_chain( c_ctx, w_ctx, symbol_chain, ROOT_SYMBOL );
}

SymbolId create_new_relative_symbol_from_name_chain( CrateCtx &c_ctx, Worker &w_ctx,
                                                     const sptr<std::vector<SymbolIdentifier>> symbol_chain,
                                                     SymbolId parent_symbol ) {
    SymbolId curr_symbol = parent_symbol;
    for ( int i = 0; i < symbol_chain->size(); i++ ) {
        std::vector<SymbolId> sub_symbols =
            find_sub_symbol_by_identifier( c_ctx, w_ctx, ( *symbol_chain )[i], curr_symbol );
        if ( sub_symbols.size() == 1 ) {
            curr_symbol = sub_symbols.front();

            // Test for invalid scope access
            if ( i != symbol_chain->size() - 1 && c_ctx.symbol_graph[curr_symbol].type != c_ctx.mod_type &&
                 c_ctx.symbol_graph[curr_symbol].type != 0 ) {
                w_ctx.print_msg<MessageType::err_implicit_scope_not_module>(
                    MessageInfo( *c_ctx.symbol_graph[parent_symbol].original_expr.front(), 0, FmtStr::Color::Red ),
                    { MessageInfo( *c_ctx.symbol_graph[curr_symbol].original_expr.front(), 1 ) } );
                return 0;
            }
        } else if ( sub_symbols.empty() ) { // create new symbol
            curr_symbol = create_new_relative_symbol( c_ctx, w_ctx, ( *symbol_chain )[i], curr_symbol );
            c_ctx.symbol_graph[curr_symbol].type = c_ctx.mod_type;
        } else {
            std::vector<MessageInfo> notes;
            for ( auto &s : sub_symbols ) {
                if ( !c_ctx.symbol_graph[s].original_expr.empty() )
                    notes.push_back( MessageInfo( *c_ctx.symbol_graph[s].original_expr.front(), 1 ) );
            }
            w_ctx.print_msg<MessageType::err_sub_symbol_is_ambiguous>(
                MessageInfo( *c_ctx.symbol_graph[parent_symbol].original_expr.front(), 0, FmtStr::Color::Red ), notes );
            return 0;
        }
    }
    return curr_symbol;
}

SymbolId create_new_local_symbol_from_name_chain( CrateCtx &c_ctx, Worker &w_ctx,
                                                  const sptr<std::vector<SymbolIdentifier>> symbol_chain,
                                                  const AstNode &symbol ) {
    alias_name_chain( c_ctx, w_ctx, *symbol_chain, symbol );
    return create_new_relative_symbol_from_name_chain( c_ctx, w_ctx, symbol_chain, c_ctx.current_scope );
}

void delete_symbol( CrateCtx &c_ctx, Worker &w_ctx, SymbolId to_delete ) {
    auto &symbol = c_ctx.symbol_graph[to_delete];
    c_ctx.symbol_graph[symbol.parent].sub_nodes.erase(
        std::remove( c_ctx.symbol_graph[symbol.parent].sub_nodes.begin(),
                     c_ctx.symbol_graph[symbol.parent].sub_nodes.end(), to_delete ),
        c_ctx.symbol_graph[symbol.parent].sub_nodes.end() );
    symbol.parent = 0;
    while ( !symbol.sub_nodes.empty() )
        delete_symbol( c_ctx, w_ctx, symbol.sub_nodes.front() );
    symbol.identifier = SymbolIdentifier();
    symbol.signature_evaluated = true;
    symbol.value_evaluated = true;
    symbol.signature_evaluation_ongoing = false;
    symbol.type = 0;

    // Handle type
    if ( symbol.value != 0 ) {
        auto &type = c_ctx.type_table[symbol.value];
        type.symbol = 0;
        type.members.clear();
        for ( auto &sub : type.subtypes ) {
            c_ctx.type_table[sub].supertypes.erase( std::remove( c_ctx.type_table[sub].supertypes.begin(),
                                                                 c_ctx.type_table[sub].supertypes.end(), symbol.value ),
                                                    c_ctx.type_table[sub].supertypes.end() );
        }
        for ( auto &sub : type.supertypes ) {
            c_ctx.type_table[sub].subtypes.erase( std::remove( c_ctx.type_table[sub].subtypes.begin(),
                                                               c_ctx.type_table[sub].subtypes.end(), symbol.value ),
                                                  c_ctx.type_table[sub].subtypes.end() );
        }

        // Handle function body
        if ( type.function_body != 0 ) {
            auto &fn = c_ctx.functions[type.function_body];
            fn.type = 0;
            fn.ops.clear();
            fn.vars.clear();
        }
        type.function_body = 0;
    }
    symbol.value = 0;
}

SymbolGraphNode &create_new_member_symbol( CrateCtx &c_ctx, Worker &w_ctx, const SymbolIdentifier &symbol_identifier,
                                           SymbolId parent_symbol ) {
    auto &parent_type = c_ctx.type_table[c_ctx.symbol_graph[parent_symbol].value];
    parent_type.members.emplace_back();
    parent_type.members.back().identifier = symbol_identifier;
    parent_type.members.back().parent = parent_symbol;
    return parent_type.members.back();
}

TypeId create_new_internal_type( CrateCtx &c_ctx, Worker &w_ctx ) {
    TypeId type_id = c_ctx.type_table.size();
    c_ctx.type_table.emplace_back();
    return type_id;
}

TypeId create_new_type( CrateCtx &c_ctx, Worker &w_ctx, SymbolId from_symbol ) {
    if ( c_ctx.symbol_graph[from_symbol].value != 0 )
        LOG_ERR( "Attempted to create a type on a symbol which already has a type" );
    // TODO handle if type already exists (and if symbol types match; e. g. a struct may not be a trait)

    SymbolId type_id = c_ctx.type_table.size();
    c_ctx.type_table.emplace_back();
    c_ctx.type_table[type_id].symbol = from_symbol;
    c_ctx.symbol_graph[from_symbol].value = type_id;
    return type_id;
}

SymbolId instantiate_template( CrateCtx &c_ctx, Worker &w_ctx, SymbolId from_template,
                               std::vector<std::pair<TypeId, ConstValue>> &template_values ) {
    // Search for an existing template instantiation
    auto existing_template_types = find_template_instantiations( c_ctx, w_ctx, from_template );
    if ( existing_template_types.empty() ) {
        // Its not a template (error is already generated by find_template_instantiations). Normally the template
        // itself is included in the list
        return 0;
    }
    auto first = std::find_if( existing_template_types.begin(), existing_template_types.end(), [&]( SymbolId s ) {
        return c_ctx.symbol_graph[s].identifier.template_values == template_values;
    } );

    if ( first != existing_template_types.end() ) {
        // Template already instantiated
        return *first;
    } else {
        // Create a new template instance
        // TODO maybe extract this into a clone-function
        auto identifier = c_ctx.symbol_graph[from_template].identifier;
        identifier.template_values = template_values;

        // Update identifier information
        if ( identifier.eval_type.template_type_index != 0 ) {
            identifier.eval_type.type =
                *template_values[identifier.eval_type.template_type_index].second.get<TypeId>().value();
            identifier.eval_type.template_type_index = 0;
        }
        for ( auto &p : identifier.parameters ) {
            if ( p.template_type_index != 0 ) {
                p.type = *template_values[p.template_type_index].second.get<TypeId>().value();
                p.template_type_index = 0;
            }
        }

        auto &parent = c_ctx.symbol_graph[from_template].parent;
        auto new_symbol = create_new_relative_symbol( c_ctx, w_ctx, identifier, parent );
        auto new_type = create_new_type( c_ctx, w_ctx, new_symbol );

        // Copy symbol information
        auto &symbol = c_ctx.symbol_graph[new_symbol];
        symbol.sub_nodes = c_ctx.symbol_graph[from_template].sub_nodes; // TODO instantiate template methods aswell?
        symbol.original_expr = c_ctx.symbol_graph[from_template].original_expr;
        symbol.identifier = identifier;
        symbol.pub = c_ctx.symbol_graph[from_template].pub;
        symbol.signature_evaluated = true;
        symbol.proposed = true;
        symbol.compiler_annotations = c_ctx.symbol_graph[from_template].compiler_annotations;
        symbol.type = ( c_ctx.symbol_graph[from_template].type == c_ctx.template_struct_type
                            ? c_ctx.struct_type
                            : c_ctx.symbol_graph[from_template].type == c_ctx.template_fn_type ? c_ctx.fn_type
                                                                                               : c_ctx.trait_type );

        // Copy type information
        c_ctx.type_table[new_type] = c_ctx.type_table[c_ctx.symbol_graph[from_template].value];
        c_ctx.type_table[new_type].symbol = new_symbol; // fix symbol
        c_ctx.type_table[new_type].supertypes.push_back( c_ctx.symbol_graph[from_template].value );
        c_ctx.type_table[c_ctx.symbol_graph[from_template].value].subtypes.push_back( new_type );

        // Set member types
        for ( auto &m : c_ctx.type_table[new_type].members ) {
            if ( m.template_type_index != 0 ) {
                m.value = template_values[m.template_type_index].first;
            }
        }

        // Copy the function body
        if ( c_ctx.type_table[new_type].function_body != 0 ) {
            FunctionImplId new_func = c_ctx.functions.size();
            c_ctx.functions.push_back( c_ctx.functions[c_ctx.type_table[new_type].function_body] );
            c_ctx.functions[new_func].type = new_type;
            c_ctx.type_table[new_type].function_body = new_func;
        }

        return new_symbol;
    }
}

void switch_scope_to_symbol( CrateCtx &c_ctx, Worker &w_ctx, SymbolId new_scope ) {
    c_ctx.current_scope = new_scope;
}

void pop_scope( CrateCtx &c_ctx, Worker &w_ctx ) {
    if ( c_ctx.symbol_graph[c_ctx.current_scope].parent == 0 ) {
        LOG_ERR( "Attempted to switch to the parent scope of the root scope" );
        return;
    }
    switch_scope_to_symbol( c_ctx, w_ctx, c_ctx.symbol_graph[c_ctx.current_scope].parent );
}

bool expect_exactly_one_symbol( CrateCtx &c_ctx, Worker &w_ctx, std::vector<SymbolId> &container,
                                const AstNode &symbol ) {
    if ( container.empty() ) {
        w_ctx.print_msg<MessageType::err_symbol_not_found>( MessageInfo( symbol, 0, FmtStr::Color::Red ) );
        return false;
    } else if ( container.size() > 1 ) {
        std::vector<MessageInfo> notes;
        for ( auto &t : container ) {
            if ( !c_ctx.symbol_graph[t].original_expr.empty() )
                notes.push_back( MessageInfo( *c_ctx.symbol_graph[t].original_expr.front(), 1 ) );
        }
        w_ctx.print_msg<MessageType::err_symbol_is_ambiguous>( MessageInfo( symbol, 0, FmtStr::Color::Red ), notes );
        return false;
    } else {
        return true;
    }
}

bool expect_unscoped_variable( CrateCtx &c_ctx, Worker &w_ctx, std::vector<SymbolIdentifier> &symbol_chain,
                               const AstNode &symbol ) {
    if ( symbol_chain.size() != 1 || !symbol_chain.front().template_values.empty() ) {
        w_ctx.print_msg<MessageType::err_local_variable_scoped>( MessageInfo( symbol, 0, FmtStr::Color::Red ) );
        return false;
    } else {
        return true;
    }
}

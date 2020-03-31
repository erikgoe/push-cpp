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

// Checks if a symbol identifier matches a symbol identifier pattern
bool symbol_identifier_matches( const SymbolIdentifier &pattern, const SymbolIdentifier &candidate ) {
    if ( candidate.name == pattern.name ) {
        if ( pattern.eval_type == 0 || candidate.eval_type == pattern.eval_type ) {
            bool matches = true;

            if ( candidate.parameters.size() < pattern.parameters.size() )
                matches = false;
            for ( size_t i = 0; i < pattern.parameters.size() && matches; i++ ) {
                if ( pattern.parameters[i].first != 0 && candidate.parameters[i].first != pattern.parameters[i].first )
                    matches = false;
            }

            if ( candidate.template_values.size() < pattern.template_values.size() )
                matches = false;
            for ( size_t i = 0; i < pattern.template_values.size() && matches; i++ ) {
                if ( pattern.template_values[i].first != 0 &&
                     candidate.template_values[i].first != pattern.template_values[i].first )
                    matches = false;
            }

            return matches;
        }
    }
    return false;
}

std::vector<SymbolId> find_sub_symbol_by_identifier( CrateCtx &c_ctx, const SymbolIdentifier &identifier,
                                                     SymbolId parent ) {
    std::vector<SymbolId> ret;
    for ( auto &sub_id : c_ctx.symbol_graph[parent].sub_nodes ) {
        if ( symbol_identifier_matches( identifier, c_ctx.symbol_graph[sub_id].identifier ) )
            ret.push_back( sub_id );
    }
    return ret;
}

// TODO split into global, relative and local implementations (including aliasing)
std::vector<SymbolId> find_sub_symbol_by_identifier_chain( CrateCtx &c_ctx,
                                                           sptr<std::vector<SymbolIdentifier>> identifier_chain,
                                                           SymbolId parent ) {
    SymbolId search_scope = parent;
    // Search in local scope first and then progress to outer scopes
    while ( true ) {
        std::vector<SymbolId> curr_symbols( 1, search_scope );
        for ( size_t i = 0; i < identifier_chain->size(); i++ ) {
            curr_symbols = find_sub_symbol_by_identifier( c_ctx, ( *identifier_chain )[i], curr_symbols.front() );
            if ( i == identifier_chain->size() - 1 ) {
                if ( curr_symbols.empty() && search_scope != 0 ) {
                    // Search one scope upwards
                    search_scope = c_ctx.symbol_graph[search_scope].parent;
                    break;
                }
                return curr_symbols;
            }
            if ( curr_symbols.size() != 1 )
                return std::vector<SymbolId>();
        }
    }
}

std::vector<size_t> find_member_symbol_by_identifier( CrateCtx &c_ctx, const SymbolIdentifier &identifier,
                                                      SymbolId parent_symbol ) {
    std::vector<size_t> ret;
    auto &parent_type = c_ctx.type_table[c_ctx.symbol_graph[parent_symbol].value];

    for ( size_t i = 0; i < parent_type.members.size(); i++ ) {
        if ( symbol_identifier_matches( identifier, parent_type.members[i].identifier ) )
            ret.push_back( i );
    }
    return ret;
}

String get_local_symbol_name( CrateCtx &c_ctx, SymbolId symbol ) {
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
            for ( size_t j = 0; j < ident.template_values[i].second.data_size; j++ ) {
                ret += to_string( ident.template_values[i].second.data[j] );
            }
            ret +=
                ":" + to_string( ident.template_values[i].first ) + ( i < ident.template_values.size() - 1 ? "," : "" );
        }
        ret += ">";
    }

    // parameters
    if ( !ident.parameters.empty() || ident.eval_type != 0 ) {
        ret += "[" + to_string( ident.eval_type );
        for ( size_t i = 0; i < ident.parameters.size(); i++ ) {
            ret += "," + ident.parameters[i].second + ":" + to_string( ident.parameters[i].first );
        }
        ret += "]";
    }
    return ret;
}

String get_full_symbol_name( CrateCtx &c_ctx, SymbolId symbol ) {
    if ( symbol == 0 )
        return "";
    String symbol_str = get_local_symbol_name( c_ctx, symbol );

    SymbolId curr_symbol_id = c_ctx.symbol_graph[symbol].parent;
    while ( curr_symbol_id > ROOT_SYMBOL ) {
        symbol_str = get_local_symbol_name( c_ctx, curr_symbol_id ) + "::" + symbol_str;
        curr_symbol_id = c_ctx.symbol_graph[curr_symbol_id].parent;
    }
    return symbol_str;
}

bool alias_name_chain( CrateCtx &c_ctx, std::vector<SymbolIdentifier> &symbol_chain ) {
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
            auto second = std::find_if( first, scope_itr->end(), test );
            if ( second != scope_itr->end() ) {
                LOG_ERR( "Ambiguous symbol substitution" );
                return false; // TODO create error message
            }

            // Apply rule and return (don't check super scopes)
            symbol_chain.erase( symbol_chain.begin(), symbol_chain.begin() + first->from->size() );
            symbol_chain.insert( symbol_chain.begin(), first->to->begin(), first->to->end() );
            return true;
        }
    }
    return true; // found no substution, but no error occurred
}

SymbolId create_new_global_symbol( CrateCtx &c_ctx, const String &name ) {
    if ( !find_sub_symbol_by_identifier( c_ctx, SymbolIdentifier{ name }, ROOT_SYMBOL ).empty() ) {
        LOG_ERR( "Attempted to create an existing global symbol '" + name + "'" );
    }
    SymbolId sym_id = c_ctx.symbol_graph.size();
    c_ctx.symbol_graph.emplace_back();
    c_ctx.symbol_graph[sym_id].identifier.name = name;
    return sym_id;
}

SymbolId create_new_relative_symbol( CrateCtx &c_ctx, const String &name, SymbolId parent_symbol ) {
    if ( !name.empty() && !find_sub_symbol_by_identifier( c_ctx, SymbolIdentifier{ name }, parent_symbol ).empty() ) {
        LOG_ERR( "Attempted to create an existing non-anonymous relative symbol '" + name + "' to parent '" +
                 to_string( parent_symbol ) + "'" );
    }
    SymbolId sym_id = c_ctx.symbol_graph.size();
    c_ctx.symbol_graph.emplace_back();
    c_ctx.symbol_graph[sym_id].parent = parent_symbol;
    c_ctx.symbol_graph[parent_symbol].sub_nodes.push_back( sym_id );
    c_ctx.symbol_graph[sym_id].identifier.name = name;
    return sym_id;
}

SymbolId create_new_local_symbol( CrateCtx &c_ctx, const String &name ) {
    return create_new_relative_symbol( c_ctx, name, c_ctx.current_scope );
}

SymbolId create_new_global_symbol_from_name_chain( CrateCtx &c_ctx,
                                                   const sptr<std::vector<SymbolIdentifier>> symbol_chain ) {
    return create_new_relative_symbol_from_name_chain( c_ctx, symbol_chain, ROOT_SYMBOL );
}

SymbolId create_new_relative_symbol_from_name_chain( CrateCtx &c_ctx,
                                                     const sptr<std::vector<SymbolIdentifier>> symbol_chain,
                                                     SymbolId parent_symbol ) {
    SymbolId curr_symbol = parent_symbol;
    for ( int i = 0; i < symbol_chain->size(); i++ ) {
        std::vector<SymbolId> sub_symbols = find_sub_symbol_by_identifier( c_ctx, ( *symbol_chain )[i], curr_symbol );
        if ( sub_symbols.size() == 1 ) {
            curr_symbol = sub_symbols.front();

            // Test for invalid scope access
            if ( i != symbol_chain->size() - 1 && c_ctx.symbol_graph[curr_symbol].type != c_ctx.mod_type &&
                 c_ctx.symbol_graph[curr_symbol].type != 0 ) {
                LOG_ERR( "Implicit scope is not a module" );
                return 0; // TODO create error message
            }
        } else if ( sub_symbols.empty() ) { // create new symbol
            curr_symbol = create_new_relative_symbol( c_ctx, ( *symbol_chain )[i].name, curr_symbol );
            c_ctx.symbol_graph[curr_symbol].type = c_ctx.mod_type;
        } else {
            LOG_ERR( "Symbol chain is not unique" );
            return 0; // TODO create error message, because symbol chain is not unique d
        }
    }
    return curr_symbol;
}

SymbolId create_new_local_symbol_from_name_chain( CrateCtx &c_ctx,
                                                  const sptr<std::vector<SymbolIdentifier>> symbol_chain ) {
    alias_name_chain( c_ctx, *symbol_chain );
    return create_new_relative_symbol_from_name_chain( c_ctx, symbol_chain, c_ctx.current_scope );
}

SymbolGraphNode &create_new_member_symbol( CrateCtx &c_ctx, const SymbolIdentifier &symbol_identifier,
                                           SymbolId parent_symbol ) {
    auto &parent_type = c_ctx.type_table[c_ctx.symbol_graph[parent_symbol].value];
    parent_type.members.emplace_back();
    parent_type.members.back().identifier = symbol_identifier;
    parent_type.members.back().parent = parent_symbol;
    return parent_type.members.back();
}

TypeId create_new_internal_type( CrateCtx &c_ctx ) {
    SymbolId type_id = c_ctx.type_table.size();
    c_ctx.type_table.emplace_back();
    return type_id;
}

TypeId create_new_type( CrateCtx &c_ctx, SymbolId from_symbol ) {
    if ( c_ctx.symbol_graph[from_symbol].value != 0 )
        LOG_ERR( "Attempted to create a type on a symbol which already has a type" );
    SymbolId type_id = c_ctx.type_table.size();
    c_ctx.type_table.emplace_back();
    c_ctx.type_table[type_id].symbol = from_symbol;
    c_ctx.symbol_graph[from_symbol].value = type_id;
    return type_id;
}

void switch_scope_to_symbol( CrateCtx &c_ctx, SymbolId new_scope ) {
    c_ctx.current_scope = new_scope;
}

void pop_scope( CrateCtx &c_ctx ) {
    if ( c_ctx.symbol_graph[c_ctx.current_scope].parent == 0 ) {
        LOG_ERR( "Attempted to switch to the parent scope of the root scope" );
        return;
    }
    switch_scope_to_symbol( c_ctx, c_ctx.symbol_graph[c_ctx.current_scope].parent );
}

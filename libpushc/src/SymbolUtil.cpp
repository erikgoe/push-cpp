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

SymbolId find_sub_symbol_by_name( CrateCtx &c_ctx, const String &name, SymbolId parent ) {
    for ( auto &sub_id : c_ctx.symbol_graph[parent].sub_nodes )
        if ( c_ctx.symbol_graph[sub_id].identifier.name == name )
            return sub_id;
    return 0;
}

SymbolId find_sub_symbol_by_name_chain( CrateCtx &c_ctx, sptr<std::vector<SymbolIdentifier>> name_chain,
                                        SymbolId parent ) {
    SymbolId curr_symbol = parent;
    for ( auto &symbol : *name_chain ) {
        curr_symbol = find_sub_symbol_by_name( c_ctx, symbol.name, curr_symbol );
        if ( curr_symbol == 0 )
            return 0;
    }
    return curr_symbol;
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
    if ( !ident.parameters.empty() ) {
        ret += "[";
        for ( size_t i = 0; i < ident.parameters.size(); i++ ) {
            ret += ident.parameters[i].second + ":" + to_string( ident.parameters[i].first ) +
                   ( i < ident.parameters.size() - 1 ? "," : "" );
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

SymbolId create_new_global_symbol( CrateCtx &c_ctx, const String &name ) {
    if ( find_sub_symbol_by_name( c_ctx, name, ROOT_SYMBOL ) ) {
        LOG_ERR( "Attempted to create an existing global symbol '" + name + "'" );
    }
    SymbolId sym_id = c_ctx.symbol_graph.size();
    c_ctx.symbol_graph.emplace_back();
    c_ctx.symbol_graph[sym_id].identifier.name = name;
    return sym_id;
}

SymbolId create_new_relative_symbol( CrateCtx &c_ctx, const String &name, SymbolId parent_symbol ) {
    if ( !name.empty() && find_sub_symbol_by_name( c_ctx, name, parent_symbol ) ) {
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
    for ( auto &symbol : *symbol_chain ) {
        SymbolId sub_symbol = find_sub_symbol_by_name( c_ctx, symbol.name, curr_symbol );
        if ( sub_symbol != 0 ) {
            curr_symbol = sub_symbol;
        } else { // create new symbol
            curr_symbol = create_new_relative_symbol( c_ctx, symbol.name, curr_symbol );
        }
    }
    return curr_symbol;
}

SymbolId create_new_local_symbol_from_name_chain( CrateCtx &c_ctx,
                                                  const sptr<std::vector<SymbolIdentifier>> symbol_chain ) {
    return create_new_relative_symbol_from_name_chain( c_ctx, symbol_chain, c_ctx.current_scope );
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

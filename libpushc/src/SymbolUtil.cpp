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

sptr<std::vector<String>> split_symbol_chain( const String &chained, String separator ) {
    auto ret = make_shared<std::vector<String>>();
    size_t pos = 0;
    size_t prev_pos = pos;
    while ( pos != String::npos ) {
        pos = chained.find( separator, pos );
        ret->push_back( chained.slice( prev_pos, pos - prev_pos ) );
        if ( pos != String::npos )
            pos += separator.size();
        prev_pos = pos;
    }
    return ret;
}

SymbolId find_sub_symbol_by_name( CrateCtx &c_ctx, const String &name, SymbolId parent ) {
    for ( auto &sub_id : c_ctx.symbol_graph[parent].sub_nodes )
        if ( c_ctx.symbol_graph[sub_id].name == name )
            return sub_id;
    return 0;
}

SymbolId find_sub_symbol_by_name_chain( CrateCtx &c_ctx, sptr<std::vector<String>> name_chain, SymbolId parent ) {
    SymbolId curr_symbol = parent;
    for ( auto &symbol : *name_chain ) {
        curr_symbol = find_sub_symbol_by_name( c_ctx, symbol, curr_symbol );
        if ( curr_symbol == 0 )
            return 0;
    }
    return curr_symbol;
}

String get_full_symbol_name( CrateCtx &c_ctx, SymbolId symbol ) {
    if ( symbol == 0 )
        return "";
    String symbol_str = c_ctx.symbol_graph[symbol].name;
    SymbolId curr_symbol_id = c_ctx.symbol_graph[symbol].parent;
    while ( curr_symbol_id > ROOT_SYMBOL ) {
        symbol_str = c_ctx.symbol_graph[curr_symbol_id].name + "::" + symbol_str;
        curr_symbol_id = c_ctx.symbol_graph[curr_symbol_id].parent;
    }
    return symbol_str;
}

SymbolId create_new_global_symbol( CrateCtx &c_ctx, const String &name ) {
    if ( find_sub_symbol_by_name( c_ctx, name, ROOT_SYMBOL ) ) {
        LOG_ERR( "Attempted to create a existing global symbol '" + name + "'" );
    }
    SymbolId sym_id = c_ctx.symbol_graph.size();
    c_ctx.symbol_graph.emplace_back();
    c_ctx.symbol_graph[sym_id].name = name;
    return sym_id;
}

SymbolId create_new_relative_symbol( CrateCtx &c_ctx, const String &name, SymbolId parent_symbol ) {
    if ( find_sub_symbol_by_name( c_ctx, name, parent_symbol ) ) {
        LOG_ERR( "Attempted to create a existing local symbol '" + name + "' to parent '" + to_string( parent_symbol ) +
                 "'" );
    }
    SymbolId sym_id = c_ctx.symbol_graph.size();
    c_ctx.symbol_graph.emplace_back();
    c_ctx.symbol_graph[sym_id].parent = parent_symbol;
    c_ctx.symbol_graph[parent_symbol].sub_nodes.push_back( sym_id );
    c_ctx.symbol_graph[sym_id].name = name;
    return sym_id;
}

SymbolId create_new_local_symbol( CrateCtx &c_ctx, const String &name ) {
    return create_new_relative_symbol( c_ctx, name, c_ctx.current_scope );
}

SymbolId create_new_global_symbol_from_name_chain( CrateCtx &c_ctx, const sptr<std::vector<String>> symbol_chain ) {
    SymbolId curr_symbol = ROOT_SYMBOL; // starting with the global root
    for ( auto &symbol : *symbol_chain ) {
        SymbolId sub_symbol = find_sub_symbol_by_name( c_ctx, symbol, curr_symbol );
        if ( sub_symbol != 0 ) {
            curr_symbol = sub_symbol;
        } else { // create new symbol
            curr_symbol = create_new_relative_symbol( c_ctx, symbol, curr_symbol );
        }
    }
    return curr_symbol;
}

TypeId create_new_type( CrateCtx &c_ctx, SymbolId from_symbol ) {
    if ( c_ctx.symbol_graph[from_symbol].type != 0 )
        LOG_ERR( "Attempted to create a type on a symbol which already has a type" );
    SymbolId type_id = c_ctx.type_table.size();
    c_ctx.type_table.emplace_back();
    c_ctx.type_table[type_id].symbol = from_symbol;
    c_ctx.symbol_graph[from_symbol].type = type_id;
    return type_id;
}

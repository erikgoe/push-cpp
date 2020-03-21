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
#include "libpushc/Expression.h"
#include "libpushc/SymbolUtil.h"

sptr<std::vector<String>> get_symbol_chain_from_expr( sptr<Expr> expr ) {
    if ( auto atomic_symbol = std::dynamic_pointer_cast<AtomicSymbolExpr>( expr ); atomic_symbol ) {
        return make_shared<std::vector<String>>( 1, atomic_symbol->symbol_name );
    } else if ( auto scope_symbol = std::dynamic_pointer_cast<ScopeAccessExpr>( expr ); scope_symbol ) {
        auto base = get_symbol_chain_from_expr( scope_symbol->base );
        auto name = get_symbol_chain_from_expr( scope_symbol->name );
        base->insert( base->end(), name->begin(), name->end() );
        return base;
    }
    return nullptr;
}

String Expr::get_additional_debug_data() {
    String result;
    if ( !static_statements.empty() ) {
        result += "$(";
        for ( auto &stst : static_statements ) {
            result += stst->get_debug_repr() + ", ";
        }
        result += ")";
    }
    return result;
}

void BlockExpr::pre_symbol_discovery( CrateCtx &c_ctx ) {
    switch_scope_to_symbol( c_ctx, create_new_local_symbol( c_ctx, "" ) );
}

void BlockExpr::symbol_discovery( CrateCtx &c_ctx ) {
    pop_scope( c_ctx );
}

void FuncExpr::pre_symbol_discovery( CrateCtx &c_ctx ) {
    if ( std::dynamic_pointer_cast<SymbolExpr>( symbol ) )
        switch_scope_to_symbol(
            c_ctx, create_new_local_symbol_from_name_chain( c_ctx, get_symbol_chain_from_expr( symbol ) ) );
}

void FuncExpr::symbol_discovery( CrateCtx &c_ctx ) {
    pop_scope( c_ctx );
}

void IfExpr::pre_symbol_discovery( CrateCtx &c_ctx ) {
    switch_scope_to_symbol( c_ctx, create_new_local_symbol( c_ctx, "" ) );
}

void IfExpr::symbol_discovery( CrateCtx &c_ctx ) {
    pop_scope( c_ctx );
}

void IfElseExpr::pre_symbol_discovery( CrateCtx &c_ctx ) {
    switch_scope_to_symbol( c_ctx, create_new_local_symbol( c_ctx, "" ) );
}

void IfElseExpr::symbol_discovery( CrateCtx &c_ctx ) {
    pop_scope( c_ctx );
}

void PreLoopExpr::pre_symbol_discovery( CrateCtx &c_ctx ) {
    switch_scope_to_symbol( c_ctx, create_new_local_symbol( c_ctx, "" ) );
}

void PreLoopExpr::symbol_discovery( CrateCtx &c_ctx ) {
    pop_scope( c_ctx );
}

void PostLoopExpr::pre_symbol_discovery( CrateCtx &c_ctx ) {
    switch_scope_to_symbol( c_ctx, create_new_local_symbol( c_ctx, "" ) );
}

void PostLoopExpr::symbol_discovery( CrateCtx &c_ctx ) {
    pop_scope( c_ctx );
}

void InfLoopExpr::pre_symbol_discovery( CrateCtx &c_ctx ) {
    switch_scope_to_symbol( c_ctx, create_new_local_symbol( c_ctx, "" ) );
}

void InfLoopExpr::symbol_discovery( CrateCtx &c_ctx ) {
    pop_scope( c_ctx );
}

void ItrLoopExpr::pre_symbol_discovery( CrateCtx &c_ctx ) {
    switch_scope_to_symbol( c_ctx, create_new_local_symbol( c_ctx, "" ) );
}

void ItrLoopExpr::symbol_discovery( CrateCtx &c_ctx ) {
    pop_scope( c_ctx );
}

void MatchExpr::pre_symbol_discovery( CrateCtx &c_ctx ) {
    switch_scope_to_symbol( c_ctx, create_new_local_symbol( c_ctx, "" ) );
}

void MatchExpr::symbol_discovery( CrateCtx &c_ctx ) {
    pop_scope( c_ctx );
}

void StructExpr::pre_symbol_discovery( CrateCtx &c_ctx ) {
    if ( std::dynamic_pointer_cast<SymbolExpr>( name ) )
        switch_scope_to_symbol( c_ctx,
                                create_new_local_symbol_from_name_chain( c_ctx, get_symbol_chain_from_expr( name ) ) );
}

void StructExpr::symbol_discovery( CrateCtx &c_ctx ) {
    pop_scope( c_ctx );
}

void TraitExpr::pre_symbol_discovery( CrateCtx &c_ctx ) {
    if ( std::dynamic_pointer_cast<SymbolExpr>( name ) )
        switch_scope_to_symbol( c_ctx,
                                create_new_local_symbol_from_name_chain( c_ctx, get_symbol_chain_from_expr( name ) ) );
}

void TraitExpr::symbol_discovery( CrateCtx &c_ctx ) {
    pop_scope( c_ctx );
}

void ImplExpr::pre_symbol_discovery( CrateCtx &c_ctx ) {
    if ( std::dynamic_pointer_cast<SymbolExpr>( struct_name ) )
        switch_scope_to_symbol(
            c_ctx, create_new_local_symbol_from_name_chain( c_ctx, get_symbol_chain_from_expr( struct_name ) ) );
}

void ImplExpr::symbol_discovery( CrateCtx &c_ctx ) {
    pop_scope( c_ctx );
}

void ModuleExpr::pre_symbol_discovery( CrateCtx &c_ctx ) {
    if ( std::dynamic_pointer_cast<SymbolExpr>( symbol ) )
        switch_scope_to_symbol(
            c_ctx, create_new_local_symbol_from_name_chain( c_ctx, get_symbol_chain_from_expr( symbol ) ) );
}

void ModuleExpr::symbol_discovery( CrateCtx &c_ctx ) {
    pop_scope( c_ctx );
}

void StaticStatementExpr::pre_symbol_discovery( CrateCtx &c_ctx ) {
    switch_scope_to_symbol( c_ctx, create_new_local_symbol( c_ctx, "" ) );
}

void StaticStatementExpr::symbol_discovery( CrateCtx &c_ctx ) {
    pop_scope( c_ctx );
}

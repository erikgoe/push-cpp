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

// Defined here, because libpush does not define the Expr symbol
MessageInfo::MessageInfo( const sptr<Expr> &expr, u32 message_idx, FmtStr::Color color )
        : MessageInfo( expr->get_position_info(), message_idx, color ) {}

sptr<std::vector<SymbolIdentifier>> get_symbol_chain_from_expr( sptr<SymbolExpr> expr ) {
    if ( auto atomic_symbol = std::dynamic_pointer_cast<AtomicSymbolExpr>( expr ); atomic_symbol ) {
        return make_shared<std::vector<SymbolIdentifier>>( 1, SymbolIdentifier{ atomic_symbol->symbol_name } );
    } else if ( auto scope_symbol = std::dynamic_pointer_cast<ScopeAccessExpr>( expr ); scope_symbol ) {
        auto base = get_symbol_chain_from_expr( std::dynamic_pointer_cast<SymbolExpr>( scope_symbol->base ) );
        auto name = get_symbol_chain_from_expr( std::dynamic_pointer_cast<SymbolExpr>( scope_symbol->name ) );
        base->insert( base->end(), name->begin(), name->end() );
        return base;
    } else if ( auto template_symbol = std::dynamic_pointer_cast<TemplateExpr>( expr ); template_symbol ) {
        return get_symbol_chain_from_expr(
            std::dynamic_pointer_cast<SymbolExpr>( template_symbol->symbol ) ); // TODO add template arguments
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

void BlockExpr::pre_symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    switch_scope_to_symbol( c_ctx, create_new_local_symbol( c_ctx, "" ) );
}

void BlockExpr::symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    pop_scope( c_ctx );
}

void FuncExpr::pre_symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    if ( auto symbol_symbol = std::dynamic_pointer_cast<SymbolExpr>( symbol ); symbol_symbol ) {
        SymbolId new_id = create_new_local_symbol_from_name_chain( c_ctx, get_symbol_chain_from_expr( symbol_symbol ) );
        switch_scope_to_symbol( c_ctx, new_id );
        symbol_symbol->update_symbol_id( new_id );
        if ( return_type != 0 ) {
            auto return_symbols = find_sub_symbol_by_identifier_chain(
                c_ctx, get_symbol_chain_from_expr( std::dynamic_pointer_cast<SymbolExpr>( return_type ) ),
                c_ctx.current_scope );
            if ( return_symbols.empty() ) {
                w_ctx.print_msg<MessageType::err_symbol_not_found>( MessageInfo( return_type, 0, FmtStr::Color::Red ) );
                return;
            } else if ( return_symbols.size() != 1 ) {
                std::vector<MessageInfo> notes;
                for ( auto &rs : return_symbols ) {
                    if ( !c_ctx.symbol_graph[rs].original_expr.empty() )
                        notes.push_back( MessageInfo( c_ctx.symbol_graph[rs].original_expr.front(), 1 ) );
                }
                w_ctx.print_msg<MessageType::err_symbol_is_ambiguous>( MessageInfo( return_type, 0, FmtStr::Color::Red ), notes );
            }
            c_ctx.symbol_graph[new_id].identifier.eval_type = return_symbols.front();
        }
    }
}

void FuncExpr::symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    pop_scope( c_ctx );
}

void IfExpr::pre_symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    switch_scope_to_symbol( c_ctx, create_new_local_symbol( c_ctx, "" ) );
}

void IfExpr::symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    pop_scope( c_ctx );
}

void IfElseExpr::pre_symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    switch_scope_to_symbol( c_ctx, create_new_local_symbol( c_ctx, "" ) );
}

void IfElseExpr::symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    pop_scope( c_ctx );
}

void PreLoopExpr::pre_symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    switch_scope_to_symbol( c_ctx, create_new_local_symbol( c_ctx, "" ) );
}

void PreLoopExpr::symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    pop_scope( c_ctx );
}

void PostLoopExpr::pre_symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    switch_scope_to_symbol( c_ctx, create_new_local_symbol( c_ctx, "" ) );
}

void PostLoopExpr::symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    pop_scope( c_ctx );
}

void InfLoopExpr::pre_symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    switch_scope_to_symbol( c_ctx, create_new_local_symbol( c_ctx, "" ) );
}

void InfLoopExpr::symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    pop_scope( c_ctx );
}

void ItrLoopExpr::pre_symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    switch_scope_to_symbol( c_ctx, create_new_local_symbol( c_ctx, "" ) );
}

void ItrLoopExpr::symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    pop_scope( c_ctx );
}

void MatchExpr::pre_symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    switch_scope_to_symbol( c_ctx, create_new_local_symbol( c_ctx, "" ) );
}

void MatchExpr::symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    pop_scope( c_ctx );
}

void StructExpr::pre_symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    if ( auto symbol_symbol = std::dynamic_pointer_cast<SymbolExpr>( name ); symbol_symbol ) {
        SymbolId new_id = create_new_local_symbol_from_name_chain( c_ctx, get_symbol_chain_from_expr( symbol_symbol ) );
        switch_scope_to_symbol( c_ctx, new_id );
        symbol_symbol->update_symbol_id( new_id );
    }
}

void StructExpr::symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    pop_scope( c_ctx );
}

void TraitExpr::pre_symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    if ( auto symbol_symbol = std::dynamic_pointer_cast<SymbolExpr>( name ); symbol_symbol ) {
        SymbolId new_id = create_new_local_symbol_from_name_chain( c_ctx, get_symbol_chain_from_expr( symbol_symbol ) );
        switch_scope_to_symbol( c_ctx, new_id );
        symbol_symbol->update_symbol_id( new_id );
    }
}

void TraitExpr::symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    pop_scope( c_ctx );
}

void ImplExpr::pre_symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    if ( auto symbol_symbol = std::dynamic_pointer_cast<SymbolExpr>( struct_name ); symbol_symbol ) {
        SymbolId new_id = create_new_local_symbol_from_name_chain( c_ctx, get_symbol_chain_from_expr( symbol_symbol ) );
        switch_scope_to_symbol( c_ctx, new_id );
        symbol_symbol->update_symbol_id( new_id );
    }
}

void ImplExpr::symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    pop_scope( c_ctx );
}

void ModuleExpr::pre_symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    if ( auto symbol_symbol = std::dynamic_pointer_cast<SymbolExpr>( symbol ); symbol_symbol ) {
        SymbolId new_id = create_new_local_symbol_from_name_chain( c_ctx, get_symbol_chain_from_expr( symbol_symbol ) );
        switch_scope_to_symbol( c_ctx, new_id );
        symbol_symbol->update_symbol_id( new_id );
    }
}

void ModuleExpr::symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    pop_scope( c_ctx );
}

void StaticStatementExpr::pre_symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    switch_scope_to_symbol( c_ctx, create_new_local_symbol( c_ctx, "" ) );
}

void StaticStatementExpr::symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    pop_scope( c_ctx );
}

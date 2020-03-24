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

bool TokenExpr::primitive_semantic_check( CrateCtx &c_ctx, Worker &w_ctx ) {
    w_ctx.print_msg<MessageType::err_orphan_token>( MessageInfo( t, 0, FmtStr::Color::Red ) );
    return false;
}

bool SingleCompletedExpr::primitive_semantic_check( CrateCtx &c_ctx, Worker &w_ctx ) {
    if ( std::dynamic_pointer_cast<CompletedExpr>( sub_expr ) != nullptr ) { // double semicolon
        w_ctx.print_msg<MessageType::err_semicolon_without_meaning>(
            MessageInfo( shared_from_this(), 0, FmtStr::Color::Red ) );
        return false;
    }
    return true;
}

bool BlockExpr::primitive_semantic_check( CrateCtx &c_ctx, Worker &w_ctx ) {
    // Check for missing semicolons (except the last expr)
    for ( auto expr = sub_expr.begin(); expr != sub_expr.end() && expr != sub_expr.end() - 1; expr++ ) {
        if ( std::dynamic_pointer_cast<CompletedExpr>( *expr ) == nullptr ) {
            w_ctx.print_msg<MessageType::err_unfinished_expr>( MessageInfo( *expr, 0, FmtStr::Color::Red ) );
            return false;
        }
    }
    return true;
}

void BlockExpr::pre_symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    SymbolId new_id = create_new_local_symbol( c_ctx, "" );
    switch_scope_to_symbol( c_ctx, new_id );
    c_ctx.symbol_graph[new_id].original_expr.push_back( shared_from_this() );
}

void BlockExpr::symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    pop_scope( c_ctx );
}

bool FuncHeadExpr::primitive_semantic_check( CrateCtx &c_ctx, Worker &w_ctx ) {
    if ( std::dynamic_pointer_cast<SymbolExpr>( symbol ) == nullptr ) {
        w_ctx.print_msg<MessageType::err_expected_symbol>( MessageInfo( symbol, 0, FmtStr::Color::Red ) );
        return false;
    }
    if ( parameters ) {
        if ( auto paren_expr = std::dynamic_pointer_cast<ParenthesisExpr>( parameters ); paren_expr != nullptr ) {
            for ( auto &entry : paren_expr->get_list() ) {
                if ( std::dynamic_pointer_cast<SymbolExpr>( entry ) == nullptr &&
                     std::dynamic_pointer_cast<TypedExpr>( entry ) == nullptr ) {
                    w_ctx.print_msg<MessageType::err_expected_symbol>( MessageInfo( entry, 0, FmtStr::Color::Red ) );
                    return false;
                }
            }
        } else {
            w_ctx.print_msg<MessageType::err_expected_parametes>( MessageInfo( parameters, 0, FmtStr::Color::Red ) );
            return false;
        }
    }
    return true;
}

bool FuncExpr::primitive_semantic_check( CrateCtx &c_ctx, Worker &w_ctx ) {
    if ( symbol && std::dynamic_pointer_cast<SymbolExpr>( symbol ) == nullptr ) {
        w_ctx.print_msg<MessageType::err_expected_symbol>( MessageInfo( symbol, 0, FmtStr::Color::Red ) );
        return false;
    }
    if ( return_type && std::dynamic_pointer_cast<SymbolExpr>( return_type ) == nullptr ) {
        w_ctx.print_msg<MessageType::err_expected_symbol>( MessageInfo( return_type, 0, FmtStr::Color::Red ) );
        return false;
    }
    if ( parameters && std::dynamic_pointer_cast<ParenthesisExpr>( parameters ) == nullptr ) {
        w_ctx.print_msg<MessageType::err_expected_parametes>( MessageInfo( parameters, 0, FmtStr::Color::Red ) );
        return false;
    }
    return true;
}

void FuncExpr::pre_symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    if ( auto symbol_symbol = std::dynamic_pointer_cast<SymbolExpr>( symbol ); symbol_symbol ) {
        SymbolId new_id = create_new_local_symbol_from_name_chain( c_ctx, get_symbol_chain_from_expr( symbol_symbol ) );
        switch_scope_to_symbol( c_ctx, new_id );
        symbol_symbol->update_symbol_id( new_id );
        c_ctx.symbol_graph[new_id].original_expr.push_back( symbol );

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
                w_ctx.print_msg<MessageType::err_symbol_is_ambiguous>(
                    MessageInfo( return_type, 0, FmtStr::Color::Red ), notes );
            }
            c_ctx.symbol_graph[new_id].identifier.eval_type = return_symbols.front();
        }
    }
}

void FuncExpr::symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    switch_scope_to_symbol(
        c_ctx, c_ctx.symbol_graph[std::dynamic_pointer_cast<SymbolExpr>( symbol )->get_symbol_id()].parent );
}

bool FuncCallExpr::primitive_semantic_check( CrateCtx &c_ctx, Worker &w_ctx ) {
    if ( std::dynamic_pointer_cast<SymbolExpr>( symbol ) == nullptr ) {
        w_ctx.print_msg<MessageType::err_expected_symbol>( MessageInfo( symbol, 0, FmtStr::Color::Red ) );
        return false;
    }
    if ( parameters && std::dynamic_pointer_cast<ParenthesisExpr>( parameters ) == nullptr ) {
        w_ctx.print_msg<MessageType::err_expected_parametes>( MessageInfo( parameters, 0, FmtStr::Color::Red ) );
        return false;
    }
    return true;
}

bool SimpleBindExpr::primitive_semantic_check( CrateCtx &c_ctx, Worker &w_ctx ) {
    // TODO make "=" operator not hard coded
    auto assignment = std::dynamic_pointer_cast<OperatorExpr>( expr );
    if ( assignment == nullptr || assignment->op != "=" ) {
        w_ctx.print_msg<MessageType::err_expected_assignment>( MessageInfo( expr, 0, FmtStr::Color::Red ) );
        return false;
    }
    if ( auto lvalue = std::dynamic_pointer_cast<TypedExpr>( assignment->lvalue ); lvalue != nullptr ) {
        if ( std::dynamic_pointer_cast<SymbolExpr>( lvalue->symbol ) == nullptr ) {
            w_ctx.print_msg<MessageType::err_expected_symbol>( MessageInfo( lvalue->symbol, 0, FmtStr::Color::Red ) );
            return false;
        }
        if ( std::dynamic_pointer_cast<SymbolExpr>( lvalue->type ) == nullptr ) {
            w_ctx.print_msg<MessageType::err_expected_symbol>( MessageInfo( lvalue->type, 0, FmtStr::Color::Red ) );
            return false;
        }
    } else if ( auto lvalue = std::dynamic_pointer_cast<SymbolExpr>( assignment->lvalue ); lvalue == nullptr ) {
        w_ctx.print_msg<MessageType::err_expected_symbol>( MessageInfo( lvalue, 0, FmtStr::Color::Red ) );
        return false;
    }
    return true;
}

bool AliasBindExpr::primitive_semantic_check( CrateCtx &c_ctx, Worker &w_ctx ) {
    // TODO make "=" operator not hard coded
    auto assignment = std::dynamic_pointer_cast<OperatorExpr>( expr );
    if ( assignment == nullptr || assignment->op != "=" ) {
        w_ctx.print_msg<MessageType::err_expected_assignment>( MessageInfo( expr, 0, FmtStr::Color::Red ) );
        return false;
    }
    if ( auto lvalue = std::dynamic_pointer_cast<SymbolExpr>( assignment->lvalue ); lvalue == nullptr ) {
        w_ctx.print_msg<MessageType::err_expected_symbol>( MessageInfo( lvalue, 0, FmtStr::Color::Red ) );
        return false;
    }
    if ( auto lvalue = std::dynamic_pointer_cast<SymbolExpr>( assignment->rvalue ); lvalue == nullptr ) {
        w_ctx.print_msg<MessageType::err_expected_symbol>( MessageInfo( lvalue, 0, FmtStr::Color::Red ) );
        return false;
    }
    return true;
}
void IfExpr::pre_symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    SymbolId new_id = create_new_local_symbol( c_ctx, "" );
    switch_scope_to_symbol( c_ctx, new_id );
    c_ctx.symbol_graph[new_id].original_expr.push_back( shared_from_this() );
}

void IfExpr::symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    pop_scope( c_ctx );
}

void IfElseExpr::pre_symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    SymbolId new_id = create_new_local_symbol( c_ctx, "" );
    switch_scope_to_symbol( c_ctx, new_id );
    c_ctx.symbol_graph[new_id].original_expr.push_back( shared_from_this() );
}

void IfElseExpr::symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    pop_scope( c_ctx );
}

void PreLoopExpr::pre_symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    SymbolId new_id = create_new_local_symbol( c_ctx, "" );
    switch_scope_to_symbol( c_ctx, new_id );
    c_ctx.symbol_graph[new_id].original_expr.push_back( shared_from_this() );
}

void PreLoopExpr::symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    pop_scope( c_ctx );
}

void PostLoopExpr::pre_symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    SymbolId new_id = create_new_local_symbol( c_ctx, "" );
    switch_scope_to_symbol( c_ctx, new_id );
    c_ctx.symbol_graph[new_id].original_expr.push_back( shared_from_this() );
}

void PostLoopExpr::symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    pop_scope( c_ctx );
}

void InfLoopExpr::pre_symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    SymbolId new_id = create_new_local_symbol( c_ctx, "" );
    switch_scope_to_symbol( c_ctx, new_id );
    c_ctx.symbol_graph[new_id].original_expr.push_back( shared_from_this() );
}

void InfLoopExpr::symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    pop_scope( c_ctx );
}

void ItrLoopExpr::pre_symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    SymbolId new_id = create_new_local_symbol( c_ctx, "" );
    switch_scope_to_symbol( c_ctx, new_id );
    c_ctx.symbol_graph[new_id].original_expr.push_back( shared_from_this() );
}

void ItrLoopExpr::symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    pop_scope( c_ctx );
}

bool MatchExpr::primitive_semantic_check( CrateCtx &c_ctx, Worker &w_ctx ) {
    if ( auto cases_list = std::dynamic_pointer_cast<ListedExpr>( cases ); cases_list != nullptr ) {
        for ( auto &entry : cases_list->get_list() ) {
            // TODO make "=>" operator not hard coded
            if ( auto assignment = std::dynamic_pointer_cast<OperatorExpr>( entry );
                 assignment == nullptr || assignment->op != "=>" ) {
                w_ctx.print_msg<MessageType::err_expected_implication>( MessageInfo( entry, 0, FmtStr::Color::Red ) );
                return false;
            }
        }
    } else {
        w_ctx.print_msg<MessageType::err_expected_comma_list>( MessageInfo( cases, 0, FmtStr::Color::Red ) );
        return false;
    }
    return true;
}

void MatchExpr::pre_symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    SymbolId new_id = create_new_local_symbol( c_ctx, "" );
    switch_scope_to_symbol( c_ctx, new_id );
    c_ctx.symbol_graph[new_id].original_expr.push_back( shared_from_this() );
}

void MatchExpr::symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    pop_scope( c_ctx );
}

bool ArrayAccessExpr::primitive_semantic_check( CrateCtx &c_ctx, Worker &w_ctx ) {
    // TODO maybe allow multiple parameters
    if ( !index || std::dynamic_pointer_cast<CommaExpr>( index ) != nullptr ) {
        w_ctx.print_msg<MessageType::err_expected_only_one_parameter>( MessageInfo( index, 0, FmtStr::Color::Red ) );
        return false;
    }
    return true;
}

bool StructExpr::primitive_semantic_check( CrateCtx &c_ctx, Worker &w_ctx ) {
    if ( name && std::dynamic_pointer_cast<SymbolExpr>( name ) == nullptr ) {
        w_ctx.print_msg<MessageType::err_expected_function_head>( MessageInfo( name, 0, FmtStr::Color::Red ) );
        return false;
    }
    if ( body ) {
        if ( auto cases_list = std::dynamic_pointer_cast<ListedExpr>( body ); cases_list != nullptr ) {
            for ( auto &entry : cases_list->get_list() ) {
                if ( auto pub_entry = std::dynamic_pointer_cast<PublicAttrExpr>( entry ); pub_entry != nullptr ) {
                    if ( std::dynamic_pointer_cast<TypedExpr>( pub_entry->symbol ) == nullptr &&
                         std::dynamic_pointer_cast<SymbolExpr>( entry ) == nullptr ) {
                        w_ctx.print_msg<MessageType::err_expected_symbol>(
                            MessageInfo( pub_entry->symbol, 0, FmtStr::Color::Red ) );
                        return false;
                    }
                } else if ( std::dynamic_pointer_cast<TypedExpr>( entry ) == nullptr &&
                            std::dynamic_pointer_cast<SymbolExpr>( entry ) == nullptr ) {
                    w_ctx.print_msg<MessageType::err_expected_symbol>( MessageInfo( entry, 0, FmtStr::Color::Red ) );
                    return false;
                }
            }
        } else {
            w_ctx.print_msg<MessageType::err_expected_comma_list>( MessageInfo( body, 0, FmtStr::Color::Red ) );
            return false;
        }
    }
    return true;
}

void StructExpr::pre_symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    if ( auto symbol_symbol = std::dynamic_pointer_cast<SymbolExpr>( name ); symbol_symbol ) {
        SymbolId new_id = create_new_local_symbol_from_name_chain( c_ctx, get_symbol_chain_from_expr( symbol_symbol ) );
        switch_scope_to_symbol( c_ctx, new_id );
        symbol_symbol->update_symbol_id( new_id );
        c_ctx.symbol_graph[new_id].original_expr.push_back( name );
    }
}

void StructExpr::symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    switch_scope_to_symbol( c_ctx,
                            c_ctx.symbol_graph[std::dynamic_pointer_cast<SymbolExpr>( name )->get_symbol_id()].parent );
}

bool TraitExpr::primitive_semantic_check( CrateCtx &c_ctx, Worker &w_ctx ) {
    if ( std::dynamic_pointer_cast<SymbolExpr>( name ) == nullptr ) {
        w_ctx.print_msg<MessageType::err_expected_function_head>( MessageInfo( name, 0, FmtStr::Color::Red ) );
        return false;
    }
    if ( auto cases_list = std::dynamic_pointer_cast<ListedExpr>( body ); cases_list != nullptr ) {
        for ( auto &entry : cases_list->get_list() ) {
            if ( auto pub_entry = std::dynamic_pointer_cast<PublicAttrExpr>( entry ); pub_entry != nullptr ) {
                if ( std::dynamic_pointer_cast<FuncHeadExpr>( pub_entry->symbol ) == nullptr ) {
                    w_ctx.print_msg<MessageType::err_expected_function_head>(
                        MessageInfo( pub_entry->symbol, 0, FmtStr::Color::Red ) );
                    return false;
                }
            } else if ( std::dynamic_pointer_cast<FuncHeadExpr>( entry ) == nullptr ) {
                w_ctx.print_msg<MessageType::err_expected_function_head>( MessageInfo( entry, 0, FmtStr::Color::Red ) );
                return false;
            }
        }
    } else {
        w_ctx.print_msg<MessageType::err_expected_comma_list>( MessageInfo( body, 0, FmtStr::Color::Red ) );
        return false;
    }
    return true;
}

void TraitExpr::pre_symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    if ( auto symbol_symbol = std::dynamic_pointer_cast<SymbolExpr>( name ); symbol_symbol ) {
        SymbolId new_id = create_new_local_symbol_from_name_chain( c_ctx, get_symbol_chain_from_expr( symbol_symbol ) );
        switch_scope_to_symbol( c_ctx, new_id );
        symbol_symbol->update_symbol_id( new_id );
        c_ctx.symbol_graph[new_id].original_expr.push_back( name );
    }
}

void TraitExpr::symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    switch_scope_to_symbol( c_ctx,
                            c_ctx.symbol_graph[std::dynamic_pointer_cast<SymbolExpr>( name )->get_symbol_id()].parent );
}

bool ImplExpr::primitive_semantic_check( CrateCtx &c_ctx, Worker &w_ctx ) {
    if ( std::dynamic_pointer_cast<SymbolExpr>( struct_name ) == nullptr ) {
        w_ctx.print_msg<MessageType::err_expected_function_head>( MessageInfo( struct_name, 0, FmtStr::Color::Red ) );
        return false;
    }
    if ( trait_name && std::dynamic_pointer_cast<SymbolExpr>( trait_name ) == nullptr ) {
        w_ctx.print_msg<MessageType::err_expected_function_head>( MessageInfo( trait_name, 0, FmtStr::Color::Red ) );
        return false;
    }
    if ( auto cases_list = std::dynamic_pointer_cast<ListedExpr>( body ); cases_list != nullptr ) {
        for ( auto &entry : cases_list->get_list() ) {
            if ( auto pub_entry = std::dynamic_pointer_cast<PublicAttrExpr>( entry ); pub_entry != nullptr ) {
                if ( std::dynamic_pointer_cast<FuncExpr>( pub_entry->symbol ) == nullptr ) {
                    w_ctx.print_msg<MessageType::err_expected_function_definition>(
                        MessageInfo( pub_entry->symbol, 0, FmtStr::Color::Red ) );
                    return false;
                }
            } else if ( std::dynamic_pointer_cast<FuncExpr>( entry ) == nullptr ) {
                w_ctx.print_msg<MessageType::err_expected_function_definition>(
                    MessageInfo( entry, 0, FmtStr::Color::Red ) );
                return false;
            }
        }
    } else {
        w_ctx.print_msg<MessageType::err_expected_comma_list>( MessageInfo( body, 0, FmtStr::Color::Red ) );
        return false;
    }
    return true;
}

void ImplExpr::pre_symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    if ( auto symbol_symbol = std::dynamic_pointer_cast<SymbolExpr>( struct_name ); symbol_symbol ) {
        SymbolId new_id = create_new_local_symbol_from_name_chain( c_ctx, get_symbol_chain_from_expr( symbol_symbol ) );
        switch_scope_to_symbol( c_ctx, new_id );
        symbol_symbol->update_symbol_id( new_id );
        c_ctx.symbol_graph[new_id].original_expr.push_back( struct_name );
    }
}

void ImplExpr::symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    switch_scope_to_symbol(
        c_ctx, c_ctx.symbol_graph[std::dynamic_pointer_cast<SymbolExpr>( struct_name )->get_symbol_id()].parent );
}

void ModuleExpr::pre_symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    if ( auto symbol_symbol = std::dynamic_pointer_cast<SymbolExpr>( symbol ); symbol_symbol ) {
        SymbolId new_id = create_new_local_symbol_from_name_chain( c_ctx, get_symbol_chain_from_expr( symbol_symbol ) );
        switch_scope_to_symbol( c_ctx, new_id );
        symbol_symbol->update_symbol_id( new_id );
        c_ctx.symbol_graph[new_id].original_expr.push_back( symbol );
    }
}

void ModuleExpr::symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    switch_scope_to_symbol(
        c_ctx, c_ctx.symbol_graph[std::dynamic_pointer_cast<SymbolExpr>( symbol )->get_symbol_id()].parent );
}

bool DeclarationExpr::primitive_semantic_check( CrateCtx &c_ctx, Worker &w_ctx ) {
    if ( std::dynamic_pointer_cast<FuncHeadExpr>( symbol ) == nullptr ) {
        w_ctx.print_msg<MessageType::err_expected_function_head>( MessageInfo( symbol, 0, FmtStr::Color::Red ) );
        return false;
    }
    return true;
}

bool PublicAttrExpr::primitive_semantic_check( CrateCtx &c_ctx, Worker &w_ctx ) {
    if ( std::dynamic_pointer_cast<TypedExpr>( symbol ) == nullptr &&
         std::dynamic_pointer_cast<SymbolExpr>( symbol ) == nullptr &&
         std::dynamic_pointer_cast<FuncHeadExpr>( symbol ) == nullptr ) {
        w_ctx.print_msg<MessageType::err_expected_symbol>( MessageInfo( symbol, 0, FmtStr::Color::Red ) );
        return false;
    }
    return true;
}

void StaticStatementExpr::pre_symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    SymbolId new_id = create_new_local_symbol( c_ctx, "" );
    switch_scope_to_symbol( c_ctx, new_id );
    c_ctx.symbol_graph[new_id].original_expr.push_back( shared_from_this() );
}

void StaticStatementExpr::symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    pop_scope( c_ctx );
}

bool CompilerAnnotationExpr::primitive_semantic_check( CrateCtx &c_ctx, Worker &w_ctx ) {
    if ( std::dynamic_pointer_cast<SymbolExpr>( name ) == nullptr ) {
        w_ctx.print_msg<MessageType::err_expected_symbol>( MessageInfo( name, 0, FmtStr::Color::Red ) );
        return false;
    }
    return true;
}

bool MacroExpr::primitive_semantic_check( CrateCtx &c_ctx, Worker &w_ctx ) {
    if ( std::dynamic_pointer_cast<SymbolExpr>( name ) == nullptr ) {
        w_ctx.print_msg<MessageType::err_expected_symbol>( MessageInfo( name, 0, FmtStr::Color::Red ) );
        return false;
    }
    return true;
}

bool TemplateExpr::primitive_semantic_check( CrateCtx &c_ctx, Worker &w_ctx ) {
    if ( std::dynamic_pointer_cast<SymbolExpr>( symbol ) == nullptr ) {
        w_ctx.print_msg<MessageType::err_expected_symbol>( MessageInfo( symbol, 0, FmtStr::Color::Red ) );
        return false;
    }

    if ( attributes ) {
        if ( auto attributes_list = std::dynamic_pointer_cast<CommaExpr>( attributes ); attributes_list != nullptr ) {
            for ( auto &entry : attributes_list->get_list() ) {
                if ( std::dynamic_pointer_cast<SymbolExpr>( entry ) == nullptr &&
                     std::dynamic_pointer_cast<TypedExpr>( entry ) == nullptr ) {
                    w_ctx.print_msg<MessageType::err_expected_symbol>( MessageInfo( entry, 0, FmtStr::Color::Red ) );
                    return false;
                }
            }
        } else if ( std::dynamic_pointer_cast<SymbolExpr>( attributes ) == nullptr &&
                    std::dynamic_pointer_cast<TypedExpr>( attributes ) == nullptr ) {
            w_ctx.print_msg<MessageType::err_expected_comma_list>( MessageInfo( attributes, 0, FmtStr::Color::Red ) );
            return false;
        }
    }
    return true;
}

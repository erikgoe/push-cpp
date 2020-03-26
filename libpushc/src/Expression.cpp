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

bool TokenExpr::basic_semantic_check( CrateCtx &c_ctx, Worker &w_ctx ) {
    w_ctx.print_msg<MessageType::err_orphan_token>( MessageInfo( t, 0, FmtStr::Color::Red ) );
    return false;
}

bool DeclExpr::basic_semantic_check( CrateCtx &c_ctx, Worker &w_ctx ) {
    // Check for missing semicolons
    for ( auto expr = sub_expr.begin(); expr != sub_expr.end(); expr++ ) {
        if ( std::dynamic_pointer_cast<CompletedExpr>( *expr ) == nullptr ) {
            w_ctx.print_msg<MessageType::err_unfinished_expr>( MessageInfo( *expr, 0, FmtStr::Color::Red ) );
            return false;
        }
    }
    return true;
}

bool DeclExpr::first_transformation( CrateCtx &c_ctx, Worker &w_ctx ) {
    for ( auto itr = sub_expr.begin(); itr != sub_expr.end(); itr++ ) {
        if ( auto block = std::dynamic_pointer_cast<BlockExpr>( *itr ); block != nullptr ) {
            // Replace BlockExpr with DeclExpr
            auto new_decl = make_shared<DeclExpr>();
            new_decl->sub_expr = block->sub_expr;
            *itr = new_decl;
        } else if ( auto pub = std::dynamic_pointer_cast<PublicAttrExpr>( *itr ); pub != nullptr ) {
            // Resolve pub keyword
            pub->set_inner_public();
            *itr = pub->symbol;
        }
    }
    return true;
}

bool SingleCompletedExpr::basic_semantic_check( CrateCtx &c_ctx, Worker &w_ctx ) {
    if ( std::dynamic_pointer_cast<CompletedExpr>( sub_expr ) != nullptr ) { // double semicolon
        w_ctx.print_msg<MessageType::err_semicolon_without_meaning>(
            MessageInfo( shared_from_this(), 0, FmtStr::Color::Red ) );
        return false;
    }
    return true;
}

bool SingleCompletedExpr::first_transformation( CrateCtx &c_ctx, Worker &w_ctx ) {
    if ( auto pub = std::dynamic_pointer_cast<PublicAttrExpr>( sub_expr ); pub != nullptr ) {
        // Resolve pub keyword
        pub->set_inner_public();
        sub_expr = pub->symbol;
    }
    return true;
}

bool BlockExpr::basic_semantic_check( CrateCtx &c_ctx, Worker &w_ctx ) {
    // Check for missing semicolons (except the last expr)
    for ( auto expr = sub_expr.begin(); expr != sub_expr.end() && expr != sub_expr.end() - 1; expr++ ) {
        if ( std::dynamic_pointer_cast<CompletedExpr>( *expr ) == nullptr ) {
            w_ctx.print_msg<MessageType::err_unfinished_expr>( MessageInfo( *expr, 0, FmtStr::Color::Red ) );
            return false;
        }
    }
    return true;
}

bool BlockExpr::symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    SymbolId new_id = create_new_local_symbol( c_ctx, "" );
    switch_scope_to_symbol( c_ctx, new_id );
    c_ctx.symbol_graph[new_id].original_expr.push_back( shared_from_this() );
    return true;
}

bool BlockExpr::post_symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    pop_scope( c_ctx );
    return true;
}

bool ArraySpecifierExpr::basic_semantic_check( CrateCtx &c_ctx, Worker &w_ctx ) {
    // Check for missing semicolons (except the last expr)
    for ( auto expr = sub_expr.begin(); expr != sub_expr.end() && expr != sub_expr.end() - 1; expr++ ) {
        if ( std::dynamic_pointer_cast<CompletedExpr>( *expr ) == nullptr ) {
            w_ctx.print_msg<MessageType::err_unfinished_expr>( MessageInfo( *expr, 0, FmtStr::Color::Red ) );
            return false;
        }
    }
    return true;
}

bool FuncHeadExpr::basic_semantic_check( CrateCtx &c_ctx, Worker &w_ctx ) {
    if ( std::dynamic_pointer_cast<SymbolExpr>( symbol ) == nullptr ) {
        w_ctx.print_msg<MessageType::err_expected_symbol>( MessageInfo( symbol, 0, FmtStr::Color::Red ) );
        return false;
    }
    if ( parameters ) {
        if ( auto paren_expr = std::dynamic_pointer_cast<ParenthesisExpr>( parameters ); paren_expr != nullptr ) {
            for ( auto &entry : paren_expr->get_list() ) {
                if ( auto typed = std::dynamic_pointer_cast<TypedExpr>( entry ); typed != nullptr ) {
                    if ( std::dynamic_pointer_cast<SymbolExpr>( typed->symbol ) == nullptr ) {
                        w_ctx.print_msg<MessageType::err_expected_symbol>(
                            MessageInfo( typed->symbol, 0, FmtStr::Color::Red ) );
                        return false;
                    }
                    if ( std::dynamic_pointer_cast<SymbolExpr>( typed->type ) == nullptr ) {
                        w_ctx.print_msg<MessageType::err_expected_symbol>(
                            MessageInfo( typed->type, 0, FmtStr::Color::Red ) );
                        return false;
                    }
                } else if ( std::dynamic_pointer_cast<SymbolExpr>( entry ) == nullptr ) {
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

bool FuncExpr::basic_semantic_check( CrateCtx &c_ctx, Worker &w_ctx ) {
    if ( symbol && std::dynamic_pointer_cast<SymbolExpr>( symbol ) == nullptr ) {
        w_ctx.print_msg<MessageType::err_expected_symbol>( MessageInfo( symbol, 0, FmtStr::Color::Red ) );
        return false;
    }
    if ( return_type && std::dynamic_pointer_cast<SymbolExpr>( return_type ) == nullptr ) {
        w_ctx.print_msg<MessageType::err_expected_symbol>( MessageInfo( return_type, 0, FmtStr::Color::Red ) );
        return false;
    }
    if ( parameters ) {
        if ( auto paren_expr = std::dynamic_pointer_cast<ParenthesisExpr>( parameters ); paren_expr != nullptr ) {
            for ( auto &entry : paren_expr->get_list() ) {
                if ( auto typed = std::dynamic_pointer_cast<TypedExpr>( entry ); typed != nullptr ) {
                    if ( std::dynamic_pointer_cast<SymbolExpr>( typed->symbol ) == nullptr ) {
                        w_ctx.print_msg<MessageType::err_expected_symbol>(
                            MessageInfo( typed->symbol, 0, FmtStr::Color::Red ) );
                        return false;
                    }
                    if ( std::dynamic_pointer_cast<SymbolExpr>( typed->type ) == nullptr ) {
                        w_ctx.print_msg<MessageType::err_expected_symbol>(
                            MessageInfo( typed->type, 0, FmtStr::Color::Red ) );
                        return false;
                    }
                } else if ( std::dynamic_pointer_cast<SymbolExpr>( entry ) == nullptr ) {
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

bool FuncExpr::symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
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
                return false;
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
    return true;
}

bool FuncExpr::post_symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    switch_scope_to_symbol(
        c_ctx, c_ctx.symbol_graph[std::dynamic_pointer_cast<SymbolExpr>( symbol )->get_symbol_id()].parent );
    return true;
}

bool FuncCallExpr::basic_semantic_check( CrateCtx &c_ctx, Worker &w_ctx ) {
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

bool SimpleBindExpr::basic_semantic_check( CrateCtx &c_ctx, Worker &w_ctx ) {
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
        w_ctx.print_msg<MessageType::err_expected_symbol>( MessageInfo( assignment->lvalue, 0, FmtStr::Color::Red ) );
        return false;
    }
    return true;
}

bool AliasBindExpr::basic_semantic_check( CrateCtx &c_ctx, Worker &w_ctx ) {
    // TODO make "=" operator not hard coded
    if ( auto assignment = std::dynamic_pointer_cast<OperatorExpr>( expr ); assignment != nullptr ) {
        if ( assignment->op != "=" ) {
            w_ctx.print_msg<MessageType::err_expected_assignment>( MessageInfo( expr, 0, FmtStr::Color::Red ) );
            return false;
        }
        if ( auto lvalue = std::dynamic_pointer_cast<SymbolExpr>( assignment->lvalue ); lvalue == nullptr ) {
            w_ctx.print_msg<MessageType::err_expected_symbol>(
                MessageInfo( assignment->lvalue, 0, FmtStr::Color::Red ) );
            return false;
        }
        if ( auto lvalue = std::dynamic_pointer_cast<SymbolExpr>( assignment->rvalue ); lvalue == nullptr ) {
            w_ctx.print_msg<MessageType::err_expected_symbol>(
                MessageInfo( assignment->lvalue, 0, FmtStr::Color::Red ) );
            return false;
        }
    } else if ( std::dynamic_pointer_cast<SymbolExpr>( expr ) == nullptr ) {
        w_ctx.print_msg<MessageType::err_expected_symbol>( MessageInfo( expr, 0, FmtStr::Color::Red ) );
        return false;
    }
    return true;
}

bool IfExpr::symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    SymbolId new_id = create_new_local_symbol( c_ctx, "" );
    switch_scope_to_symbol( c_ctx, new_id );
    c_ctx.symbol_graph[new_id].original_expr.push_back( shared_from_this() );
    return true;
}

bool IfExpr::post_symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    pop_scope( c_ctx );
    return true;
}

bool IfElseExpr::symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    SymbolId new_id = create_new_local_symbol( c_ctx, "" );
    switch_scope_to_symbol( c_ctx, new_id );
    c_ctx.symbol_graph[new_id].original_expr.push_back( shared_from_this() );
    return true;
}

bool IfElseExpr::post_symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    pop_scope( c_ctx );
    return true;
}

bool PreLoopExpr::symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    SymbolId new_id = create_new_local_symbol( c_ctx, "" );
    switch_scope_to_symbol( c_ctx, new_id );
    c_ctx.symbol_graph[new_id].original_expr.push_back( shared_from_this() );
    return true;
}

bool PreLoopExpr::post_symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    pop_scope( c_ctx );
    return true;
}

bool PostLoopExpr::symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    SymbolId new_id = create_new_local_symbol( c_ctx, "" );
    switch_scope_to_symbol( c_ctx, new_id );
    c_ctx.symbol_graph[new_id].original_expr.push_back( shared_from_this() );
    return true;
}

bool PostLoopExpr::post_symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    pop_scope( c_ctx );
    return true;
}

bool InfLoopExpr::symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    SymbolId new_id = create_new_local_symbol( c_ctx, "" );
    switch_scope_to_symbol( c_ctx, new_id );
    c_ctx.symbol_graph[new_id].original_expr.push_back( shared_from_this() );
    return true;
}

bool InfLoopExpr::post_symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    pop_scope( c_ctx );
    return true;
}

bool ItrLoopExpr::symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    SymbolId new_id = create_new_local_symbol( c_ctx, "" );
    switch_scope_to_symbol( c_ctx, new_id );
    c_ctx.symbol_graph[new_id].original_expr.push_back( shared_from_this() );
    return true;
}

bool ItrLoopExpr::post_symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    pop_scope( c_ctx );
    return true;
}

bool MatchExpr::basic_semantic_check( CrateCtx &c_ctx, Worker &w_ctx ) {
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

bool MatchExpr::symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    SymbolId new_id = create_new_local_symbol( c_ctx, "" );
    switch_scope_to_symbol( c_ctx, new_id );
    c_ctx.symbol_graph[new_id].original_expr.push_back( shared_from_this() );
    return true;
}

bool MatchExpr::post_symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    pop_scope( c_ctx );
    return true;
}

bool ArrayAccessExpr::basic_semantic_check( CrateCtx &c_ctx, Worker &w_ctx ) {
    // TODO maybe allow multiple parameters
    if ( !index || std::dynamic_pointer_cast<CommaExpr>( index ) != nullptr ) {
        w_ctx.print_msg<MessageType::err_expected_only_one_parameter>( MessageInfo( index, 0, FmtStr::Color::Red ) );
        return false;
    }
    return true;
}

bool StructExpr::basic_semantic_check( CrateCtx &c_ctx, Worker &w_ctx ) {
    if ( name && std::dynamic_pointer_cast<SymbolExpr>( name ) == nullptr ) {
        w_ctx.print_msg<MessageType::err_expected_symbol>( MessageInfo( name, 0, FmtStr::Color::Red ) );
        return false;
    }
    if ( body ) {
        if ( auto list = std::dynamic_pointer_cast<ListedExpr>( body ); list != nullptr ) {
            for ( auto &entry : list->get_list() ) {
                if ( auto pub_entry = std::dynamic_pointer_cast<PublicAttrExpr>( entry ); pub_entry != nullptr ) {
                    // PublicAttrExpr checks already whether its symbol is TypedExpr, SymbolExpr or FuncHeadExpr, but
                    // still this is needed. If it's a TypedExpr, symbol and type entries are already checked
                    if ( std::dynamic_pointer_cast<FuncExpr>( pub_entry->symbol ) != nullptr ||
                         std::dynamic_pointer_cast<FuncHeadExpr>( pub_entry->symbol ) != nullptr ) {
                        w_ctx.print_msg<MessageType::err_method_not_allowed>(
                            MessageInfo( pub_entry->symbol, 0, FmtStr::Color::Red ) );
                        return false;
                    }
                } else {
                    if ( std::dynamic_pointer_cast<FuncExpr>( entry ) != nullptr ||
                         std::dynamic_pointer_cast<FuncHeadExpr>( entry ) != nullptr ) {
                        w_ctx.print_msg<MessageType::err_method_not_allowed>(
                            MessageInfo( entry, 0, FmtStr::Color::Red ) );
                        return false;
                    } else if ( auto typed = std::dynamic_pointer_cast<TypedExpr>( entry ); typed != nullptr ) {
                        if ( std::dynamic_pointer_cast<SymbolExpr>( typed->symbol ) == nullptr ) {
                            w_ctx.print_msg<MessageType::err_expected_symbol>(
                                MessageInfo( typed->symbol, 0, FmtStr::Color::Red ) );
                            return false;
                        }
                        if ( std::dynamic_pointer_cast<SymbolExpr>( typed->type ) == nullptr ) {
                            w_ctx.print_msg<MessageType::err_expected_symbol>(
                                MessageInfo( typed->type, 0, FmtStr::Color::Red ) );
                            return false;
                        }
                    } else if ( std::dynamic_pointer_cast<SymbolExpr>( entry ) == nullptr ) {
                        w_ctx.print_msg<MessageType::err_expected_symbol>(
                            MessageInfo( entry, 0, FmtStr::Color::Red ) );
                        return false;
                    }
                }
            }
        } else {
            w_ctx.print_msg<MessageType::err_expected_comma_list>( MessageInfo( body, 0, FmtStr::Color::Red ) );
            return false;
        }
    }
    return true;
}

bool StructExpr::first_transformation( CrateCtx &c_ctx, Worker &w_ctx ) {
    if ( auto single = std::dynamic_pointer_cast<SingleCompletedExpr>( body ); single != nullptr ) {
        if ( auto pub = std::dynamic_pointer_cast<PublicAttrExpr>( single->sub_expr ); pub != nullptr ) {
            // Resolve pub keyword
            pub->set_inner_public();
            single->sub_expr = pub->symbol;
        }
    } else {
        std::vector<sptr<Expr>> *list;

        if ( auto block = std::dynamic_pointer_cast<SetExpr>( body ); block != nullptr )
            list = &block->sub_expr;
        else if ( auto block = std::dynamic_pointer_cast<BlockExpr>( body ); block != nullptr )
            list = &block->sub_expr;

        for ( auto itr = list->begin(); itr != list->end(); itr++ ) {
            if ( auto pub = std::dynamic_pointer_cast<PublicAttrExpr>( *itr ); pub != nullptr ) {
                // Resolve pub keyword
                pub->set_inner_public();
                *itr = pub->symbol;
            }
        }
    }
    return true;
}

bool StructExpr::symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    if ( auto symbol_symbol = std::dynamic_pointer_cast<SymbolExpr>( name ); symbol_symbol ) {
        SymbolId new_id = create_new_local_symbol_from_name_chain( c_ctx, get_symbol_chain_from_expr( symbol_symbol ) );
        switch_scope_to_symbol( c_ctx, new_id );
        symbol_symbol->update_symbol_id( new_id );
        c_ctx.symbol_graph[new_id].original_expr.push_back( name );
    }
    return true;
}

bool StructExpr::post_symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    switch_scope_to_symbol( c_ctx,
                            c_ctx.symbol_graph[std::dynamic_pointer_cast<SymbolExpr>( name )->get_symbol_id()].parent );
    return true;
}

bool TraitExpr::basic_semantic_check( CrateCtx &c_ctx, Worker &w_ctx ) {
    if ( std::dynamic_pointer_cast<SymbolExpr>( name ) == nullptr ) {
        w_ctx.print_msg<MessageType::err_expected_symbol>( MessageInfo( name, 0, FmtStr::Color::Red ) );
        return false;
    }
    if ( auto list = std::dynamic_pointer_cast<ListedExpr>( body ); list != nullptr ) {
        for ( auto &entry : list->get_list() ) {
            if ( auto pub_entry = std::dynamic_pointer_cast<PublicAttrExpr>( entry ); pub_entry != nullptr ) {
                if ( std::dynamic_pointer_cast<FuncHeadExpr>( pub_entry->symbol ) == nullptr &&
                     std::dynamic_pointer_cast<FuncExpr>( pub_entry->symbol ) == nullptr ) {
                    w_ctx.print_msg<MessageType::err_expected_function_head>(
                        MessageInfo( pub_entry->symbol, 0, FmtStr::Color::Red ) );
                    return false;
                }
            } else if ( std::dynamic_pointer_cast<FuncHeadExpr>( entry ) == nullptr &&
                        std::dynamic_pointer_cast<FuncExpr>( entry ) == nullptr ) {
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

bool TraitExpr::first_transformation( CrateCtx &c_ctx, Worker &w_ctx ) {
    if ( auto single = std::dynamic_pointer_cast<SingleCompletedExpr>( body ); single != nullptr ) {
        if ( auto pub = std::dynamic_pointer_cast<PublicAttrExpr>( single->sub_expr ); pub != nullptr ) {
            // Resolve pub keyword
            pub->set_inner_public();
            single->sub_expr = pub->symbol;
        }
    } else {
        std::vector<sptr<Expr>> *list;

        if ( auto block = std::dynamic_pointer_cast<SetExpr>( body ); block != nullptr )
            list = &block->sub_expr;
        else if ( auto block = std::dynamic_pointer_cast<BlockExpr>( body ); block != nullptr )
            list = &block->sub_expr;

        for ( auto itr = list->begin(); itr != list->end(); itr++ ) {
            if ( auto pub = std::dynamic_pointer_cast<PublicAttrExpr>( *itr ); pub != nullptr ) {
                // Resolve pub keyword
                pub->set_inner_public();
                *itr = pub->symbol;
            }
        }
    }
    return true;
}

bool TraitExpr::symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    if ( auto symbol_symbol = std::dynamic_pointer_cast<SymbolExpr>( name ); symbol_symbol ) {
        SymbolId new_id = create_new_local_symbol_from_name_chain( c_ctx, get_symbol_chain_from_expr( symbol_symbol ) );
        switch_scope_to_symbol( c_ctx, new_id );
        symbol_symbol->update_symbol_id( new_id );
        c_ctx.symbol_graph[new_id].original_expr.push_back( name );
    }
    return true;
}

bool TraitExpr::post_symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    switch_scope_to_symbol( c_ctx,
                            c_ctx.symbol_graph[std::dynamic_pointer_cast<SymbolExpr>( name )->get_symbol_id()].parent );
    return true;
}

bool ImplExpr::basic_semantic_check( CrateCtx &c_ctx, Worker &w_ctx ) {
    if ( std::dynamic_pointer_cast<SymbolExpr>( struct_name ) == nullptr ) {
        w_ctx.print_msg<MessageType::err_expected_symbol>( MessageInfo( struct_name, 0, FmtStr::Color::Red ) );
        return false;
    }
    if ( trait_name && std::dynamic_pointer_cast<SymbolExpr>( trait_name ) == nullptr ) {
        w_ctx.print_msg<MessageType::err_expected_symbol>( MessageInfo( trait_name, 0, FmtStr::Color::Red ) );
        return false;
    }
    if ( auto list = std::dynamic_pointer_cast<ListedExpr>( body ); list != nullptr ) {
        for ( auto &entry : list->get_list() ) {
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

bool ImplExpr::first_transformation( CrateCtx &c_ctx, Worker &w_ctx ) {
    if ( auto single = std::dynamic_pointer_cast<SingleCompletedExpr>( body ); single != nullptr ) {
        if ( auto pub = std::dynamic_pointer_cast<PublicAttrExpr>( single->sub_expr ); pub != nullptr ) {
            // Resolve pub keyword
            pub->set_inner_public();
            single->sub_expr = pub->symbol;
        }
    } else {
        std::vector<sptr<Expr>> *list;

        if ( auto block = std::dynamic_pointer_cast<SetExpr>( body ); block != nullptr )
            list = &block->sub_expr;
        else if ( auto block = std::dynamic_pointer_cast<BlockExpr>( body ); block != nullptr )
            list = &block->sub_expr;

        for ( auto itr = list->begin(); itr != list->end(); itr++ ) {
            if ( auto pub = std::dynamic_pointer_cast<PublicAttrExpr>( *itr ); pub != nullptr ) {
                // Resolve pub keyword
                pub->set_inner_public();
                *itr = pub->symbol;
            }
        }
    }
    return true;
}

bool ImplExpr::symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    if ( auto symbol_symbol = std::dynamic_pointer_cast<SymbolExpr>( struct_name ); symbol_symbol ) {
        SymbolId new_id = create_new_local_symbol_from_name_chain( c_ctx, get_symbol_chain_from_expr( symbol_symbol ) );
        switch_scope_to_symbol( c_ctx, new_id );
        symbol_symbol->update_symbol_id( new_id );
        c_ctx.symbol_graph[new_id].original_expr.push_back( struct_name );
    }
    return true;
}

bool ImplExpr::post_symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    switch_scope_to_symbol(
        c_ctx, c_ctx.symbol_graph[std::dynamic_pointer_cast<SymbolExpr>( struct_name )->get_symbol_id()].parent );
    return true;
}

bool ModuleExpr::symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    if ( auto symbol_symbol = std::dynamic_pointer_cast<SymbolExpr>( symbol ); symbol_symbol ) {
        SymbolId new_id = create_new_local_symbol_from_name_chain( c_ctx, get_symbol_chain_from_expr( symbol_symbol ) );
        switch_scope_to_symbol( c_ctx, new_id );
        symbol_symbol->update_symbol_id( new_id );
        c_ctx.symbol_graph[new_id].original_expr.push_back( symbol );
    }
    return true;
}

bool ModuleExpr::post_symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    switch_scope_to_symbol(
        c_ctx, c_ctx.symbol_graph[std::dynamic_pointer_cast<SymbolExpr>( symbol )->get_symbol_id()].parent );
    return true;
}

bool DeclarationExpr::basic_semantic_check( CrateCtx &c_ctx, Worker &w_ctx ) {
    if ( std::dynamic_pointer_cast<FuncHeadExpr>( symbol ) == nullptr ) {
        w_ctx.print_msg<MessageType::err_expected_function_head>( MessageInfo( symbol, 0, FmtStr::Color::Red ) );
        return false;
    }
    return true;
}

bool PublicAttrExpr::basic_semantic_check( CrateCtx &c_ctx, Worker &w_ctx ) {
    if ( auto typed = std::dynamic_pointer_cast<TypedExpr>( symbol ); typed != nullptr ) {
        if ( std::dynamic_pointer_cast<SymbolExpr>( typed->symbol ) == nullptr ) {
            w_ctx.print_msg<MessageType::err_expected_symbol>( MessageInfo( typed->symbol, 0, FmtStr::Color::Red ) );
            return false;
        }
        if ( std::dynamic_pointer_cast<SymbolExpr>( typed->type ) == nullptr ) {
            w_ctx.print_msg<MessageType::err_expected_symbol>( MessageInfo( typed->type, 0, FmtStr::Color::Red ) );
            return false;
        }
    } else if ( std::dynamic_pointer_cast<SymbolExpr>( symbol ) == nullptr &&
                std::dynamic_pointer_cast<FuncHeadExpr>( symbol ) == nullptr &&
                std::dynamic_pointer_cast<FuncExpr>( symbol ) == nullptr ) {
        w_ctx.print_msg<MessageType::err_expected_symbol>( MessageInfo( symbol, 0, FmtStr::Color::Red ) );
        return false;
    }
    return true;
}

bool PublicAttrExpr::is_inner_public() {
    if ( auto typed = std::dynamic_pointer_cast<TypedExpr>( symbol ); typed != nullptr ) {
        return std::dynamic_pointer_cast<SymbolExpr>( typed->symbol )->is_public();
    } else if ( auto symbol_expr = std::dynamic_pointer_cast<SymbolExpr>( symbol ); symbol_expr != nullptr ) {
        return symbol_expr->is_public();
    } else if ( auto fn = std::dynamic_pointer_cast<FuncHeadExpr>( symbol ); fn != nullptr ) {
        return fn->pub;
    } else if ( auto fn = std::dynamic_pointer_cast<FuncExpr>( symbol ); fn != nullptr ) {
        return fn->pub;
    }
    return false; // never happens
}

void PublicAttrExpr::set_inner_public( bool value ) {
    if ( auto typed = std::dynamic_pointer_cast<TypedExpr>( symbol ); typed != nullptr ) {
        std::dynamic_pointer_cast<SymbolExpr>( typed->symbol )->set_public( value );
    } else if ( auto symbol_expr = std::dynamic_pointer_cast<SymbolExpr>( symbol ); symbol_expr != nullptr ) {
        symbol_expr->set_public( value );
    } else if ( auto fn = std::dynamic_pointer_cast<FuncHeadExpr>( symbol ); fn != nullptr ) {
        fn->pub = value;
    } else if ( auto fn = std::dynamic_pointer_cast<FuncExpr>( symbol ); fn != nullptr ) {
        fn->pub = value;
    }
}

bool StaticStatementExpr::symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    SymbolId new_id = create_new_local_symbol( c_ctx, "" );
    switch_scope_to_symbol( c_ctx, new_id );
    c_ctx.symbol_graph[new_id].original_expr.push_back( shared_from_this() );
    return true;
}

bool StaticStatementExpr::post_symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    pop_scope( c_ctx );
    return true;
}

bool CompilerAnnotationExpr::basic_semantic_check( CrateCtx &c_ctx, Worker &w_ctx ) {
    if ( std::dynamic_pointer_cast<SymbolExpr>( symbol ) == nullptr ) {
        w_ctx.print_msg<MessageType::err_expected_symbol>( MessageInfo( symbol, 0, FmtStr::Color::Red ) );
        return false;
    }
    if ( std::dynamic_pointer_cast<ParenthesisExpr>( parameters ) == nullptr ) {
        w_ctx.print_msg<MessageType::err_expected_parametes>( MessageInfo( parameters, 0, FmtStr::Color::Red ) );
        return false;
    }
    return true;
}

bool MacroExpr::basic_semantic_check( CrateCtx &c_ctx, Worker &w_ctx ) {
    if ( std::dynamic_pointer_cast<SymbolExpr>( name ) == nullptr ) {
        w_ctx.print_msg<MessageType::err_expected_symbol>( MessageInfo( name, 0, FmtStr::Color::Red ) );
        return false;
    }
    return true;
}

bool TemplateExpr::basic_semantic_check( CrateCtx &c_ctx, Worker &w_ctx ) {
    if ( std::dynamic_pointer_cast<SymbolExpr>( symbol ) == nullptr ) {
        w_ctx.print_msg<MessageType::err_expected_symbol>( MessageInfo( symbol, 0, FmtStr::Color::Red ) );
        return false;
    }
    return true;
}

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
#include "libpushc/MirTranslation.h"

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
    LOG_ERR( "Could not parse symbol chain from expr" );
    return nullptr;
}

String Expr::get_additional_debug_data() {
    String result;
    if ( !annotations.empty() ) {
        result += "#(";
        for ( auto &a : annotations ) {
            result += a->get_debug_repr() + ", ";
        }
        result += ")";
    }
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

bool DeclExpr::first_transformation( CrateCtx &c_ctx, Worker &w_ctx, sptr<Expr> &anchor, sptr<Expr> parent ) {
    std::vector<sptr<Expr>> annotation_list;
    for ( size_t i = 0; i < sub_expr.size(); i++ ) {
        // Resolve annotations
        if ( std::dynamic_pointer_cast<CompilerAnnotationExpr>( sub_expr[i] ) != nullptr ) {
            annotation_list.push_back( sub_expr[i] );
            sub_expr.erase( sub_expr.begin() + i );
            i--;
            continue;
        } else if ( !annotation_list.empty() ) {
            sub_expr[i]->annotations = std::move( annotation_list );
            annotation_list.clear();
        }

        if ( auto sc_expr = std::dynamic_pointer_cast<SingleCompletedExpr>( sub_expr[i] ); sc_expr != nullptr ) {
            if ( auto comma_expr = std::dynamic_pointer_cast<CommaExpr>( sc_expr->sub_expr ); comma_expr != nullptr ) {
                // Resolve commas
                sub_expr.erase( sub_expr.begin() + i );
                sub_expr.insert( sub_expr.begin() + i, comma_expr->sub_expr.begin(), comma_expr->sub_expr.end() );
                i--;
                continue;
            } else if ( auto alias = std::dynamic_pointer_cast<AliasBindExpr>( sc_expr->sub_expr ); alias != nullptr ) {
                // Resolve alias statements
                auto subs = alias->get_substitutions();
                substitutions.insert( substitutions.end(), subs.begin(), subs.end() );
                sub_expr.erase( sub_expr.begin() + i );
                i--;
                continue;
            }
        } else if ( auto comma_expr = std::dynamic_pointer_cast<CommaExpr>( sub_expr[i] ); comma_expr != nullptr ) {
            // Resolve commas
            sub_expr.erase( sub_expr.begin() + i );
            sub_expr.insert( sub_expr.begin() + i, comma_expr->sub_expr.begin(), comma_expr->sub_expr.end() );
            i--;
            continue;
        }
    }
    return true;
}

bool DeclExpr::symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    c_ctx.current_substitutions.push_back( substitutions );
    return true;
}

bool DeclExpr::post_symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    c_ctx.current_substitutions.pop_back();
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

bool SingleCompletedExpr::first_transformation( CrateCtx &c_ctx, Worker &w_ctx, sptr<Expr> &anchor,
                                                sptr<Expr> parent ) {
    anchor = sub_expr; // resolve single completed exprs
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

bool BlockExpr::first_transformation( CrateCtx &c_ctx, Worker &w_ctx, sptr<Expr> &anchor, sptr<Expr> parent ) {
    if ( std::dynamic_pointer_cast<DeclExpr>( parent ) != nullptr ||
         std::dynamic_pointer_cast<StructExpr>( parent ) != nullptr ||
         std::dynamic_pointer_cast<TraitExpr>( parent ) != nullptr ||
         std::dynamic_pointer_cast<ImplExpr>( parent ) != nullptr ||
         std::dynamic_pointer_cast<ModuleExpr>( parent ) != nullptr ) {
        auto new_decl = make_shared<DeclExpr>();
        new_decl->copy_from_other( shared_from_this() );
        new_decl->sub_expr = sub_expr;
        anchor = new_decl;
    } else {
        // Insert implicit return type
        if ( sub_expr.empty() || std::dynamic_pointer_cast<SingleCompletedExpr>( sub_expr.back() ) != nullptr )
            sub_expr.push_back( make_shared<UnitExpr>() );

        std::vector<sptr<Expr>> annotation_list;
        for ( size_t i = 0; i < sub_expr.size(); i++ ) {
            // Resolve annotations
            if ( std::dynamic_pointer_cast<CompilerAnnotationExpr>( sub_expr[i] ) != nullptr ) {
                annotation_list.push_back( sub_expr[i] );
                sub_expr.erase( sub_expr.begin() + i );
                i--;
                continue;
            } else if ( !annotation_list.empty() ) {
                sub_expr[i]->annotations = std::move( annotation_list );
                annotation_list.clear();
            }
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

MirVarId BlockExpr::parse_mir( CrateCtx &c_ctx, Worker &w_ctx, FunctionImplId func ) {
    c_ctx.curr_living_vars.emplace_back();
    c_ctx.curr_name_mapping.emplace_back();

    // Handle all expressions
    if ( sub_expr.size() > 1 ) {
        for ( auto expr = sub_expr.begin(); expr != sub_expr.end() - 1; expr++ ) {
            ( *expr )->parse_mir( c_ctx, w_ctx, func );
        }
    }
    if ( sub_expr.empty() ) {
        LOG_ERR( "No return value from block" );
    }
    MirVarId ret = sub_expr.back()->parse_mir( c_ctx, w_ctx, func );

    // Drop all created variables
    for ( auto &var : c_ctx.curr_living_vars.back() ) {
        if ( var != ret )
            drop_variable( c_ctx, w_ctx, func, shared_from_this(), var );
    }

    c_ctx.curr_name_mapping.pop_back();
    c_ctx.curr_living_vars.pop_back();
    return ret;
}

bool SetExpr::first_transformation( CrateCtx &c_ctx, Worker &w_ctx, sptr<Expr> &anchor, sptr<Expr> parent ) {
    if ( std::dynamic_pointer_cast<DeclExpr>( parent ) != nullptr ||
         std::dynamic_pointer_cast<StructExpr>( parent ) != nullptr ||
         std::dynamic_pointer_cast<TraitExpr>( parent ) != nullptr ||
         std::dynamic_pointer_cast<ImplExpr>( parent ) != nullptr ||
         std::dynamic_pointer_cast<ModuleExpr>( parent ) != nullptr ) {
        auto new_decl = make_shared<DeclExpr>();
        new_decl->copy_from_other( shared_from_this() );
        new_decl->sub_expr = sub_expr;
        anchor = new_decl;
    } else {
        std::vector<sptr<Expr>> annotation_list;
        for ( size_t i = 0; i < sub_expr.size(); i++ ) {
            // Resolve annotations
            if ( std::dynamic_pointer_cast<CompilerAnnotationExpr>( sub_expr[i] ) != nullptr ) {
                annotation_list.push_back( sub_expr[i] );
                sub_expr.erase( sub_expr.begin() + i );
                i--;
                continue;
            } else if ( !annotation_list.empty() ) {
                sub_expr[i]->annotations = std::move( annotation_list );
                annotation_list.clear();
            }

            // Resolve commas
            if ( auto sc_expr = std::dynamic_pointer_cast<SingleCompletedExpr>( sub_expr[i] ); sc_expr != nullptr ) {
                if ( auto comma_expr = std::dynamic_pointer_cast<CommaExpr>( sc_expr->sub_expr );
                     comma_expr != nullptr ) {
                    sub_expr.erase( sub_expr.begin() + i );
                    sub_expr.insert( sub_expr.begin() + i, comma_expr->sub_expr.begin(), comma_expr->sub_expr.end() );
                    i--;
                    continue;
                }
            } else if ( auto comma_expr = std::dynamic_pointer_cast<CommaExpr>( sub_expr[i] ); comma_expr != nullptr ) {
                sub_expr.erase( sub_expr.begin() + i );
                sub_expr.insert( sub_expr.begin() + i, comma_expr->sub_expr.begin(), comma_expr->sub_expr.end() );
                i--;
                continue;
            }
        }
    }
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

MirVarId AtomicSymbolExpr::parse_mir( CrateCtx &c_ctx, Worker &w_ctx, FunctionImplId func ) {
    auto name_chain = get_symbol_chain_from_expr( std::dynamic_pointer_cast<SymbolExpr>( shared_from_this() ) );
    if ( name_chain->size() != 1 || !name_chain->front().template_values.empty() ) {
        w_ctx.print_msg<MessageType::err_local_variable_scoped>(
            MessageInfo( shared_from_this(), 0, FmtStr::Color::Red ) );
    }

    for ( auto itr = c_ctx.curr_name_mapping.rbegin(); itr != c_ctx.curr_name_mapping.rend(); itr++ ) {
        if ( auto var_itr = itr->find( name_chain->front().name ); var_itr != itr->end() ) {
            return var_itr->second.back();
        }
    }

    LOG_ERR( "Atomic symbol not found" );
    return 0;
}

bool CommaExpr::first_transformation( CrateCtx &c_ctx, Worker &w_ctx, sptr<Expr> &anchor, sptr<Expr> parent ) {
    return true;
}

bool FuncHeadExpr::basic_semantic_check( CrateCtx &c_ctx, Worker &w_ctx ) {
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

bool FuncHeadExpr::first_transformation( CrateCtx &c_ctx, Worker &w_ctx, sptr<Expr> &anchor, sptr<Expr> parent ) {
    if ( std::dynamic_pointer_cast<DeclExpr>( parent ) == nullptr ) {
        auto new_decl = make_shared<FuncCallExpr>();
        new_decl->copy_from_other( shared_from_this() );
        new_decl->symbol = symbol;
        new_decl->parameters = parameters;
        anchor = new_decl;
    } else if ( parameters ) {
        // Check if parameter syntax is correct (can check this here the first time, because previously it could also
        // have been a FuncCall)
        auto paren_expr = std::dynamic_pointer_cast<ParenthesisExpr>( parameters );
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
    }
    return true;
}

bool FuncHeadExpr::symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    if ( auto symbol_symbol = std::dynamic_pointer_cast<SymbolExpr>( symbol ); symbol_symbol ) {
        SymbolId new_id = create_new_local_symbol_from_name_chain( c_ctx, get_symbol_chain_from_expr( symbol_symbol ) );
        symbol_symbol->update_left_symbol_id( c_ctx.symbol_graph[c_ctx.current_scope].sub_nodes.back() );
        symbol_symbol->update_symbol_id( new_id );
        switch_scope_to_symbol( c_ctx, new_id );
        c_ctx.symbol_graph[new_id].original_expr.push_back( symbol );
        c_ctx.symbol_graph[new_id].pub = symbol_symbol->has_attribute( SymbolExpr::Attribute::pub );
        c_ctx.symbol_graph[new_id].type = c_ctx.fn_type;
    }
    return true;
}

bool FuncHeadExpr::post_symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    switch_scope_to_symbol(
        c_ctx, c_ctx.symbol_graph[std::dynamic_pointer_cast<SymbolExpr>( symbol )->get_left_symbol_id()].parent );
    return true;
}

bool FuncExpr::basic_semantic_check( CrateCtx &c_ctx, Worker &w_ctx ) {
    if ( symbol && ( std::dynamic_pointer_cast<SymbolExpr>( symbol ) == nullptr &&
                     std::dynamic_pointer_cast<ArraySpecifierExpr>( symbol ) == nullptr ) ) {
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
                    if ( std::dynamic_pointer_cast<SymbolExpr>( typed->type ) == nullptr &&
                         std::dynamic_pointer_cast<ReferenceExpr>( typed->type ) == nullptr &&
                         std::dynamic_pointer_cast<MutAttrExpr>( typed->type ) == nullptr ) {
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

bool FuncExpr::first_transformation( CrateCtx &c_ctx, Worker &w_ctx, sptr<Expr> &anchor, sptr<Expr> parent ) {
    if ( std::dynamic_pointer_cast<BlockExpr>( body ) == nullptr ) {
        auto new_block = make_shared<BlockExpr>();
        auto sc_expr = std::dynamic_pointer_cast<SingleCompletedExpr>( body );
        new_block->copy_from_other( sc_expr );
        new_block->sub_expr.push_back( sc_expr->sub_expr );
        body = new_block;
    }
    return true;
}

bool FuncExpr::symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    if ( auto symbol_symbol = std::dynamic_pointer_cast<SymbolExpr>( symbol ); symbol_symbol ) {
        SymbolId new_id = create_new_local_symbol_from_name_chain( c_ctx, get_symbol_chain_from_expr( symbol_symbol ) );
        symbol_symbol->update_left_symbol_id( c_ctx.symbol_graph[c_ctx.current_scope].sub_nodes.back() );
        symbol_symbol->update_symbol_id( new_id );
        switch_scope_to_symbol( c_ctx, new_id );
        c_ctx.symbol_graph[new_id].original_expr.push_back( shared_from_this() );
        c_ctx.symbol_graph[new_id].pub = symbol_symbol->has_attribute( SymbolExpr::Attribute::pub );
        c_ctx.symbol_graph[new_id].type = c_ctx.fn_type;
    }
    return true;
}

bool FuncExpr::post_symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    switch_scope_to_symbol(
        c_ctx, c_ctx.symbol_graph[std::dynamic_pointer_cast<SymbolExpr>( symbol )->get_left_symbol_id()].parent );
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

MirVarId FuncCallExpr::parse_mir( CrateCtx &c_ctx, Worker &w_ctx, FunctionImplId func ) {
    auto calls = find_sub_symbol_by_identifier_chain(
        c_ctx, get_symbol_chain_from_expr( std::dynamic_pointer_cast<SymbolExpr>( symbol ) ) );

    for ( auto &candidate : calls ) {
        analyse_function_signature( c_ctx, w_ctx, candidate );
    }

    // TODO select the function based on its signature

    if ( calls.empty() ) {
        w_ctx.print_msg<MessageType::err_symbol_not_found>( MessageInfo( shared_from_this(), 0, FmtStr::Color::Red ),
                                                            std::vector<MessageInfo>() );
    } else if ( calls.size() > 1 ) {
        std::vector<MessageInfo> notes;
        for ( auto &c : calls ) {
            if ( !c_ctx.symbol_graph[c].original_expr.empty() )
                notes.push_back( MessageInfo( c_ctx.symbol_graph[c].original_expr.front(), 1 ) );
        }
        w_ctx.print_msg<MessageType::err_symbol_is_ambiguous>( MessageInfo( shared_from_this(), 0, FmtStr::Color::Red ),
                                                               notes );
    }

    auto param_exprs = std::dynamic_pointer_cast<ParenthesisExpr>( parameters )->get_list();
    std::vector<MirVarId> params;
    for ( auto &pe : param_exprs ) {
        params.push_back( pe->parse_mir( c_ctx, w_ctx, func ) );
    }

    auto &op = create_call( c_ctx, w_ctx, func, shared_from_this(), calls.front(), 0, params );

    return op.ret;
}

MirVarId OperatorExpr::parse_mir( CrateCtx &c_ctx, Worker &w_ctx, FunctionImplId func ) {
    auto calls = find_sub_symbol_by_identifier_chain(
        c_ctx, split_symbol_chain( *fn, w_ctx.unit_ctx()->prelude_conf.scope_access_operator ), c_ctx.current_scope );

    for ( auto &candidate : calls ) {
        analyse_function_signature( c_ctx, w_ctx, candidate );
    }

    // TODO select the function based on its signature

    if ( calls.empty() ) {
        w_ctx.print_msg<MessageType::err_operator_symbol_not_found>(
            MessageInfo( shared_from_this(), 0, FmtStr::Color::Red ), std::vector<MessageInfo>(), *fn, op );
    } else if ( calls.size() > 1 ) {
        std::vector<MessageInfo> notes;
        for ( auto &c : calls ) {
            if ( !c_ctx.symbol_graph[c].original_expr.empty() )
                notes.push_back( MessageInfo( c_ctx.symbol_graph[c].original_expr.front(), 1 ) );
        }
        w_ctx.print_msg<MessageType::err_operator_symbol_is_ambiguous>(
            MessageInfo( shared_from_this(), 0, FmtStr::Color::Red ), notes, *fn, op );
    }


    auto left_result = lvalue->parse_mir( c_ctx, w_ctx, func );
    auto right_result = rvalue->parse_mir( c_ctx, w_ctx, func );

    auto &op = create_call( c_ctx, w_ctx, func, shared_from_this(), calls.front(), 0, { left_result, right_result } );

    return op.ret;
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
        if ( std::dynamic_pointer_cast<SymbolExpr>( lvalue->type ) == nullptr &&
             std::dynamic_pointer_cast<ReferenceExpr>( lvalue->type ) == nullptr &&
             std::dynamic_pointer_cast<MutAttrExpr>( lvalue->type ) == nullptr ) {
            w_ctx.print_msg<MessageType::err_expected_symbol>( MessageInfo( lvalue->type, 0, FmtStr::Color::Red ) );
            return false;
        }
    } else if ( auto lvalue = std::dynamic_pointer_cast<SymbolExpr>( assignment->lvalue ); lvalue == nullptr ) {
        w_ctx.print_msg<MessageType::err_expected_symbol>( MessageInfo( assignment->lvalue, 0, FmtStr::Color::Red ) );
        return false;
    }
    return true;
}

MirVarId SimpleBindExpr::parse_mir( CrateCtx &c_ctx, Worker &w_ctx, FunctionImplId func ) {
    auto assignment = std::dynamic_pointer_cast<OperatorExpr>( expr );
    sptr<SymbolExpr> symbol;
    if ( auto lvalue = std::dynamic_pointer_cast<TypedExpr>( assignment->lvalue ); lvalue != nullptr ) {
        symbol = std::dynamic_pointer_cast<SymbolExpr>( lvalue->symbol );
    } else {
        symbol = std::dynamic_pointer_cast<SymbolExpr>( assignment->lvalue );
    }

    auto name_chain = get_symbol_chain_from_expr( symbol );
    if ( name_chain->size() != 1 || !name_chain->front().template_values.empty() ) {
        w_ctx.print_msg<MessageType::err_local_variable_scoped>( MessageInfo( symbol, 0, FmtStr::Color::Red ) );
    }

    // Create variable
    create_variable( c_ctx, w_ctx, func, name_chain->front().name );

    // Expr operation
    auto var = assignment->parse_mir( c_ctx, w_ctx, func );
    drop_variable( c_ctx, w_ctx, func, shared_from_this(), var );

    return 0;
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

std::vector<SymbolSubstitution> AliasBindExpr::get_substitutions() {
    std::vector<SymbolSubstitution> result;
    if ( auto assignment = std::dynamic_pointer_cast<OperatorExpr>( expr ); assignment != nullptr ) {
        result.push_back(
            { get_symbol_chain_from_expr( std::dynamic_pointer_cast<SymbolExpr>( assignment->lvalue ) ),
              get_symbol_chain_from_expr( std::dynamic_pointer_cast<SymbolExpr>( assignment->rvalue ) ) } );
    } else {
        auto chain = get_symbol_chain_from_expr( std::dynamic_pointer_cast<SymbolExpr>( expr ) );
        result.push_back( { make_shared<std::vector<SymbolIdentifier>>( 1, chain->back() ), chain } );
    }
    return result;
}

bool IfExpr::first_transformation( CrateCtx &c_ctx, Worker &w_ctx, sptr<Expr> &anchor, sptr<Expr> parent ) {
    if ( std::dynamic_pointer_cast<BlockExpr>( expr_t ) == nullptr ) {
        auto new_block = make_shared<BlockExpr>();
        auto sc_expr = std::dynamic_pointer_cast<SingleCompletedExpr>( expr_t );
        new_block->copy_from_other( sc_expr );
        new_block->sub_expr.push_back( sc_expr->sub_expr );
        expr_t = new_block;
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

bool IfElseExpr::first_transformation( CrateCtx &c_ctx, Worker &w_ctx, sptr<Expr> &anchor, sptr<Expr> parent ) {
    if ( std::dynamic_pointer_cast<BlockExpr>( expr_t ) == nullptr ) {
        auto new_block = make_shared<BlockExpr>();
        auto sc_expr = std::dynamic_pointer_cast<SingleCompletedExpr>( expr_t );
        new_block->copy_from_other( sc_expr );
        new_block->sub_expr.push_back( sc_expr->sub_expr );
        expr_t = new_block;
    }
    if ( std::dynamic_pointer_cast<BlockExpr>( expr_f ) == nullptr ) {
        auto new_block = make_shared<BlockExpr>();
        auto sc_expr = std::dynamic_pointer_cast<SingleCompletedExpr>( expr_f );
        new_block->copy_from_other( sc_expr );
        new_block->sub_expr.push_back( sc_expr->sub_expr );
        expr_f = new_block;
    }
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

bool PreLoopExpr::first_transformation( CrateCtx &c_ctx, Worker &w_ctx, sptr<Expr> &anchor, sptr<Expr> parent ) {
    if ( std::dynamic_pointer_cast<BlockExpr>( expr ) == nullptr ) {
        auto new_block = make_shared<BlockExpr>();
        auto sc_expr = std::dynamic_pointer_cast<SingleCompletedExpr>( expr );
        new_block->copy_from_other( sc_expr );
        new_block->sub_expr.push_back( sc_expr->sub_expr );
        expr = new_block;
    }
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

bool PostLoopExpr::first_transformation( CrateCtx &c_ctx, Worker &w_ctx, sptr<Expr> &anchor, sptr<Expr> parent ) {
    if ( std::dynamic_pointer_cast<BlockExpr>( expr ) == nullptr ) {
        auto new_block = make_shared<BlockExpr>();
        auto sc_expr = std::dynamic_pointer_cast<SingleCompletedExpr>( expr );
        new_block->copy_from_other( sc_expr );
        new_block->sub_expr.push_back( sc_expr->sub_expr );
        expr = new_block;
    }
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

bool InfLoopExpr::first_transformation( CrateCtx &c_ctx, Worker &w_ctx, sptr<Expr> &anchor, sptr<Expr> parent ) {
    if ( std::dynamic_pointer_cast<BlockExpr>( expr ) == nullptr ) {
        auto new_block = make_shared<BlockExpr>();
        auto sc_expr = std::dynamic_pointer_cast<SingleCompletedExpr>( expr );
        new_block->copy_from_other( sc_expr );
        new_block->sub_expr.push_back( sc_expr->sub_expr );
        expr = new_block;
    }
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

bool ItrLoopExpr::first_transformation( CrateCtx &c_ctx, Worker &w_ctx, sptr<Expr> &anchor, sptr<Expr> parent ) {
    if ( std::dynamic_pointer_cast<BlockExpr>( expr ) == nullptr ) {
        auto new_block = make_shared<BlockExpr>();
        auto sc_expr = std::dynamic_pointer_cast<SingleCompletedExpr>( expr );
        new_block->copy_from_other( sc_expr );
        new_block->sub_expr.push_back( sc_expr->sub_expr );
        expr = new_block;
    }
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

bool MatchExpr::first_transformation( CrateCtx &c_ctx, Worker &w_ctx, sptr<Expr> &anchor, sptr<Expr> parent ) {
    if ( auto set = std::dynamic_pointer_cast<BlockExpr>( cases ); set != nullptr ) {
        auto new_block = make_shared<SetExpr>();
        new_block->copy_from_other( cases );
        new_block->sub_expr = set->sub_expr;
        cases = new_block;
    } else if ( std::dynamic_pointer_cast<SetExpr>( cases ) == nullptr ) {
        auto new_block = make_shared<SetExpr>();
        new_block->copy_from_other( cases );
        new_block->sub_expr.push_back( cases );
        cases = new_block;
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

bool StructExpr::first_transformation( CrateCtx &c_ctx, Worker &w_ctx, sptr<Expr> &anchor, sptr<Expr> parent ) {
    if ( std::dynamic_pointer_cast<BlockExpr>( body ) == nullptr &&
         std::dynamic_pointer_cast<SetExpr>( body ) == nullptr ) {
        auto new_block = make_shared<DeclExpr>();
        new_block->copy_from_other( body );
        new_block->sub_expr.push_back( body );
        body = new_block;
    }
    return true;
}

bool StructExpr::symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    if ( auto symbol_symbol = std::dynamic_pointer_cast<SymbolExpr>( name ); symbol_symbol ) {
        SymbolId new_id = create_new_local_symbol_from_name_chain( c_ctx, get_symbol_chain_from_expr( symbol_symbol ) );
        symbol_symbol->update_left_symbol_id( c_ctx.symbol_graph[c_ctx.current_scope].sub_nodes.back() );
        symbol_symbol->update_symbol_id( new_id );
        switch_scope_to_symbol( c_ctx, new_id );
        c_ctx.symbol_graph[new_id].original_expr.push_back( shared_from_this() );
        c_ctx.symbol_graph[new_id].pub = symbol_symbol->has_attribute( SymbolExpr::Attribute::pub );
        c_ctx.symbol_graph[new_id].type = c_ctx.struct_type;

        // Handle type
        if ( c_ctx.symbol_graph[new_id].value == 0 )
            create_new_type( c_ctx, new_id );

        // Handle Members
        for ( auto &expr : std::dynamic_pointer_cast<DeclExpr>( body )->sub_expr ) {
            sptr<SymbolExpr> symbol;

            // Select symbol
            if ( auto trait_expr = std::dynamic_pointer_cast<TypedExpr>( expr ); trait_expr != nullptr ) {
                symbol = std::dynamic_pointer_cast<SymbolExpr>( trait_expr->symbol );
            } else if ( auto symbol_expr = std::dynamic_pointer_cast<SymbolExpr>( expr ); symbol_expr != nullptr ) {
                symbol = symbol_expr;
            } else {
                LOG_ERR( "Struct member is not a symbol" );
                return false;
            }

            // Check if scope is right
            auto identifier_list = get_symbol_chain_from_expr( symbol );
            if ( identifier_list->size() != 1 ) {
                w_ctx.print_msg<MessageType::err_member_in_invalid_scope>(
                    MessageInfo( symbol, 0, FmtStr::Color::Red ) );
                return false;
            }

            // Check if symbol doesn't already exits
            auto indices = find_member_symbol_by_identifier( c_ctx, identifier_list->front(), new_id );
            auto &members = c_ctx.type_table[c_ctx.symbol_graph[new_id].type].members;
            if ( !indices.empty() ) {
                std::vector<MessageInfo> notes;
                for ( auto &idx : indices ) {
                    if ( !members[idx].original_expr.empty() )
                        notes.push_back( MessageInfo( members[idx].original_expr.front(), 1 ) );
                }
                w_ctx.print_msg<MessageType::err_symbol_is_ambiguous>( MessageInfo( symbol, 0, FmtStr::Color::Red ),
                                                                       notes );
                return false;
            }

            // Create member
            auto &new_member = create_new_member_symbol( c_ctx, identifier_list->front(), new_id );
            new_member.original_expr.push_back( expr );
            new_member.pub = symbol->has_attribute( SymbolExpr::Attribute::pub );
        }
    }
    return true;
}

bool StructExpr::post_symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    switch_scope_to_symbol(
        c_ctx, c_ctx.symbol_graph[std::dynamic_pointer_cast<SymbolExpr>( name )->get_left_symbol_id()].parent );
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

bool TraitExpr::first_transformation( CrateCtx &c_ctx, Worker &w_ctx, sptr<Expr> &anchor, sptr<Expr> parent ) {
    if ( std::dynamic_pointer_cast<BlockExpr>( body ) == nullptr &&
         std::dynamic_pointer_cast<SetExpr>( body ) == nullptr ) {
        auto new_block = make_shared<DeclExpr>();
        new_block->copy_from_other( body );
        new_block->sub_expr.push_back( body );
        body = new_block;
    }
    return true;
}

bool TraitExpr::symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    if ( auto symbol_symbol = std::dynamic_pointer_cast<SymbolExpr>( name ); symbol_symbol ) {
        SymbolId new_id = create_new_local_symbol_from_name_chain( c_ctx, get_symbol_chain_from_expr( symbol_symbol ) );
        symbol_symbol->update_left_symbol_id( c_ctx.symbol_graph[c_ctx.current_scope].sub_nodes.back() );
        symbol_symbol->update_symbol_id( new_id );
        switch_scope_to_symbol( c_ctx, new_id );
        c_ctx.symbol_graph[new_id].original_expr.push_back( shared_from_this() );
        c_ctx.symbol_graph[new_id].pub = symbol_symbol->has_attribute( SymbolExpr::Attribute::pub );
        c_ctx.symbol_graph[new_id].type = c_ctx.trait_type;

        // Handle type
        if ( c_ctx.symbol_graph[new_id].value == 0 )
            create_new_type( c_ctx, new_id );
    }
    return true;
}

bool TraitExpr::post_symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    switch_scope_to_symbol(
        c_ctx, c_ctx.symbol_graph[std::dynamic_pointer_cast<SymbolExpr>( name )->get_left_symbol_id()].parent );
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

bool ImplExpr::first_transformation( CrateCtx &c_ctx, Worker &w_ctx, sptr<Expr> &anchor, sptr<Expr> parent ) {
    if ( std::dynamic_pointer_cast<BlockExpr>( body ) == nullptr &&
         std::dynamic_pointer_cast<SetExpr>( body ) == nullptr ) {
        auto new_block = make_shared<DeclExpr>();
        new_block->copy_from_other( body );
        new_block->sub_expr.push_back( body );
        body = new_block;
    }
    return true;
}

bool ImplExpr::symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    if ( auto symbol_symbol = std::dynamic_pointer_cast<SymbolExpr>( struct_name ); symbol_symbol ) {
        SymbolId new_id = create_new_local_symbol_from_name_chain( c_ctx, get_symbol_chain_from_expr( symbol_symbol ) );
        symbol_symbol->update_left_symbol_id( c_ctx.symbol_graph[c_ctx.current_scope].sub_nodes.back() );
        symbol_symbol->update_symbol_id( new_id );
        switch_scope_to_symbol( c_ctx, new_id );
        c_ctx.symbol_graph[new_id].original_expr.push_back( shared_from_this() );
        c_ctx.symbol_graph[new_id].pub = symbol_symbol->has_attribute( SymbolExpr::Attribute::pub );
        c_ctx.symbol_graph[new_id].type = c_ctx.struct_type;
    }
    return true;
}

bool ImplExpr::post_symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    switch_scope_to_symbol(
        c_ctx, c_ctx.symbol_graph[std::dynamic_pointer_cast<SymbolExpr>( struct_name )->get_left_symbol_id()].parent );
    return true;
}

bool ReferenceExpr::basic_semantic_check( CrateCtx &c_ctx, Worker &w_ctx ) {
    if ( std::dynamic_pointer_cast<SymbolExpr>( symbol ) == nullptr ) {
        w_ctx.print_msg<MessageType::err_expected_symbol>( MessageInfo( symbol, 0, FmtStr::Color::Red ) );
        return false;
    }
    return true;
}

bool ReferenceExpr::first_transformation( CrateCtx &c_ctx, Worker &w_ctx, sptr<Expr> &anchor, sptr<Expr> parent ) {
    std::dynamic_pointer_cast<SymbolExpr>( symbol )->set_attribute( SymbolExpr::Attribute::ref );
    anchor = symbol;
    return true;
}

bool MutAttrExpr::basic_semantic_check( CrateCtx &c_ctx, Worker &w_ctx ) {
    if ( std::dynamic_pointer_cast<SymbolExpr>( symbol ) == nullptr &&
         std::dynamic_pointer_cast<ReferenceExpr>( symbol ) == nullptr ) {
        w_ctx.print_msg<MessageType::err_expected_symbol>( MessageInfo( symbol, 0, FmtStr::Color::Red ) );
        return false;
    }
    return true;
}

bool MutAttrExpr::first_transformation( CrateCtx &c_ctx, Worker &w_ctx, sptr<Expr> &anchor, sptr<Expr> parent ) {
    auto symbol_symbol = std::dynamic_pointer_cast<SymbolExpr>( symbol );
    if ( symbol_symbol == nullptr ) {
        symbol_symbol =
            std::dynamic_pointer_cast<SymbolExpr>( std::dynamic_pointer_cast<ReferenceExpr>( symbol )->symbol );
    }
    symbol_symbol->set_attribute( SymbolExpr::Attribute::mut );
    anchor = symbol; // don't skip the possible ReferenceExpr
    return true;
}

MirVarId TypedExpr::parse_mir( CrateCtx &c_ctx, Worker &w_ctx, FunctionImplId func ) {
    auto ret = symbol->parse_mir( c_ctx, w_ctx, func );

    auto type_ids = find_sub_symbol_by_identifier_chain(
        c_ctx, get_symbol_chain_from_expr( std::dynamic_pointer_cast<SymbolExpr>( type ) ), c_ctx.current_scope );

    if ( type_ids.empty() ) {
        w_ctx.print_msg<MessageType::err_symbol_not_found>( MessageInfo( type, 0, FmtStr::Color::Red ) );
        return false;
    } else if ( type_ids.size() != 1 ) {
        std::vector<MessageInfo> notes;
        for ( auto &tid : type_ids ) {
            if ( !c_ctx.symbol_graph[tid].original_expr.empty() )
                notes.push_back( MessageInfo( c_ctx.symbol_graph[tid].original_expr.front(), 1 ) );
        }
        w_ctx.print_msg<MessageType::err_symbol_is_ambiguous>( MessageInfo( type, 0, FmtStr::Color::Red ), notes );
    }

    // Set type
    auto &op = create_operation( c_ctx, w_ctx, func, shared_from_this(), MirEntry::Type::type, ret, {} );
    op.symbol = type_ids.front();
    if ( c_ctx.functions[func].vars[ret].value_type == 0 ) {
        c_ctx.functions[func].vars[ret].value_type = c_ctx.symbol_graph[type_ids.front()].value;
    }

    return ret;
}

bool ModuleExpr::first_transformation( CrateCtx &c_ctx, Worker &w_ctx, sptr<Expr> &anchor, sptr<Expr> parent ) {
    if ( std::dynamic_pointer_cast<BlockExpr>( body ) == nullptr ) {
        auto new_block = make_shared<DeclExpr>();
        auto sc_expr = std::dynamic_pointer_cast<SingleCompletedExpr>( body );
        new_block->copy_from_other( sc_expr );
        new_block->sub_expr.push_back( sc_expr->sub_expr );
        body = new_block;
    }
    return true;
}

bool ModuleExpr::symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    if ( auto symbol_symbol = std::dynamic_pointer_cast<SymbolExpr>( symbol ); symbol_symbol ) {
        SymbolId new_id = create_new_local_symbol_from_name_chain( c_ctx, get_symbol_chain_from_expr( symbol_symbol ) );
        symbol_symbol->update_left_symbol_id( c_ctx.symbol_graph[c_ctx.current_scope].sub_nodes.back() );
        symbol_symbol->update_symbol_id( new_id );
        switch_scope_to_symbol( c_ctx, new_id );
        c_ctx.symbol_graph[new_id].original_expr.push_back( shared_from_this() );
        c_ctx.symbol_graph[new_id].pub = symbol_symbol->has_attribute( SymbolExpr::Attribute::pub );
        c_ctx.symbol_graph[new_id].type = c_ctx.mod_type;
    }
    return true;
}

bool ModuleExpr::post_symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    switch_scope_to_symbol(
        c_ctx, c_ctx.symbol_graph[std::dynamic_pointer_cast<SymbolExpr>( symbol )->get_left_symbol_id()].parent );
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
                std::dynamic_pointer_cast<FuncExpr>( symbol ) == nullptr &&
                std::dynamic_pointer_cast<StructExpr>( symbol ) == nullptr &&
                std::dynamic_pointer_cast<TraitExpr>( symbol ) == nullptr &&
                std::dynamic_pointer_cast<ImplExpr>( symbol ) == nullptr &&
                std::dynamic_pointer_cast<ModuleExpr>( symbol ) == nullptr ) {
        w_ctx.print_msg<MessageType::err_expected_symbol>( MessageInfo( symbol, 0, FmtStr::Color::Red ) );
        return false;
    }
    return true;
}

bool PublicAttrExpr::first_transformation( CrateCtx &c_ctx, Worker &w_ctx, sptr<Expr> &anchor, sptr<Expr> parent ) {
    if ( std::dynamic_pointer_cast<DeclExpr>( parent ) == nullptr ) {
        // public symbols are only allowed in decl scopes
        w_ctx.print_msg<MessageType::err_public_not_allowed_in_context>(
            MessageInfo( shared_from_this(), 0, FmtStr::Color::Red ) );
        return false;
    }

    // Resolve public attribute
    if ( auto typed = std::dynamic_pointer_cast<TypedExpr>( symbol ); typed != nullptr ) {
        std::dynamic_pointer_cast<SymbolExpr>( typed->symbol )->set_attribute( SymbolExpr::Attribute::pub );
    } else if ( auto symbol_expr = std::dynamic_pointer_cast<SymbolExpr>( symbol ); symbol_expr != nullptr ) {
        symbol_expr->set_attribute( SymbolExpr::Attribute::pub );
    } else if ( auto fn = std::dynamic_pointer_cast<FuncHeadExpr>( symbol ); fn != nullptr ) {
        std::dynamic_pointer_cast<SymbolExpr>( fn->symbol )->set_attribute( SymbolExpr::Attribute::pub );
    } else if ( auto fn = std::dynamic_pointer_cast<FuncExpr>( symbol ); fn != nullptr ) {
        std::dynamic_pointer_cast<SymbolExpr>( fn->symbol )->set_attribute( SymbolExpr::Attribute::pub );
    } else if ( auto structure = std::dynamic_pointer_cast<StructExpr>( symbol ); structure != nullptr ) {
        std::dynamic_pointer_cast<SymbolExpr>( structure->name )->set_attribute( SymbolExpr::Attribute::pub );
    } else if ( auto trait = std::dynamic_pointer_cast<TraitExpr>( symbol ); trait != nullptr ) {
        std::dynamic_pointer_cast<SymbolExpr>( trait->name )->set_attribute( SymbolExpr::Attribute::pub );
    } else if ( auto impl = std::dynamic_pointer_cast<ImplExpr>( symbol ); impl != nullptr ) {
        std::dynamic_pointer_cast<SymbolExpr>( impl->struct_name )->set_attribute( SymbolExpr::Attribute::pub );
    } else if ( auto mod = std::dynamic_pointer_cast<ModuleExpr>( symbol ); mod != nullptr ) {
        std::dynamic_pointer_cast<SymbolExpr>( mod->symbol )->set_attribute( SymbolExpr::Attribute::pub );
    }

    anchor = symbol;

    return true;
}

bool StaticStatementExpr::first_transformation( CrateCtx &c_ctx, Worker &w_ctx, sptr<Expr> &anchor,
                                                sptr<Expr> parent ) {
    if ( std::dynamic_pointer_cast<BlockExpr>( body ) == nullptr ) {
        auto new_block = make_shared<BlockExpr>();
        auto sc_expr = std::dynamic_pointer_cast<SingleCompletedExpr>( body );
        new_block->copy_from_other( sc_expr );
        new_block->sub_expr.push_back( sc_expr->sub_expr );
        body = new_block;
    }
    return true;
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

bool UnsafeExpr::first_transformation( CrateCtx &c_ctx, Worker &w_ctx, sptr<Expr> &anchor, sptr<Expr> parent ) {
    if ( std::dynamic_pointer_cast<BlockExpr>( block ) == nullptr ) {
        auto new_block = make_shared<BlockExpr>();
        auto sc_expr = std::dynamic_pointer_cast<SingleCompletedExpr>( block );
        new_block->copy_from_other( sc_expr );
        new_block->sub_expr.push_back( sc_expr->sub_expr );
        block = new_block;
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

bool TemplateExpr::first_transformation( CrateCtx &c_ctx, Worker &w_ctx, sptr<Expr> &anchor, sptr<Expr> parent ) {
    if ( attributes.size() == 1 ) {
        if ( auto comma_list = std::dynamic_pointer_cast<CommaExpr>( attributes.front() ); comma_list != nullptr ) {
            attributes = comma_list->sub_expr;
        }
    }

    return true;
}

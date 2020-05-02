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
#include "libpushc/MirTranslation.h"
#include "libpushc/AstParser.h"
#include "libpushc/Expression.h"
#include "libpushc/SymbolUtil.h"


MirEntry &create_operation( CrateCtx &c_ctx, Worker &w_ctx, FunctionImplId function, AstNode &original_expr,
                            MirEntry::Type type, MirVarId result, std::vector<MirVarId> parameters ) {
    MirVarId return_var = result;
    if ( result == 0 ) {
        return_var = create_variable( c_ctx, w_ctx, function, "" );
    }

    // Check if parameters are valid
    for ( auto &param : parameters ) {
        bool valid = false;
        for ( auto itr = c_ctx.curr_living_vars.rbegin(); itr != c_ctx.curr_living_vars.rend(); itr++ ) {
            if ( std::find( itr->begin(), itr->end(), param ) != itr->end() ) {
                // Variable is accessible
                valid = true;
                break;
            }
        }
        if ( !valid ) {
            // TODO detect which variable in the operation and add a note where the variable ended
            w_ctx.print_msg<MessageType::err_var_not_living>( MessageInfo( original_expr, 0, FmtStr::Color::Red ) );
        }
    }

    auto operation = MirEntry{ &original_expr, type, return_var, parameters };
    c_ctx.functions[function].ops.push_back( operation );
    return c_ctx.functions[function].ops.back();
}

MirEntry &create_call( CrateCtx &c_ctx, Worker &w_ctx, FunctionImplId calling_function, AstNode &original_expr,
                       SymbolId called_function, MirVarId result, std::vector<MirVarId> parameters ) {
    FunctionImpl &caller = c_ctx.functions[calling_function];
    SymbolGraphNode &callee = c_ctx.symbol_graph[called_function];
    if ( parameters.size() != callee.identifier.parameters.size() )
        LOG_ERR( "Expected and provided parameter count differ" );

    // Create call operation
    auto &op =
        create_operation( c_ctx, w_ctx, calling_function, original_expr, MirEntry::Type::call, result, parameters );
    op.symbol = called_function;
    caller.vars[op.ret].type = MirVariable::Type::rvalue;
    if ( caller.vars[op.ret].value_type == 0 ) {
        caller.vars[op.ret].value_type = callee.identifier.eval_type.type;
    }


    // Handle parameter remains
    for ( size_t i = 0; i < parameters.size(); i++ ) {
        if ( !callee.identifier.parameters[i].ref ) {
            if ( caller.vars[parameters[i]].type == MirVariable::Type::rvalue ) {
                drop_variable( c_ctx, w_ctx, calling_function, original_expr, parameters[i] );
            }
        } else {
            // Remove from living variables
            for ( auto itr = c_ctx.curr_living_vars.rbegin(); itr != c_ctx.curr_living_vars.rend(); itr++ ) {
                if ( auto var_itr = std::find( itr->begin(), itr->end(), parameters[i] ); var_itr != itr->end() ) {
                    itr->erase( var_itr );
                    break;
                }
            }
        }
    }

    return op;
}

MirVarId create_variable( CrateCtx &c_ctx, Worker &w_ctx, FunctionImplId function, const String &name ) {
    MirVarId id = c_ctx.functions[function].vars.size();
    c_ctx.functions[function].vars.emplace_back();
    c_ctx.functions[function].vars[id].name = name;
    c_ctx.curr_living_vars.back().push_back( id );
    if ( !name.empty() ) {
        c_ctx.curr_name_mapping.back()[name].push_back( id );
    }
    return id;
}

void drop_variable( CrateCtx &c_ctx, Worker &w_ctx, FunctionImplId function, AstNode &original_expr,
                    MirVarId variable ) {
    if ( variable == 0 )
        return;

    auto &var = c_ctx.functions[function].vars[variable];

    // Create drop operation
    if ( var.type == MirVariable::Type::value || var.type == MirVariable::Type::rvalue ) {
        auto op =
            MirEntry{ &original_expr, MirEntry::Type::call, 0, { variable }, c_ctx.type_table[c_ctx.drop_fn].symbol };
        c_ctx.functions[function].ops.push_back( op );
    }

    // Remove from living variables
    for ( auto itr = c_ctx.curr_living_vars.rbegin(); itr != c_ctx.curr_living_vars.rend(); itr++ ) {
        if ( auto var_itr = std::find( itr->begin(), itr->end(), variable ); var_itr != itr->end() ) {
            itr->erase( var_itr );
            break;
        }
    }

    // Remove from name mapping
    if ( !var.name.empty() ) {
        for ( auto itr = c_ctx.curr_name_mapping.rbegin(); itr != c_ctx.curr_name_mapping.rend(); itr++ ) {
            if ( auto var_itr = itr->find( var.name ); var_itr != itr->end() ) {
                var_itr->second.pop_back();
                if ( var_itr->second.empty() )
                    itr->erase( var_itr );
                break;
            }
        }
    }
}

void analyse_function_signature( CrateCtx &c_ctx, Worker &w_ctx, SymbolId function ) {
    auto &symbol = c_ctx.symbol_graph[function];
    if ( function && symbol.identifier.eval_type.type == 0 ) {
        auto &expr = *symbol.original_expr.front();
        if ( expr.type != ExprType::func ) {
            LOG_ERR( "Function to analyse is not a function" );
            return;
        }

        // Parameters
        if ( expr.named.find( AstChild::parameters ) != expr.named.end() ) {
            auto &paren_expr = expr.named[AstChild::parameters];
            for ( auto &entry : paren_expr.children ) {
                auto &new_parameter = symbol.identifier.parameters.emplace_back();
                auto *parameter_symbol = &entry;
                if ( parameter_symbol->type == ExprType::typed_op ) {
                    parameter_symbol = &entry.named[AstChild::left_expr];
                    auto &type_symbol = entry.named[AstChild::right_expr];
                    auto types = find_sub_symbol_by_identifier_chain( c_ctx, w_ctx, type_symbol.get_symbol_chain(),
                                                                      c_ctx.current_scope );

                    if ( !expect_exactly_one_symbol( c_ctx, w_ctx, types, type_symbol ) )
                        continue;

                    new_parameter.type = c_ctx.symbol_graph[types.front()].value;
                    new_parameter.ref = type_symbol.has_prop( ExprProperty::ref );
                    new_parameter.mut = type_symbol.has_prop( ExprProperty::mut );
                }

                auto symbol_chain = parameter_symbol->get_symbol_chain();
                if ( !expect_unscoped_variable( c_ctx, w_ctx, *symbol_chain, *parameter_symbol ) )
                    continue;
                new_parameter.name = symbol_chain->front().name;
            }
        }

        // Return value
        if ( expr.named.find( AstChild::return_type ) != expr.named.end() ) {
            auto return_symbol = expr.named[AstChild::return_type];
            auto return_symbols = find_sub_symbol_by_identifier_chain( c_ctx, w_ctx, return_symbol.get_symbol_chain(),
                                                                       c_ctx.current_scope );

            if ( !expect_exactly_one_symbol( c_ctx, w_ctx, return_symbols, return_symbol ) )
                return;

            symbol.identifier.eval_type.type = c_ctx.symbol_graph[return_symbols.front()].value;
            symbol.identifier.eval_type.ref = return_symbol.has_prop( ExprProperty::ref );
            symbol.identifier.eval_type.mut = return_symbol.has_prop( ExprProperty::mut );
        }
    }
}



// Creates a function from a FuncExpr specified by @param symbolId
void generate_mir_function_impl( CrateCtx &c_ctx, Worker &w_ctx, SymbolId symbol_id ) {
    auto &symbol = c_ctx.symbol_graph[symbol_id];
    auto expr = *symbol.original_expr.front();

    // Check if only one definition exists (must be at least one)
    if ( symbol.original_expr.size() > 1 ) {
        std::vector<MessageInfo> notes;
        for ( auto &original : symbol.original_expr ) {
            notes.push_back( MessageInfo( *original, 1 ) );
        }
        notes.erase( notes.begin() );
        w_ctx.print_msg<MessageType::err_multiple_fn_definitions>(
            MessageInfo( *symbol.original_expr.front(), 0, FmtStr::Color::Red ), notes );
        return;
    }

    c_ctx.curr_living_vars.clear();
    c_ctx.curr_living_vars.emplace_back();
    c_ctx.curr_name_mapping.clear();
    c_ctx.curr_name_mapping.emplace_back();

    // Create the function
    FunctionImplId func_id = c_ctx.functions.size();
    c_ctx.functions.emplace_back();
    FunctionImpl &function = c_ctx.functions.back();
    create_variable( c_ctx, w_ctx, func_id, "" ); // unit return value
    analyse_function_signature( c_ctx, w_ctx, symbol_id );
    function.type = symbol.value;

    // Parse parameters TODO extract this from already existing data from analyse_function_signature()
    auto paren_expr = expr.named[AstChild::parameters];
    for ( auto &entry : paren_expr.children ) {
        auto *symbol = &entry;
        AstNode *type = nullptr;
        if ( symbol->type == ExprType::typed_op ) {
            symbol = &entry.named[AstChild::left_expr];
            type = &entry.named[AstChild::right_expr];
        }

        MirVarId id = create_variable( c_ctx, w_ctx, func_id );
        function.params.push_back( id );

        auto name_chain = symbol->get_symbol_chain();
        if ( !expect_unscoped_variable( c_ctx, w_ctx, *name_chain, *symbol ) )
            continue;

        function.vars[id].name = name_chain->front().name;
        function.vars[id].type = MirVariable::Type::value;
        c_ctx.curr_name_mapping.back()[name_chain->front().name].push_back( id );
        c_ctx.curr_living_vars.back().push_back( id );
        if ( type != nullptr ) {
            auto symbols =
                find_sub_symbol_by_identifier_chain( c_ctx, w_ctx, type->get_symbol_chain(), c_ctx.current_scope );

            if ( !expect_exactly_one_symbol( c_ctx, w_ctx, symbols, *type ) )
                continue;

            function.vars[id].value_type = c_ctx.symbol_graph[symbols.front()].value;
            function.vars[id].mut = type->has_prop( ExprProperty::mut );
            if ( type->has_prop( ExprProperty::ref ) )
                function.vars[id].type = MirVariable::Type::p_ref;
        }
    }

    // Parse body
    function.ret = expr.children.front().parse_mir( c_ctx, w_ctx, func_id );

    // Drop parameters
    for ( auto &p : function.params ) {
        drop_variable( c_ctx, w_ctx, func_id, expr, p );
    }
}

void get_mir( JobsBuilder &jb, UnitCtx &parent_ctx ) {
    jb.add_job<void>( []( Worker &w_ctx ) {
        auto c_ctx = w_ctx.do_query( get_ast )->jobs.back()->to<sptr<CrateCtx>>();

        // Generate the Mir function bodies
        for ( size_t i = 0; i < c_ctx->symbol_graph.size(); i++ ) {
            if ( c_ctx->symbol_graph[i].original_expr.size() == 1 &&
                 c_ctx->symbol_graph[i].original_expr.front()->type == ExprType::func ) {
                generate_mir_function_impl( *c_ctx, w_ctx, i );
            }
        }

        // DEBUG print MIR
        log( "MIR FUNCTIONS --" );
        for ( size_t i = 1; i < c_ctx->functions.size(); i++ ) {
            auto &fn = c_ctx->functions[i];
            auto get_var_name = [&fn]( MirVarId id ) -> String {
                if ( id == 0 )
                    return " ()";
                String type_str = ( fn.vars[id].type == MirVariable::Type::rvalue
                                        ? "r"
                                        : fn.vars[id].type == MirVariable::Type::l_ref
                                              ? "l"
                                              : fn.vars[id].type == MirVariable::Type::p_ref
                                                    ? "p"
                                                    : fn.vars[id].type == MirVariable::Type::label ? "b" : "" );
                return " " + fn.vars[id].name + "%" + type_str + to_string( id );
            };

            log( " fn " + to_string( i ) + " - " +
                 get_full_symbol_name( *c_ctx, w_ctx, c_ctx->type_table[fn.type].symbol ) );

            // Parameters
            for ( auto &p : fn.params ) {
                log( "  param" + get_var_name( p ) );
            }

            // Operations
            for ( const auto &op : fn.ops ) {
                String str;
                if ( op.type == MirEntry::Type::nop )
                    str = "nop";
                else if ( op.type == MirEntry::Type::intrinsic )
                    str = "intrinsic";
                else if ( op.type == MirEntry::Type::type )
                    str = "type";
                else if ( op.type == MirEntry::Type::literal )
                    str = "literal";
                else if ( op.type == MirEntry::Type::call )
                    str = "call";
                else if ( op.type == MirEntry::Type::member )
                    str = "member";
                else if ( op.type == MirEntry::Type::label )
                    str = "label";
                else if ( op.type == MirEntry::Type::cond_jmp_z )
                    str = "cond_jmp_z";
                else if ( op.type == MirEntry::Type::cast )
                    str = "cast";

                if ( op.symbol != 0 )
                    str += " " + get_full_symbol_name( *c_ctx, w_ctx, op.symbol );

                if ( op.intrinsic != MirIntrinsic::none )
                    str += " intrinsic " + to_string( static_cast<u32>( op.intrinsic ) );

                str += get_var_name( op.ret );

                if ( op.type == MirEntry::Type::literal )
                    str += " 0d" + to_string( op.data );

                for ( auto &p : op.params ) {
                    str += get_var_name( p );
                }

                log( "  " + str );
            }

            // Return value
            log( "  ret" + get_var_name( fn.ret ) );

            // Variables
            log( "\n  VARS:" );
            for ( size_t i = 1; i < fn.vars.size(); i++ ) {
                log( "  " + get_var_name( i ) + " :" + ( fn.vars[i].mut ? "mut" : "" ) +
                     ( fn.vars[i].type == MirVariable::Type::l_ref || fn.vars[i].type == MirVariable::Type::p_ref
                           ? "& "
                           : " " ) +
                     get_full_symbol_name( *c_ctx, w_ctx, c_ctx->type_table[fn.vars[i].value_type].symbol ) +
                     ( fn.vars[i].ref != 0
                           ? " -> " + get_var_name( fn.vars[i].ref ) + " +" + to_string( fn.vars[i].member_idx )
                           : "" ) );
            }
        }
        log( "----------------" );
    } );
}

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
#include "libpushc/TypeInference.h"


MirEntryId create_operation( CrateCtx &c_ctx, Worker &w_ctx, FunctionImplId function, AstNode &original_expr,
                             MirEntry::Type type, MirVarId result, ParamContainer parameters ) {
    MirVarId return_var = result;
    if ( result == 0 ) {
        return_var = create_variable( c_ctx, w_ctx, function, &original_expr, "" );
    }

    auto operation = MirEntry{ &original_expr, type, return_var, parameters };
    c_ctx.functions[function].ops.push_back( operation );
    return c_ctx.functions[function].ops.size() - 1;
}

// Collects message information from symbol locations
std::vector<MessageInfo> generate_error_messages_of_symbols( CrateCtx &c_ctx, Worker &w_ctx,
                                                             std::vector<SymbolId> &symbols ) {
    std::vector<MessageInfo> messages;
    for ( auto &s : symbols ) {
        messages.push_back( MessageInfo( *c_ctx.symbol_graph[s].original_expr.front(), 1 ) );
    }
    return messages;
}

MirEntryId create_call( CrateCtx &c_ctx, Worker &w_ctx, FunctionImplId calling_function, AstNode &original_expr,
                        MirVarId symbol_var, MirVarId result, ParamContainer parameters ) {
    FunctionImpl *caller = &c_ctx.functions[calling_function];

    // Create call operation
    auto op_id =
        create_operation( c_ctx, w_ctx, calling_function, original_expr, MirEntry::Type::call, result, parameters );
    auto &op = caller->ops[op_id];
    op.symbol = symbol_var;
    caller->vars[op.ret].type = MirVariable::Type::rvalue;

    return op_id;
}

MirVarId create_variable( CrateCtx &c_ctx, Worker &w_ctx, FunctionImplId function, AstNode *original_expr,
                          const String &name ) {
    MirVarId id = c_ctx.functions[function].vars.size();
    c_ctx.functions[function].vars.emplace_back();
    c_ctx.functions[function].vars[id].name = name;
    c_ctx.functions[function].vars[id].original_expr = original_expr;
    c_ctx.curr_vars_stack.back().push_back( id );
    if ( !name.empty() ) {
        c_ctx.curr_name_mapping.back()[name].push_back( id );
    }
    return id;
}

void purge_variable( CrateCtx &c_ctx, Worker &w_ctx, FunctionImplId function, AstNode &original_expr,
                     std::vector<MirVarId> variables ) {
    ParamContainer params;
    params.reserve( variables.size() );
    for ( auto &v : variables ) {
        if ( v != 0 && v != 1 )
            params.push_back( v );

        // Remove from name mapping
        auto &var = c_ctx.functions[function].vars[v];
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

    auto op = MirEntry{ &original_expr, MirEntry::Type::purge, 1, params };
    c_ctx.functions[function].ops.push_back( op );
}

void analyse_function_signature( CrateCtx &c_ctx, Worker &w_ctx, SymbolId function ) {
    auto &symbol = c_ctx.symbol_graph[function];
    if ( function && !symbol.signature_evaluated ) {
        auto &expr = *symbol.original_expr.front();
        if ( expr.type != ExprType::func ) {
            LOG_ERR( "Function to analyse is not a function" );
            return;
        }

        bool signature_evaluated = true;

        // Reset identifier from previous analysis
        symbol.identifier.parameters.clear();

        // Parameters
        if ( expr.named.find( AstChild::parameters ) != expr.named.end() ) {
            auto &paren_expr = expr.named[AstChild::parameters];
            for ( auto &entry : paren_expr.children ) {
                auto &new_parameter = symbol.identifier.parameters.emplace_back();
                auto *parameter_symbol = &entry;

                if ( parameter_symbol->type == ExprType::typed_op ) {
                    parameter_symbol = &entry.named[AstChild::left_expr];
                    auto &type_symbol = entry.named[AstChild::right_expr];

                    if ( type_symbol.type == ExprType::self_type ) {
                        // Self type

                        // Check if "self" is allowed
                        if ( c_ctx.symbol_graph[symbol.parent].type != c_ctx.struct_type ) {
                            // Is not in an impl
                            w_ctx.print_msg<MessageType::err_self_in_free_function>(
                                MessageInfo( *symbol.original_expr.front(), 0, FmtStr::Color::Red ) );
                            signature_evaluated = false;
                        }

                        new_parameter.type = c_ctx.symbol_graph[symbol.parent].value;
                    } else {
                        auto type_sc = type_symbol.get_symbol_chain( c_ctx, w_ctx );

                        // Search in template parameters
                        size_t template_var_index = 0;
                        if ( type_sc->size() == 1 && type_sc->front().template_values.empty() ) {
                            // Single symbol
                            for ( size_t i = 1; i < symbol.template_params.size(); i++ ) {
                                if ( symbol.template_params[i].second == type_sc->front().name ) {
                                    // Symbol matches

                                    if ( symbol.template_params[i].first != c_ctx.type_type ) {
                                        w_ctx.print_msg<MessageType::err_template_parameter_not_type>(
                                            MessageInfo( type_symbol, 0, FmtStr::Color::Red ) );
                                    } else {
                                        template_var_index = i;
                                    }
                                    break;
                                }
                            }
                        }

                        if ( template_var_index != 0 ) {
                            new_parameter.template_type_index = template_var_index;
                        } else {
                            // Search in global symbol tree
                            auto types = find_local_symbol_by_identifier_chain( c_ctx, w_ctx, type_sc );

                            if ( !expect_exactly_one_symbol( c_ctx, w_ctx, types, type_symbol ) )
                                continue;

                            new_parameter.type = c_ctx.symbol_graph[types.front()].value;
                        }
                    }

                    new_parameter.ref = type_symbol.has_prop( ExprProperty::ref );
                    new_parameter.mut = type_symbol.has_prop( ExprProperty::mut );
                } else {
                    signature_evaluated = false;
                }

                if ( parameter_symbol->type == ExprType::self ) {
                    // Self parameter

                    // Check if "self" is allowed
                    if ( c_ctx.symbol_graph[symbol.parent].type != c_ctx.struct_type ) {
                        // Is not in an impl
                        w_ctx.print_msg<MessageType::err_self_in_free_function>(
                            MessageInfo( entry, 0, FmtStr::Color::Red ) );
                        signature_evaluated = false;
                    }

                    // Check if it's the first parameter
                    if ( symbol.identifier.parameters.size() != 1 ) {
                        w_ctx.print_msg<MessageType::err_self_not_first_parameter>(
                            MessageInfo( entry, 0, FmtStr::Color::Red ) );
                        signature_evaluated = false;
                    }

                    new_parameter.type = c_ctx.symbol_graph[symbol.parent].value;
                    new_parameter.name = "self"; // TODO load from prelude
                } else {
                    // Normal parameter
                    auto symbol_chain = parameter_symbol->get_symbol_chain( c_ctx, w_ctx );
                    if ( !expect_unscoped_variable( c_ctx, w_ctx, *symbol_chain, *parameter_symbol ) )
                        continue;
                    new_parameter.name = symbol_chain->front().name;
                }
            }
        }

        // Return value
        if ( expr.named.find( AstChild::return_type ) != expr.named.end() ) {
            auto return_symbol = expr.named[AstChild::return_type];

            if ( return_symbol.type == ExprType::self_type ) {
                // Self type

                // Check if "self" is allowed
                if ( c_ctx.symbol_graph[symbol.parent].type != c_ctx.struct_type ) {
                    // Is not in an impl
                    w_ctx.print_msg<MessageType::err_self_in_free_function>(
                        MessageInfo( *symbol.original_expr.front(), 0, FmtStr::Color::Red ) );
                    signature_evaluated = false;
                }

                symbol.identifier.eval_type.type = c_ctx.symbol_graph[symbol.parent].value;
            } else {
                auto type_sc = return_symbol.get_symbol_chain( c_ctx, w_ctx );

                // Search in template parameters
                size_t template_var_index = 0;
                if ( type_sc->size() == 1 && type_sc->front().template_values.empty() ) {
                    // Single symbol
                    for ( size_t i = 1; i < symbol.template_params.size(); i++ ) {
                        if ( symbol.template_params[i].second == type_sc->front().name ) {
                            // Symbol matches
                            if ( symbol.template_params[i].first != c_ctx.type_type ) {
                                w_ctx.print_msg<MessageType::err_template_parameter_not_type>(
                                    MessageInfo( return_symbol, 0, FmtStr::Color::Red ) );
                            } else {
                                template_var_index = i;
                            }
                            break;
                        }
                    }
                }

                if ( template_var_index != 0 ) {
                    symbol.identifier.eval_type.template_type_index = template_var_index;
                } else {
                    // Search in global symbol tree
                    auto return_symbols = find_local_symbol_by_identifier_chain( c_ctx, w_ctx, type_sc );

                    if ( !expect_exactly_one_symbol( c_ctx, w_ctx, return_symbols, return_symbol ) )
                        return;

                    symbol.identifier.eval_type.type = c_ctx.symbol_graph[return_symbols.front()].value;
                }
            }

            symbol.identifier.eval_type.ref = return_symbol.has_prop( ExprProperty::ref );
            symbol.identifier.eval_type.mut = return_symbol.has_prop( ExprProperty::mut );
        } else {
            signature_evaluated = false;
        }

        symbol.signature_evaluated = signature_evaluated; // Set not until here
    }
}

void generate_mir_function_impl( CrateCtx &c_ctx, Worker &w_ctx, SymbolId symbol_id ) {
    auto &symbol = c_ctx.symbol_graph[symbol_id];

    if ( symbol.original_expr.empty() )
        return; // this is a virtual symbol (i. e. declared through the prelude, but never defined)

    auto expr = *symbol.original_expr.front();

    if ( symbol.value_evaluated || symbol.signature_evaluation_ongoing )
        return; // Function has already been created (e. g. with a call look ahead)

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

    c_ctx.curr_vars_stack.clear();
    c_ctx.curr_vars_stack.emplace_back();
    c_ctx.curr_name_mapping.clear();
    c_ctx.curr_name_mapping.emplace_back();

    // Create the function
    FunctionImplId func_id = c_ctx.functions.size();
    c_ctx.functions.emplace_back();
    FunctionImpl *function = &c_ctx.functions.back();
    create_variable( c_ctx, w_ctx, func_id, nullptr, "" ); // invalid variable
    create_variable( c_ctx, w_ctx, func_id, nullptr, "" ); // unit return value
    function->vars[1].value_type.add_requirement( c_ctx.unit_type ); // add unit type requirement to unit return value
    analyse_function_signature( c_ctx, w_ctx, symbol_id );
    function->type = symbol.value;
    symbol.signature_evaluation_ongoing = true; // start evaluation

    // Parse parameters
    auto paren_expr = expr.named[AstChild::parameters];
    for ( size_t i = 0; i < paren_expr.children.size(); i++ ) {
        auto &entry_symbol = symbol.identifier.parameters[i];
        MirVarId id = create_variable( c_ctx, w_ctx, func_id, &paren_expr.children[i] );
        function->params.push_back( id );

        function->vars[id].name = entry_symbol.name;
        if ( entry_symbol.type != 0 )
            function->vars[id].value_type.add_requirement( entry_symbol.type );
        function->vars[id].mut = entry_symbol.mut;
        if ( entry_symbol.ref )
            function->vars[id].type = MirVariable::Type::p_ref;
        else
            function->vars[id].type = MirVariable::Type::value;

        c_ctx.curr_name_mapping.back()[function->vars[id].name].push_back( id );
        c_ctx.curr_vars_stack.back().push_back( id );
    }

    // Self parameter
    if ( !paren_expr.children.empty() &&
         ( paren_expr.children.front().type == ExprType::self ||
           ( paren_expr.children.front().type == ExprType::typed_op &&
             paren_expr.children.front().named[AstChild::left_expr].type == ExprType::self ) ) ) {
        c_ctx.curr_self_var = 2; // per convention
        c_ctx.curr_self_type = c_ctx.symbol_graph[symbol.parent].value;
    } else {
        c_ctx.curr_self_var = 0;
        c_ctx.curr_self_type = 0;
    }

    // Parse body
    c_ctx.functions[func_id].ret = expr.children.front().parse_mir( c_ctx, w_ctx, func_id );
    function = &c_ctx.functions[func_id]; // update ref

    // Drop parameters
    bool annotation_is_drop = std::find( symbol.compiler_annotations.begin(), symbol.compiler_annotations.end(),
                                         "drop_handler" ) != symbol.compiler_annotations.end();
    if ( !annotation_is_drop ) {
        for ( auto p_itr = function->params.rbegin(); p_itr != function->params.rend(); p_itr++ ) {
            if ( *p_itr != function->ret && function->vars[*p_itr].type != MirVariable::Type::p_ref )
                purge_variable( c_ctx, w_ctx, func_id, expr, { *p_itr } );
        }
    }

    // Add return operation
    create_operation( c_ctx, w_ctx, func_id, *function->vars[function->ret].original_expr, MirEntry::Type::ret,
                      function->ret, {} );

    // Infer all types and function calls
    infer_operations( c_ctx, w_ctx, func_id );
    function = &c_ctx.functions[func_id]; // update ref

    // Handle drops
    resolve_drops( c_ctx, w_ctx, func_id );

    // Check and set signature
    bool annotation_is_stub = std::find( symbol.compiler_annotations.begin(), symbol.compiler_annotations.end(),
                                         "stub" ) != symbol.compiler_annotations.end();
    auto &identifier = c_ctx.symbol_graph[symbol_id].identifier;
    if ( function->ret == 0 ||
         ( function->vars[function->ret].value_type.is_final() &&
           identifier.eval_type.type != function->vars[function->ret].value_type.get_final_type() ) ) {
        if ( identifier.eval_type.type == 0 && function->ret != 0 ) {
            // Update type
            identifier.eval_type.type = function->vars[function->ret].value_type.get_final_type();
        } else if ( !annotation_is_stub ) {
            w_ctx.print_msg<MessageType::err_type_does_not_match_signature>(
                MessageInfo( function->vars[function->ret].original_expr != nullptr
                                 ? *function->vars[function->ret].original_expr
                                 : *c_ctx.symbol_graph[symbol_id].original_expr.front(),
                             0, FmtStr::Color::Red ) );
        }
    }
    for ( size_t i = 0; i < identifier.parameters.size(); i++ ) {
        if ( function->params[i] == 0 ||
             ( function->vars[function->params[i]].value_type.is_final() &&
               identifier.parameters[i].type != function->vars[function->params[i]].value_type.get_final_type() ) ) {
            if ( identifier.parameters[i].type == 0 && function->params[i] != 0 ) {
                // Update type
                identifier.parameters[i].type = function->vars[function->params[i]].value_type.get_final_type();
            } else if ( !annotation_is_stub ) {
                w_ctx.print_msg<MessageType::err_type_does_not_match_signature>(
                    MessageInfo( function->vars[function->params[i]].original_expr != nullptr
                                     ? *function->vars[function->params[i]].original_expr
                                     : *c_ctx.symbol_graph[symbol_id].original_expr.front(),
                                 0, FmtStr::Color::Red ) );
            }
        }
    }

    // Set evaluation flag
    c_ctx.symbol_graph[symbol_id].value_evaluated = true;
    c_ctx.symbol_graph[symbol_id].signature_evaluated = true; // May already be set earlier
    c_ctx.symbol_graph[symbol_id].signature_evaluation_ongoing = false; // evaluation finished
}

void get_mir( JobsBuilder &jb, UnitCtx &parent_ctx ) {
    jb.add_job<void>( []( Worker &w_ctx ) {
        auto c_ctx = w_ctx.do_query( get_ast )->jobs.back()->to<sptr<CrateCtx>>();

        auto start_time = std::chrono::system_clock::now();

        // Decide which is the first adhoc symbol
        c_ctx->first_adhoc_symbol = c_ctx->symbol_graph.size();

        // Prepare types in structs
        for ( size_t i = 0; i < c_ctx->symbol_graph.size(); i++ ) {
            if ( !c_ctx->symbol_graph[i].original_expr.empty() ) {
                auto expr_type = c_ctx->symbol_graph[i].original_expr.front()->type;
                if ( expr_type == ExprType::structure || expr_type == ExprType::implementation ) {
                    for ( auto &exprs : c_ctx->symbol_graph[i].original_expr ) {
                        exprs->find_types( *c_ctx, w_ctx );
                    }
                }
            }
        }

        // Generate the Mir function bodies
        for ( size_t i = 0; i < c_ctx->symbol_graph.size(); i++ ) {
            if ( c_ctx->symbol_graph[i].original_expr.size() == 1 &&
                 c_ctx->symbol_graph[i].original_expr.front()->type == ExprType::func &&
                 c_ctx->symbol_graph[i].type == c_ctx->fn_type && !c_ctx->symbol_graph[i].proposed ) {
                generate_mir_function_impl( *c_ctx, w_ctx, i );
            }
        }

        // DEBUG print MIR
        log( "MIR FUNCTIONS --" );
        for ( size_t i = 1; i < c_ctx->functions.size(); i++ ) {
            auto &fn = c_ctx->functions[i];
            auto get_var_name = [&fn]( MirVarId id ) -> String {
                if ( id == 0 )
                    return " INVALID_VAR";
                else if ( id == 1 )
                    return " ()";
                String type_str =
                    ( fn.vars[id].type == MirVariable::Type::rvalue
                          ? "r"
                          : fn.vars[id].type == MirVariable::Type::l_ref
                                ? "l"
                                : fn.vars[id].type == MirVariable::Type::p_ref
                                      ? "p"
                                      : fn.vars[id].type == MirVariable::Type::not_dropped
                                            ? "n"
                                            : fn.vars[id].type == MirVariable::Type::label
                                                  ? "b"
                                                  : fn.vars[id].type == MirVariable::Type::symbol
                                                        ? "s"
                                                        : fn.vars[id].type == MirVariable::Type::undecided ? "UNDECIDED"
                                                                                                           : "" );
                return " " + fn.vars[id].name + "%" + type_str + to_string( id );
            };

            log( " fn " + to_string( i ) + " - " +
                 get_full_symbol_name( *c_ctx, w_ctx, c_ctx->type_table[fn.type].symbol ) +
                 ( c_ctx->symbol_graph[c_ctx->type_table[fn.type].symbol].proposed ? " [proposed]" : "" ) );

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
                else if ( op.type == MirEntry::Type::literal )
                    str = "literal";
                else if ( op.type == MirEntry::Type::type )
                    str = "type";
                else if ( op.type == MirEntry::Type::call )
                    str = "call";
                else if ( op.type == MirEntry::Type::bind )
                    str = "bind";
                else if ( op.type == MirEntry::Type::purge )
                    str = "purge";
                else if ( op.type == MirEntry::Type::member )
                    str = "member";
                else if ( op.type == MirEntry::Type::merge )
                    str = "merge";
                else if ( op.type == MirEntry::Type::label )
                    str = "label";
                else if ( op.type == MirEntry::Type::cond_jmp_z )
                    str = "cond_jmp_z";
                else if ( op.type == MirEntry::Type::jmp )
                    str = "jmp";
                else if ( op.type == MirEntry::Type::cast )
                    str = "cast";
                else if ( op.type == MirEntry::Type::ret )
                    str = "ret";
                else
                    str = "UNKNOWN COMMAND";

                if ( op.symbol != 0 )
                    str += get_var_name( op.symbol ) + "\"" +
                           ( fn.vars[op.symbol].symbol_set.empty()
                                 ? "<none>"
                                 : get_full_symbol_name( *c_ctx, w_ctx, fn.vars[op.symbol].symbol_set.front() ) ) +
                           "\"";

                if ( op.intrinsic != MirIntrinsic::none )
                    str += " intrinsic " + to_string( static_cast<u32>( op.intrinsic ) );

                str += get_var_name( op.ret );

                if ( op.type == MirEntry::Type::literal ) {
                    str += " 0x";
                    const u8 *data_ptr = ( op.data.is_inline ? reinterpret_cast<const u8 *>( &op.data.value )
                                                             : &c_ctx->literal_data[op.data.value] );
                    for ( size_t i = 0; i < op.data.size; i++ ) {
                        append_hex_str( data_ptr[i], str );
                    }
                }

                for ( auto &p : op.params ) {
                    str += get_var_name( p );
                }

                log( "  " + str );
            }

            // Variables
            log( "\n  VARS:" );
            for ( size_t i = 1; i < fn.vars.size(); i++ ) {
                String symbols;
                for ( auto s : fn.vars[i].symbol_set ) {
                    symbols += " " + get_full_symbol_name( *c_ctx, w_ctx, s );
                }

                log(
                    "  " + get_var_name( i ) + " :" + ( fn.vars[i].mut ? "mut" : "" ) +
                    ( fn.vars[i].type == MirVariable::Type::l_ref || fn.vars[i].type == MirVariable::Type::p_ref
                          ? "& "
                          : " " ) +
                    get_full_symbol_name(
                        *c_ctx, w_ctx,
                        c_ctx->type_table[fn.vars[i].value_type.is_final() ? fn.vars[i].value_type.get_final_type() : 0]
                            .symbol ) +
                    ( fn.vars[i].symbol_set.empty() ? "" : "symbols" + symbols ) +
                    ( fn.vars[i].ref != 0
                          ? " ->" + get_var_name( fn.vars[i].ref ) + " +" + to_string( fn.vars[i].member_idx )
                          : "" ) +
                    ( fn.vars[i].base_ref != 0 ? " base:" + get_var_name( fn.vars[i].base_ref ) : "" ) );
            }
        }
        log( "----------------" );

        auto duration = std::chrono::system_clock::now() - start_time;
        log( "MIR took " + to_string( duration.count() / 1000000 ) + " milliseconds" );
    } );
}

bool infer_operations( CrateCtx &c_ctx, Worker &w_ctx, FunctionImplId function ) {
    auto *fn = &c_ctx.functions[function];
    for ( auto &op : fn->ops ) {
        // Infer types (don't enforce types when not necessary)
        infer_type( c_ctx, w_ctx, function, op.ret );
        fn = &c_ctx.functions[function]; // update ref
        for ( auto &var : op.params ) {
            infer_type( c_ctx, w_ctx, function, var );
            fn = &c_ctx.functions[function]; // update ref
        }

        // Check if call is unambiguous and finalize it
        if ( op.type == MirEntry::Type::call ) {
            // Enforce the types for a call earlier
            if ( !enforce_type_of_variable( c_ctx, w_ctx, function, op.ret ) )
                return false;
            for ( auto &var : op.params ) {
                if ( !enforce_type_of_variable( c_ctx, w_ctx, function, var ) )
                    return false;
            }

            if ( !infer_function_call( c_ctx, w_ctx, function, op ) )
                return false;
            fn = &c_ctx.functions[function]; // update ref

            auto &symbols = fn->vars[op.symbol].symbol_set;

            if ( op.inference_finished ) {
                // Fulfill function proposals
                if ( c_ctx.symbol_graph[symbols.front()].proposed ) {
                    c_ctx.symbol_graph[symbols.front()].proposed = false;
                }
            } else if ( symbols.empty() ) {
                w_ctx.print_msg<MessageType::err_no_suitable_function>(
                    MessageInfo( *op.original_expr, 0, FmtStr::Color::Red ) );
                return false;
            } else if ( symbols.size() > 1 ) {
                w_ctx.print_msg<MessageType::err_multiple_suitable_functions>(
                    MessageInfo( *op.original_expr, 0, FmtStr::Color::Red ) );
                return false;
            } else {
                LOG_ERR( "call inference error; exactly one symbol but not finished" );
                return false;
            }
        }
    }

    // Infer variables which are not used
    for ( size_t i = 1; i < fn->vars.size(); i++ ) {
        if ( !fn->vars[i].type_inference_finished )
            infer_type( c_ctx, w_ctx, function, i );
    }

    // Enforce the types now
    for ( size_t i = 1; i < fn->vars.size(); i++ ) {
        if ( !enforce_type_of_variable( c_ctx, w_ctx, function, i ) )
            return false;
    }

    return true;
}

bool resolve_drops( CrateCtx &c_ctx, Worker &w_ctx, FunctionImplId function ) {
    auto *fn = &c_ctx.functions[function];

    // New operation container
    std::vector<MirEntry> new_ops;
    new_ops.reserve( fn->ops.size() + fn->vars.size() );

    // Management data
    std::vector<MirVarId> living_vars;
    std::vector<MirEntryId> drops_to_infer;
    std::vector<MirEntryId> drop_positions( fn->vars.size() );
    for ( auto p : fn->params ) {
        living_vars.push_back( p );
    }

    // Helper functions
    auto remove_from_living_vars = [&]( MirVarId var, MirEntryId position ) {
        auto pos = std::find( living_vars.begin(), living_vars.end(), var );
        if ( pos != living_vars.end() )
            living_vars.erase( pos );
        drop_positions[var] = position;
    };
    auto drop_var = [&]( MirVarId var, MirEntryId position ) {
        auto pos = std::find( living_vars.begin(), living_vars.end(), var );
        if ( pos != living_vars.end() ) {
            // Create call var
            auto call_var = create_variable( c_ctx, w_ctx, function, fn->vars[var].original_expr );
            c_ctx.functions[function].vars[call_var].type = MirVariable::Type::symbol;
            c_ctx.functions[function].vars[call_var].value_type.set_final_type( &c_ctx, function, c_ctx.type_type );
            c_ctx.functions[function].vars[call_var].symbol_set = c_ctx.drop_fn;

            new_ops.push_back( MirEntry{ fn->vars[var].original_expr, MirEntry::Type::call, 1, { var }, call_var } );
            drops_to_infer.push_back( new_ops.size() - 1 );

            living_vars.erase( pos );
            drop_positions[var] = position;
        }
    };

    for ( size_t op_i = 0; op_i < fn->ops.size(); op_i++ ) {
        auto &op = fn->ops[op_i];

        // Copy the operation if reasonable
        if ( op.type != MirEntry::Type::nop && op.type != MirEntry::Type::purge )
            new_ops.push_back( op );

        // Manage variables of the operation
        switch ( op.type ) {
        case MirEntry::Type::purge:
            // Handle drops
            for ( size_t i = 0; i < op.params.size(); i++ ) {
                if ( op.params.get_param( i ) > 1 )
                    drop_var( op.params.get_param( i ), op_i );
            }
            break;
        case MirEntry::Type::intrinsic:
        case MirEntry::Type::literal:
        case MirEntry::Type::call:
        case MirEntry::Type::bind:
        case MirEntry::Type::merge:
        case MirEntry::Type::member:
            // Handle parameter life
            if ( op.type == MirEntry::Type::call ) {
                if ( fn->vars[op.symbol].symbol_set.size() != 1 )
                    break;
                auto &symbol = c_ctx.symbol_graph[fn->vars[op.symbol].symbol_set.front()];

                for ( size_t i = 0; i < op.params.size(); i++ ) {
                    if ( op.params.get_param( i ) > 1 ) {
                        // First check if variable is still living
                        if ( std::find( living_vars.begin(), living_vars.end(), op.params.get_param( i ) ) ==
                             living_vars.end() ) {
                            w_ctx.print_msg<MessageType::err_var_not_living>(
                                MessageInfo( *op.original_expr, 0, FmtStr::Color::Red ),
                                { MessageInfo( *fn->ops[drop_positions[op.params.get_param( i )]].original_expr,
                                               1 ) } );
                            continue;
                        }

                        // Handle drop
                        if ( !symbol.identifier.parameters[i].ref ) {
                            // Variable has been moved

                            // Drop referenced vars
                            if ( fn->vars[op.params.get_param( i )].type == MirVariable::Type::l_ref ) {
                                remove_from_living_vars( fn->vars[op.params.get_param( i )].ref, op_i );
                            }

                            remove_from_living_vars( op.params.get_param( i ), op_i );
                        } else if ( fn->vars[op.params.get_param( i )].type == MirVariable::Type::rvalue ) {
                            // Rvalues require an additional drop
                            drop_var( op.params.get_param( i ), op_i );
                        }
                    }
                }
            } else if ( op.type == MirEntry::Type::bind || op.type == MirEntry::Type::merge ) {
                for ( auto &p : op.params ) {
                    if ( p > 1 ) {
                        // Variable has been moved
                        remove_from_living_vars( p, op_i );
                    }
                }
            }

            // Introduce new variables
            if ( op.ret > 1 )
                living_vars.push_back( op.ret );
            break;

        default:
            break;
        }
    }

    // Swap operation containers
    fn->ops.swap( new_ops );

    // Infer drops
    for ( auto v : drops_to_infer )
        if ( !infer_function_call( c_ctx, w_ctx, function, fn->ops[v] ) )
            return false;

    return true;
}

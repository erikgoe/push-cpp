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


MirEntryId create_operation( CrateCtx &c_ctx, Worker &w_ctx, FunctionImplId function, AstNode &original_expr,
                             MirEntry::Type type, MirVarId result, ParamContainer parameters ) {
    MirVarId return_var = result;
    if ( result == 0 ) {
        return_var = create_variable( c_ctx, w_ctx, function, &original_expr, "" );
    }

    // Check if parameters are valid
    /*for ( auto &param : parameters ) {
        bool valid = false;
        for ( auto itr = c_ctx.curr_living_vars.rbegin(); itr != c_ctx.curr_living_vars.rend(); itr++ ) {
            if ( std::find( itr->begin(), itr->end(), param ) != itr->end() ) {
                // Variable is accessible
                valid = true;
                break;
            }
        }
        if ( !valid ) {
            auto original_expr = c_ctx.functions[function].vars[param].original_expr;
            auto drop_expr = c_ctx.functions[function].drop_list[param].second;
            if ( !original_expr ) {
                LOG_ERR( "Unit variable accessed out of its lifetime" );
            } else if ( !drop_expr ) {
                LOG_ERR( "Variable dropped at unknown expr" );
            } else {
                // TODO test this
                w_ctx.print_msg<MessageType::err_var_not_living>( MessageInfo( *original_expr, 0, FmtStr::Color::Red ),
                                                                  { MessageInfo( *drop_expr, 1 ) } );
            }
        }
    }*/

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

    // Infer the function call as good as possible
    /*if ( infer_function_call( c_ctx, w_ctx, calling_function, op ) ) {
        caller = &c_ctx.functions[calling_function]; // update ref

        // Handle parameter remains
        for ( size_t i = 0; i < parameters.size(); i++ ) {
            bool is_ref = c_ctx.symbol_graph[caller->ops[op_id].symbols.front()].identifier.parameters[i].ref;
            // Test if the borrowing behaviour of all possible functions match
            for ( auto &s : caller->ops[op_id].symbols ) {
                if ( is_ref != c_ctx.symbol_graph[s].identifier.parameters[i].ref ) {
                    // Function signatures differ to much
                    w_ctx.print_msg<MessageType::err_multiple_suitable_functions_for_parameter_ref>(
                        MessageInfo( original_expr, 0, FmtStr::Color::Red ),
                        generate_error_messages_of_symbols( c_ctx, w_ctx, caller->ops[op_id].symbols ), i );
                    break;
                    // TODO maybe add an enforce_type_of_variable() enforcement to still be able to select a function
                }
            }

            if ( is_ref ) {
                // Reference parameter expected
                if ( caller->vars[parameters.get_param( i )].type == MirVariable::Type::rvalue ) {
                    drop_variable( c_ctx, w_ctx, calling_function, original_expr, parameters.get_param( i ) );
                }
            } else {
                // Parameter moved

                // Drop referenced vars
                if ( caller->vars[parameters.get_param( i )].type == MirVariable::Type::l_ref ) {
                    remove_from_local_living_vars( c_ctx, w_ctx, calling_function, original_expr,
                                                   caller->vars[parameters.get_param( i )].ref );
                }

                // Remove from living variables
                remove_from_local_living_vars( c_ctx, w_ctx, calling_function, original_expr,
                                               parameters.get_param( i ) );
            }
        }
    }*/

    return op_id;
}

MirVarId create_variable( CrateCtx &c_ctx, Worker &w_ctx, FunctionImplId function, AstNode *original_expr,
                          const String &name ) {
    MirVarId id = c_ctx.functions[function].vars.size();
    c_ctx.functions[function].vars.emplace_back();
    c_ctx.functions[function].vars[id].name = name;
    c_ctx.functions[function].vars[id].original_expr = original_expr;
    c_ctx.curr_living_vars.back().push_back( id );
    if ( !name.empty() ) {
        c_ctx.curr_name_mapping.back()[name].push_back( id );
    }
    return id;
}

void drop_variable( CrateCtx &c_ctx, Worker &w_ctx, FunctionImplId function, AstNode &original_expr,
                    MirVarId variable ) {
    return; // dead code TODO delete this method and all occurrences
    /*if ( variable == 0 || variable == 1 )
        return;

    auto &var = c_ctx.functions[function].vars[variable];

    // Create drop operation
    if ( var.type == MirVariable::Type::value || var.type == MirVariable::Type::rvalue ) {
        auto op = MirEntry{ &original_expr, MirEntry::Type::call, 1, { variable }, c_ctx.drop_fn };
        c_ctx.functions[function].ops.push_back( op );
    }

    // Remove from living variables
    remove_from_local_living_vars( c_ctx, w_ctx, function, original_expr, variable );*/
}

void remove_from_local_living_vars( CrateCtx &c_ctx, Worker &w_ctx, FunctionImplId function, AstNode &original_expr,
                                    MirVarId variable ) {
    if ( variable == 0 || variable == 1 )
        return;

    auto &var = c_ctx.functions[function].vars[variable];

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

    // Add to drop list
    if ( c_ctx.functions[function].drop_list.size() <= variable )
        c_ctx.functions[function].drop_list.resize( variable + 1 );
    if ( c_ctx.functions[function].drop_list[variable].second == nullptr )
        c_ctx.functions[function].drop_list[variable] =
            std::make_pair( c_ctx.functions[function].vars[variable].name, &original_expr );
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

// Creates a function from a FuncExpr specified by @param symbolId
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

    c_ctx.curr_living_vars.clear();
    c_ctx.curr_living_vars.emplace_back();
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
        c_ctx.curr_living_vars.back().push_back( id );
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
            drop_variable( c_ctx, w_ctx, func_id, expr, *p_itr );
        }
    }

    // Infer all types and function calls (if not already)
    for ( auto &op : function->ops ) {
        if ( function->vars[op.ret].type != MirVariable::Type::symbol ) {
            infer_type( c_ctx, w_ctx, func_id, op.ret );
            enforce_type_of_variable( c_ctx, w_ctx, func_id, op.ret );
            function = &c_ctx.functions[func_id]; // update ref
        }
        for ( auto &var : op.params ) {
            if ( function->vars[var].type != MirVariable::Type::symbol ) {
                infer_type( c_ctx, w_ctx, func_id, var );
                enforce_type_of_variable( c_ctx, w_ctx, func_id, var );
                function = &c_ctx.functions[func_id]; // update ref
            }
        }
        if ( op.type == MirEntry::Type::call ) {
            infer_function_call( c_ctx, w_ctx, func_id, op );
            function = &c_ctx.functions[func_id]; // update ref

            auto &symbols = function->vars[op.symbol].symbol_set;

            if ( op.inference_finished ) {
                // Fulfill function proposals
                if ( c_ctx.symbol_graph[symbols.front()].proposed ) {
                    c_ctx.symbol_graph[symbols.front()].proposed = false;
                }
            } else if ( symbols.empty() ) {
                w_ctx.print_msg<MessageType::err_no_suitable_function>(
                    MessageInfo( *op.original_expr, 0, FmtStr::Color::Red ) );
            } else if ( symbols.size() > 1 ) {
                w_ctx.print_msg<MessageType::err_multiple_suitable_functions>(
                    MessageInfo( *op.original_expr, 0, FmtStr::Color::Red ) );
            }
        }
    }

    // TODO handle drops

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
                                                  : fn.vars[id].type == MirVariable::Type::symbol ? "s" : "" );
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
                else if ( op.type == MirEntry::Type::symbol )
                    str = "symbol";
                else
                    str = "UNKNOWN COMMAND";

                if ( op.symbol != 0 )
                    str += " s" + to_string( op.symbol );

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

            // Return value
            log( "  ret" + get_var_name( fn.ret ) );

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

// Checks if a type implements a trait (or is the trait)
bool type_has_trait( CrateCtx &c_ctx, Worker &w_ctx, TypeId type, TypeId trait ) {
    if ( type == trait ) {
        return true;
    } else {
        for ( auto &st : c_ctx.type_table[type].supertypes ) {
            if ( type_has_trait( c_ctx, w_ctx, st, trait ) )
                return true;
        }
        return false;
    }
}

/*
// Creates the set intersection of all types of the @param types and find the widest matching trait. This _is_ a
// projection! i. e. f^2==f
std::vector<TypeId> find_widest_common_traits( CrateCtx &c_ctx, Worker &w_ctx, std::vector<TypeId> types ) {
    std::vector<TypeId> typeset;
    auto common_types = find_common_types( c_ctx, w_ctx, types );

    // Select the supertypes whose subtypes match exactly common_types
    for ( auto t : common_types ) {
        for ( auto supertype : c_ctx.type_table[t].supertypes ) {
            if ( common_types.size() == c_ctx.type_table[supertype].subtypes.size() ) {
                auto &subtypes = c_ctx.type_table[supertype].subtypes;
                std::sort( subtypes.begin(), subtypes.end() );
                subtypes.erase( std::unique( subtypes.begin(), subtypes.end() ), subtypes.end() );
                if ( subtypes == common_types ) {
                    // sets match
                    typeset.push_back( supertype );
                }
            }
        }
    }

    // Sort and filter set
    std::sort( typeset.begin(), typeset.end() );
    typeset.erase( std::unique( typeset.begin(), typeset.end() ), typeset.end() );

    return typeset;
}*/

// Creates the set intersection of all subtypes of the @param types. This is not a projection! i. e. f^2!=f
std::vector<TypeId> find_common_types( CrateCtx &c_ctx, Worker &w_ctx, std::vector<TypeId> types ) {
    std::vector<TypeId> typeset;
    if ( types.empty() )
        return typeset;

    typeset.push_back( types.front() );
    typeset.insert( typeset.end(), c_ctx.type_table[types.front()].subtypes.begin(),
                    c_ctx.type_table[types.front()].subtypes.end() );

    // TODO maybe implement a faster (linear instead of quadratic) intersection algorithm, based on the fact that the
    // sets are ordered.

    // Create set intersection of types
    for ( auto t_itr = types.begin() + 1; t_itr != types.end(); t_itr++ ) {
        typeset.erase( std::remove_if( typeset.begin(), typeset.end(),
                                       [&]( TypeId t ) { return !type_has_trait( c_ctx, w_ctx, t, *t_itr ); } ),
                       typeset.end() );
    }

    // Remove all traits (only types should be returned)
    typeset.erase( std::remove_if( typeset.begin(), typeset.end(),
                                   [&]( TypeId t ) {
                                       auto symbol_type = c_ctx.symbol_graph[c_ctx.type_table[t].symbol].type;
                                       return symbol_type == c_ctx.trait_type ||
                                              symbol_type == c_ctx.template_trait_type;
                                   } ),
                   typeset.end() );

    // Sort and filter set
    std::sort( typeset.begin(), typeset.end() );
    typeset.erase( std::unique( typeset.begin(), typeset.end() ), typeset.end() );

    return typeset;
}

// Tests if the given where clause is fulfilled. @param clause is required for this recursive implementation
bool is_where_clause_fulfilled( CrateCtx &c_ctx, Worker &w_ctx, AstNode *clause, SymbolGraphNode &symbol,
                                SymbolIdentifier &symbol_identifier_back ) {
    // TODO implement constant evaluation, currently only trait bounds are checked

    if ( clause->has_prop( ExprProperty::shortcut_and ) ) {
        return is_where_clause_fulfilled( c_ctx, w_ctx, &clause->named[AstChild::left_expr], symbol,
                                          symbol_identifier_back ) &&
               is_where_clause_fulfilled( c_ctx, w_ctx, &clause->named[AstChild::right_expr], symbol,
                                          symbol_identifier_back );
    } else if ( clause->has_prop( ExprProperty::shortcut_or ) ) {
        return is_where_clause_fulfilled( c_ctx, w_ctx, &clause->named[AstChild::left_expr], symbol,
                                          symbol_identifier_back ) ||
               is_where_clause_fulfilled( c_ctx, w_ctx, &clause->named[AstChild::right_expr], symbol,
                                          symbol_identifier_back );
    } else {
        // Normal clause
        if ( clause->type != ExprType::typed_op ) {
            LOG_WARN( "Arbitrary where clauses are currenlty not implemented. Only trait bounds are supported" );
            return false;
        }

        // Get all the symbol data
        auto lhs_symbol_chain = clause->named[AstChild::left_expr].get_symbol_chain( c_ctx, w_ctx );
        if ( !expect_unscoped_variable( c_ctx, w_ctx, *lhs_symbol_chain, *clause ) )
            return false;
        auto rhs_symbol_chain = clause->named[AstChild::right_expr].get_symbol_chain( c_ctx, w_ctx );
        auto rhs_symbols = find_local_symbol_by_identifier_chain( c_ctx, w_ctx, rhs_symbol_chain );
        if ( !expect_exactly_one_symbol( c_ctx, w_ctx, rhs_symbols, clause->named[AstChild::right_expr] ) )
            return false;
        auto rhs_type = c_ctx.symbol_graph[rhs_symbols.front()].value;

        // Find the types and check them
        auto template_param_itr =
            std::find_if( symbol.template_params.begin(), symbol.template_params.end(),
                          [&]( auto &&pair ) { return pair.second == lhs_symbol_chain->front().name; } );
        if ( template_param_itr != symbol.template_params.end() ) {
            auto opt_type = symbol_identifier_back.template_values[template_param_itr - symbol.template_params.begin()]
                                .second.get<TypeId>();
            if ( !opt_type.has_value() ) {
                w_ctx.print_msg<MessageType::err_template_parameter_not_type>(
                    MessageInfo( *symbol.original_expr.front(), 0, FmtStr::Color::Red ) );
                return false;
            }

            auto &supertypes = c_ctx.type_table[*opt_type.value()].supertypes;
            if ( *opt_type.value() != rhs_type &&
                 std::find( supertypes.begin(), supertypes.end(), rhs_type ) == supertypes.end() )
                return false; // type does fulfill requirement
        } else {
            LOG_ERR( "Symbol in where clause is not a template argument" );
            return false;
        }
    }
    return true;
}

bool infer_function_call( CrateCtx &c_ctx, Worker &w_ctx, FunctionImplId function, MirEntry &call_op,
                          std::vector<MirVarId> infer_stack ) {
    if ( c_ctx.functions[function].vars[call_op.symbol].symbol_set.empty() ) {
        w_ctx.print_msg<MessageType::err_no_suitable_function>(
            MessageInfo( *call_op.original_expr, 0, FmtStr::Color::Red ) );
        return false;
    }

    // First infer the parameter and return types
    infer_type( c_ctx, w_ctx, function, call_op.ret, infer_stack );
    for ( auto &p : call_op.params ) {
        infer_type( c_ctx, w_ctx, function, p, infer_stack );
    }

    auto &fn = c_ctx.functions[function];
    auto &symbols = fn.vars[call_op.symbol].symbol_set;
    auto &template_args = fn.vars[call_op.symbol].template_args;

    if ( call_op.inference_finished )
        return true; // don't repeat the inference (which could lead to false errors)

    // Ignore adhoc types
    symbols.erase(
        std::remove_if( symbols.begin(), symbols.end(), [&]( auto &&s ) { return s >= c_ctx.first_adhoc_symbol; } ),
        symbols.end() );

    // Decide which function to call
    std::vector<SymbolId> selected;
    for ( auto s_itr = symbols.begin(); s_itr != symbols.end(); s_itr++ ) {
        auto &s = *s_itr;
        bool param_matches = false, fn_matches = true;

        // Handle template functions
        if ( c_ctx.symbol_graph[s].type == c_ctx.template_fn_type ) {
            // It's a template
            analyse_function_signature( c_ctx, w_ctx, s );
            if ( !c_ctx.symbol_graph[s].signature_evaluated ) {
                // TODO implement inference of template function parameter types. Unspecified paremters must be equally
                // typed for all template instances! Otherwise they would introduce new "invisible" template parameters
                w_ctx.print_msg<MessageType::err_template_signature_incomplete>(
                    MessageInfo( *call_op.original_expr, 0, FmtStr::Color::Red ) );
                return false;
            }

            if ( template_args.size() > c_ctx.symbol_graph[s].template_params.size() )
                fn_matches = false;
        } else {
            // Not a template

            if ( !template_args.empty() ) {
                fn_matches = false;
            } else {
                // First evaluate function signature if necessary
                if ( !c_ctx.symbol_graph[s].signature_evaluated ) {
                    if ( !c_ctx.symbol_graph[s].signature_evaluation_ongoing ) {
                        // TODO fix this better
                        // Save context to restore
                        auto tmp_curr_living_vars = c_ctx.curr_living_vars;
                        auto tmp_curr_name_mapping = c_ctx.curr_name_mapping;
                        auto tmp_curr_self_var = c_ctx.curr_self_var;
                        auto tmp_curr_self_type = c_ctx.curr_self_type;

                        generate_mir_function_impl( c_ctx, w_ctx, s );

                        // Restore context
                        c_ctx.curr_living_vars = tmp_curr_living_vars;
                        c_ctx.curr_name_mapping = tmp_curr_name_mapping;
                        c_ctx.curr_self_var = tmp_curr_self_var;
                        c_ctx.curr_self_type = tmp_curr_self_type;
                    }
                }
            }
        }

        // Template stuff
        auto new_symbol_chain = get_symbol_chain_from_symbol( c_ctx, w_ctx, s );
        auto &sc_back = new_symbol_chain->back();
        sc_back.template_values.resize( c_ctx.symbol_graph[s].template_params.size() );
        std::vector<std::vector<TypeId>> template_values;
        template_values.resize( c_ctx.symbol_graph[s].template_params.size() );

        // Update references (to prevent iterator invalidation)
        auto &fn = c_ctx.functions[function];
        auto &symbols = c_ctx.functions[function].vars[call_op.symbol].symbol_set;
        auto &template_args = c_ctx.functions[function].vars[call_op.symbol].template_args;

        // Check return type
        if ( fn_matches ) {
            if ( c_ctx.symbol_graph[s].identifier.eval_type.template_type_index != 0 ) {
                // Template type

                if ( c_ctx.symbol_graph[s].type != c_ctx.template_fn_type )
                    LOG_ERR( "Non-template with template return type" );

                auto &type_requirements = fn.vars[call_op.ret].value_type.get_all_requirements( &c_ctx, function );
                template_values[c_ctx.symbol_graph[s].identifier.eval_type.template_type_index].insert(
                    template_values[c_ctx.symbol_graph[s].identifier.eval_type.template_type_index].begin(),
                    type_requirements.begin(), type_requirements.end() );
            } else {
                // Normal type
                for ( auto &requirement : fn.vars[call_op.ret].value_type.get_all_requirements( &c_ctx, function ) ) {
                    if ( type_has_trait( c_ctx, w_ctx, requirement,
                                         c_ctx.symbol_graph[s].identifier.eval_type.type ) ) {
                        param_matches = true;
                        break;
                    }
                }
                if ( !param_matches && fn.vars[call_op.ret].value_type.has_any_requirements() )
                    fn_matches = false; // contains no matching type
            }
        }

        // Check parameters
        if ( call_op.params.size() != c_ctx.symbol_graph[s].identifier.parameters.size() )
            fn_matches = false;

        if ( fn_matches ) {
            // Generate permutation
            std::vector<size_t> parameter_permutation;
            std::vector<String> parameter_names;
            parameter_names.reserve( c_ctx.symbol_graph[s].identifier.parameters.size() );
            for ( auto &p : c_ctx.symbol_graph[s].identifier.parameters )
                parameter_names.push_back( p.name );
            if ( !call_op.params.get_param_permutation( parameter_names, parameter_permutation ) )
                fn_matches = false;

            for ( size_t i = 0; i < c_ctx.symbol_graph[s].identifier.parameters.size() && fn_matches; i++ ) {
                if ( c_ctx.symbol_graph[s].identifier.parameters[i].template_type_index != 0 ) {
                    // Template type

                    if ( c_ctx.symbol_graph[s].type != c_ctx.template_fn_type )
                        LOG_ERR( "Non-template with template parameters" );

                    auto &type_requirements =
                        fn.vars[call_op.params.get_param( parameter_permutation[i] )].value_type.get_all_requirements(
                            &c_ctx, function );
                    template_values[c_ctx.symbol_graph[s].identifier.parameters[i].template_type_index].insert(
                        template_values[c_ctx.symbol_graph[s].identifier.parameters[i].template_type_index].begin(),
                        type_requirements.begin(), type_requirements.end() );
                } else {
                    // Normal type
                    param_matches = false;
                    for ( auto &requirement :
                          fn.vars[call_op.params.get_param( parameter_permutation[i] )].value_type.get_all_requirements(
                              &c_ctx, function ) ) {
                        if ( type_has_trait( c_ctx, w_ctx, c_ctx.symbol_graph[s].identifier.parameters[i].type,
                                             requirement ) ) {
                            param_matches = true;
                            break;
                        }
                    }
                    if ( !param_matches && fn.vars[call_op.params.get_param( parameter_permutation[i] )]
                                               .value_type.has_any_requirements() )
                        fn_matches = false; // contains no matching type
                }
            }
        }

        // Final symbol checks
        if ( c_ctx.symbol_graph[s].type == c_ctx.template_fn_type ) {
            // Template

            // Generate permutation
            std::vector<size_t> template_args_permutation;
            std::vector<String> template_arg_names;
            template_arg_names.reserve( c_ctx.symbol_graph[s].template_params.size() );
            for ( auto &p : c_ctx.symbol_graph[s].template_params )
                template_arg_names.push_back( p.second );
            if ( !template_args.get_param_permutation( template_arg_names, template_args_permutation, true ) )
                fn_matches = false;
            template_args_permutation[0] =
                template_args.INVALID_POSITION_VAL; // First entry is invalid (unspecified parameter)

            // Combine all parameters
            for ( size_t i = 1; i < template_values.size() && fn_matches; i++ ) {
                auto common_types = find_common_types( c_ctx, w_ctx, template_values[i] );

                // Explict parameters
                if ( template_args.get_param( template_args_permutation[i], true ) != 0 &&
                     enforce_type_of_variable( c_ctx, w_ctx, function,
                                               template_args.get_param( template_args_permutation[i], true ) ) ) {
                    auto &v =
                        c_ctx.functions[function].vars[template_args.get_param( template_args_permutation[i], true )];
                    if ( v.type == MirVariable::Type::symbol ) {
                        if ( std::find( common_types.begin(), common_types.end(), v.value_type.get_final_type() ) !=
                             common_types.end() ) {
                            // Explicit type is possible
                            common_types.clear();
                            common_types.push_back( v.value_type.get_final_type() );
                        } else {
                            fn_matches = false;
                        }
                    } else {
                        // TODO constant evaluation
                        common_types = v.value_type.get_all_requirements( &c_ctx, function );
                    }
                }

                auto chosen_type = choose_final_type( c_ctx, w_ctx, common_types );
                if ( chosen_type == 0 ) {
                    fn_matches = false;
                } else {
                    sc_back.template_values[i] = std::make_pair( c_ctx.type_type, ConstValue( chosen_type ) );
                }
            }

            // Check the where clause
            if ( fn_matches && c_ctx.symbol_graph[s].where_clause &&
                 !is_where_clause_fulfilled( c_ctx, w_ctx, c_ctx.symbol_graph[s].where_clause, c_ctx.symbol_graph[s],
                                             sc_back ) )
                fn_matches = false;

            // Check if function is already instantiated
            if ( fn_matches ) {
                auto possible_equal_symbols = find_local_symbol_by_identifier_chain( c_ctx, w_ctx, new_symbol_chain );
                if ( !possible_equal_symbols.empty() ) {
                    fn_matches = false; // already instantiated (don't use the current one)

                    // Should only be one symbol
                    if ( possible_equal_symbols.size() != 1 )
                        LOG_ERR( "Multiple possible symbols for a template instantiation found" );

                    selected.push_back( possible_equal_symbols.front() );
                }
            }

            if ( fn_matches ) {
                // Instantiate function
                auto new_symbol = instantiate_template( c_ctx, w_ctx, s, sc_back.template_values );

                selected.push_back( new_symbol );
            }
        } else {
            // Not a template
            if ( fn_matches ) {
                selected.push_back( s );
            }
        }
    }

    {
        // Check if some symbols where found
        auto &symbols = c_ctx.functions[function].vars[call_op.symbol].symbol_set; // update ref
        symbols = selected; // remove unsuitable symbols
        if ( selected.size() == 1 ) {
            call_op.inference_finished = true;

            // Resolve permutation
            std::vector<String> parameter_names;
            parameter_names.reserve( c_ctx.symbol_graph[symbols.front()].identifier.parameters.size() );
            for ( auto &p : c_ctx.symbol_graph[symbols.front()].identifier.parameters )
                parameter_names.push_back( p.name );
            call_op.params.apply_param_permutation( parameter_names );
        }
    }
    return true;
}

bool infer_type( CrateCtx &c_ctx, Worker &w_ctx, FunctionImplId function, MirVarId var,
                 std::vector<MirVarId> infer_stack ) {
    if ( var == 0 )
        return false; // invalid variable
    else if ( var == 1 )
        return true; // unit type is already well-defined

    if ( std::find( infer_stack.begin(), infer_stack.end(), var ) != infer_stack.end() )
        return false; // Variable is currently inferred (infer cycle)

    auto *fn = &c_ctx.functions[function];

    if ( fn->vars[var].type == MirVariable::Type::label )
        return true; // Skip typeless variables (labels)

    infer_stack.push_back( var );

    for ( size_t i = fn->vars[var].next_type_check_position; i < fn->ops.size(); i++ ) {
        auto &op = fn->ops[i];

        if ( op.ret == var ) {
            switch ( op.type ) {
            case MirEntry::Type::call:
                if ( infer_function_call( c_ctx, w_ctx, function, op, infer_stack ) ) {
                    fn = &c_ctx.functions[function]; // update ref
                    std::vector<TypeId> types;
                    types.reserve( fn->vars[op.symbol].symbol_set.size() );
                    for ( auto &s : fn->vars[op.symbol].symbol_set ) {
                        types.push_back( c_ctx.symbol_graph[s].identifier.eval_type.type );
                    }
                    auto typeset = find_common_types( c_ctx, w_ctx, types );

                    // We can only add a requirement, if the type/trait is unambiguous
                    if ( typeset.size() == 1 ) {
                        fn->vars[var].value_type.add_requirement( typeset.front() );
                    }
                }
                fn = &c_ctx.functions[function]; // update ref
                break;
            case MirEntry::Type::bind:
                // Pass type requirements. Bind contains exactly one parameter
                infer_type( c_ctx, w_ctx, function, op.params.get_param( 0 ), infer_stack );
                fn->vars[var].value_type.bind_variable( &c_ctx, function, op.params.get_param( 0 ), var );
                break;
            case MirEntry::Type::inv:
                // fn->vars[var].value_type.add_requirement( c_ctx.boolean_type ); // TODO
                break;
            case MirEntry::Type::cast:
                if ( fn->vars[op.symbol].symbol_set.size() == 1 )
                    fn->vars[var].value_type.add_requirement(
                        c_ctx.symbol_graph[fn->vars[op.symbol].symbol_set.front()].value );
                break;
            default:
                break; // does not change any types
            }
        } else if ( op.params.find( var ) != op.params.end() ) {
            switch ( op.type ) {
            case MirEntry::Type::call:
                if ( infer_function_call( c_ctx, w_ctx, function, op, infer_stack ) ) {
                    fn = &c_ctx.functions[function]; // update ref
                    std::vector<TypeId> types;
                    types.reserve( fn->vars[op.symbol].symbol_set.size() );
                    for ( auto &s : fn->vars[op.symbol].symbol_set ) {
                        types.push_back( c_ctx.symbol_graph[s]
                                             .identifier.parameters[op.params.find( var ) - op.params.begin()]
                                             .type );
                    }
                    auto typeset = find_common_types( c_ctx, w_ctx, types );

                    // We can only add a requirement, if the type/trait is unambiguous
                    if ( typeset.size() == 1 ) {
                        fn->vars[var].value_type.add_requirement( typeset.front() );
                    }
                }
                fn = &c_ctx.functions[function]; // update ref
                break;
            case MirEntry::Type::bind:
                // The return variable will create the binding
                infer_type( c_ctx, w_ctx, function, op.ret, infer_stack );
                break;
            case MirEntry::Type::cond_jmp_z:
                // fn->vars[var].value_type.add_requirement(c_ctx.boolean_type); // TODO
                break;
            case MirEntry::Type::inv:
                // fn->vars[var].value_type.add_requirement(c_ctx.boolean_type); // TODO
                break;
            default:
                break; // does not change any types
            }
        }
    }
    fn->vars[var].next_type_check_position = fn->ops.size(); // optimizes the iterative type inference process

    infer_stack.pop_back();

    return true;
}

TypeId choose_final_type( CrateCtx &c_ctx, Worker &w_ctx, std::vector<TypeId> types ) {
    if ( types.empty() )
        return 0;

    // TODO select the best fit (with smallest bounds; e. g. prefer u32 over u64)
    // DEBUG this implementation is only for testing
    return types.front();
}

bool enforce_type_of_variable( CrateCtx &c_ctx, Worker &w_ctx, FunctionImplId function, MirVarId var ) {
    auto &fn = c_ctx.functions[function];

    if ( var == 0 )
        return false; // invalid variable
    else if ( var == 1 )
        return true; // unit type is already well-defined

    if ( fn.vars[var].type == MirVariable::Type::label )
        return true; // Skip typeless variables (labels)

    if ( fn.vars[var].value_type.is_final() )
        return true; // Final type was already specified

    if ( fn.vars[var].value_type.has_unfinalized_requirements() ) {
        auto typeset =
            find_common_types( c_ctx, w_ctx, fn.vars[var].value_type.get_all_requirements( &c_ctx, function ) );
        typeset.erase(
            std::remove_if(
                typeset.begin(), typeset.end(),
                [&]( TypeId t ) { return c_ctx.symbol_graph[c_ctx.type_table[t].symbol].type != c_ctx.struct_type; } ),
            typeset.end() ); // TODO trait object should also be possible

        if ( typeset.empty() ) {
            // Found requirements, but they cannot be fulfilled
            w_ctx.print_msg<MessageType::err_no_suitable_type_found>(
                MessageInfo( *fn.vars[var].original_expr, 0, FmtStr::Color::Red ) );
            return false;
        }

        fn.vars[var].value_type.set_final_type( choose_final_type( c_ctx, w_ctx, typeset ) );
    } else {
        // Still no suitable type
        w_ctx.print_msg<MessageType::err_no_suitable_type_found>(
            MessageInfo( var == 0 ? *c_ctx.symbol_graph[c_ctx.type_table[fn.type].symbol].original_expr.front()
                                  : *fn.vars[var].original_expr,
                         0, FmtStr::Color::Red ) );
        return false;
    }

    // TODO Fail if no type could be selected, because the domain of the possible types can not be compared
    /*std::vector<MessageInfo> occurrences;
    occurrences.resize( typeset.size() );
    for ( auto &t : typeset ) {
        occurrences.push_back(
            MessageInfo( *c_ctx.symbol_graph[c_ctx.type_table[t].symbol].original_expr.front(), 1 ) );
    }
    w_ctx.print_msg<MessageType::err_multiple_suitable_types_found>(
        MessageInfo( *fn.vars[var].original_expr, 0, FmtStr::Color::Red ), occurrences );
    return false;*/

    return true;
}

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
#include "libpushc/TypeInference.h"
#include "libpushc/MirTranslation.h"
#include "libpushc/Expression.h"
#include "libpushc/SymbolUtil.h"

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
                        auto tmp_curr_living_vars = c_ctx.curr_vars_stack;
                        auto tmp_curr_name_mapping = c_ctx.curr_name_mapping;
                        auto tmp_curr_self_var = c_ctx.curr_self_var;
                        auto tmp_curr_self_type = c_ctx.curr_self_type;

                        generate_mir_function_impl( c_ctx, w_ctx, s );

                        // Restore context
                        c_ctx.curr_vars_stack = tmp_curr_living_vars;
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
                if ( !param_matches && fn.vars[call_op.ret].value_type.has_any_requirements( &c_ctx, function ) )
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
                    if ( !param_matches &&
                         fn.vars[call_op.params.get_param( parameter_permutation[i] )].value_type.has_any_requirements(
                             &c_ctx, function ) )
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
                        if ( !expect_exactly_one_symbol( c_ctx, w_ctx, v.symbol_set, *v.original_expr ) ) {
                            fn_matches = false;
                        } else {
                            TypeId symbol_type = c_ctx.symbol_graph[v.symbol_set.front()].value;
                            if ( std::find( common_types.begin(), common_types.end(), symbol_type ) !=
                                 common_types.end() ) {
                                // Explicit type is possible
                                common_types.clear();
                                common_types.push_back( symbol_type );
                            } else {
                                fn_matches = false;
                            }
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

// Infers if the operation is a attribute access or a function call
bool resolve_member_access( CrateCtx &c_ctx, Worker &w_ctx, FunctionImplId function, MirEntry &op ) {
    if ( !enforce_type_of_variable( c_ctx, w_ctx, function, op.params.get_param( 0 ) ) )
        return false;

    auto *fn = &c_ctx.functions[function];
    auto &base_type = c_ctx.type_table[fn->vars[op.params.get_param( 0 )].value_type.get_final_type()];

    // Find Attributes
    auto attrs = find_member_symbol_by_identifier( c_ctx, w_ctx, fn->vars[op.ret].member_identifier, base_type.symbol );

    // Find methods
    std::vector<SymbolId> methods;
    for ( auto &node : c_ctx.symbol_graph[base_type.symbol].sub_nodes ) {
        if ( symbol_identifier_base_matches( fn->vars[op.ret].member_identifier, c_ctx.symbol_graph[node].identifier ) )
            methods.push_back( node );
    }

    if ( attrs.empty() && methods.empty() ) {
        w_ctx.print_msg<MessageType::err_symbol_not_found>(
            MessageInfo( *fn->vars[op.ret].original_expr, 0, FmtStr::Color::Red ) );
        return false;
    } else if ( attrs.size() > 1 ) { // methods may be overloaded, just check attributes
        std::vector<MessageInfo> notes;
        for ( auto &attr : attrs ) {
            if ( !c_ctx.symbol_graph[attr].original_expr.empty() )
                notes.push_back( MessageInfo( *c_ctx.symbol_graph[attr].original_expr.front(), 1 ) );
        }
        w_ctx.print_msg<MessageType::err_member_symbol_is_ambiguous>(
            MessageInfo( *fn->vars[op.ret].original_expr, 0, FmtStr::Color::Red ), notes );
        return false;
    }

    // Update operation
    auto &result_var = fn->vars[op.ret];
    if ( !attrs.empty() ) {
        // Access attribute
        auto &base_var = fn->vars[op.params.get_param( 0 )];
        fn->vars[op.ret].member_idx = attrs.front();
        fn->vars[op.ret].type = MirVariable::Type::l_ref;
        if ( base_var.type == MirVariable::Type::l_ref ) {
            // Pass reference (never reference a l_ref)
            result_var.ref = base_var.ref;
            result_var.member_idx += base_var.member_idx;
        } else {
            result_var.ref = op.params.get_param( 0 );
        }
        result_var.mut = base_var.mut;
        result_var.value_type.add_requirement( base_type.members[attrs.front()].value );
    } else {
        // Access method

        // First check if it's a method and not a function
        methods.erase(
            std::remove_if( methods.begin(), methods.end(),
                            [&]( SymbolId m ) {
                                auto &identifier = c_ctx.symbol_graph[m].identifier;
                                return ( identifier.parameters.empty() ||
                                         c_ctx.symbol_graph[c_ctx.symbol_graph[m].parent].type !=
                                             c_ctx.struct_type ); // TODO this check may be weak (rather check
                                                                  // if the parameter is actually "self")
                            } ),
            methods.end() );
        if ( methods.empty() ) {
            w_ctx.print_msg<MessageType::err_method_is_a_free_function>(
                MessageInfo( *result_var.original_expr, 0, FmtStr::Color::Red ),
                { MessageInfo( *c_ctx.symbol_graph[methods.front()].original_expr.front(), 1 ) } );
            return false;
        }

        op.type = MirEntry::Type::nop; // invalidate operation
        result_var.type = MirVariable::Type::symbol;
        result_var.value_type.set_final_type( &c_ctx, function, c_ctx.type_type );
        result_var.base_ref = op.params.get_param( 0 );
        for ( auto &s : methods ) {
            result_var.symbol_set.push_back( s );
        }
    }
    return true;
}

// Selects the appropriate struct type and does member checks
bool infer_struct_type( CrateCtx &c_ctx, Worker &w_ctx, FunctionImplId function, MirEntry &op ) {
    auto &fn = c_ctx.functions[function];
    auto &symbol_var = fn.vars[op.symbol];

    // Remove symbols whose member count or member types don't match
    symbol_var.symbol_set.erase(
        std::remove_if( symbol_var.symbol_set.begin(), symbol_var.symbol_set.end(),
                        [&]( auto &&s ) {
                            auto &members = c_ctx.type_table[c_ctx.symbol_graph[s].value].members;

                            // Check member count
                            if ( members.size() != op.params.size() )
                                return true;

                            // Generate permutation
                            std::vector<size_t> member_permutation;
                            std::vector<String> member_names;
                            member_names.reserve( members.size() );
                            for ( auto &p : members )
                                member_names.push_back( p.identifier.name );
                            if ( !op.params.get_param_permutation( member_names, member_permutation ) )
                                return false;

                            // Check the type of every member
                            for ( size_t i = 0; i < members.size(); i++ ) {
                                for ( auto &requirement : fn.vars[op.params.get_param( member_permutation[i] )]
                                                              .value_type.get_all_requirements( &c_ctx, function ) ) {
                                    if ( !type_has_trait( c_ctx, w_ctx, members[i].value, requirement ) ) {
                                        return true;
                                    }
                                }
                            }

                            return false;
                        } ),
        symbol_var.symbol_set.end() );

    // Select one symbol
    if ( !expect_exactly_one_symbol( c_ctx, w_ctx, symbol_var.symbol_set, *op.original_expr ) )
        return false;

    // Resolve permutation
    auto &final_type = c_ctx.type_table[c_ctx.symbol_graph[symbol_var.symbol_set.front()].value];
    std::vector<String> member_permutation;
    member_permutation.reserve( final_type.members.size() );
    for ( auto &p : final_type.members )
        member_permutation.push_back( p.identifier.name );
    op.params.apply_param_permutation( member_permutation );

    // Add type requirements
    fn.vars[op.ret].value_type.add_requirement( c_ctx.symbol_graph[symbol_var.symbol_set.front()].value );
    for ( size_t i = 0; i < op.params.size(); i++ ) {
        fn.vars[op.params.get_param( i )].value_type.add_requirement( final_type.members[i].value );
    }

    return true;
}

bool infer_type( CrateCtx &c_ctx, Worker &w_ctx, FunctionImplId function, MirVarId var,
                 std::vector<MirVarId> infer_stack ) {
    if ( var == 0 )
        return false; // invalid variable
    else if ( var == 1 )
        return true; // unit type is already well-defined

    auto *fn = &c_ctx.functions[function];

    if ( fn->vars[var].type_inference_finished )
        return true; // type was already inferred

    if ( fn->vars[var].type == MirVariable::Type::label )
        return true; // Skip typeless variables (labels)

    if ( std::find( infer_stack.begin(), infer_stack.end(), var ) != infer_stack.end() )
        return false; // Variable is currently inferred (infer cycle)

    infer_stack.push_back( var );

    for ( size_t i = 0; i < fn->ops.size(); i++ ) {
        auto &op = fn->ops[i];

        if ( op.ret == var ) {
            switch ( op.type ) {
            case MirEntry::Type::type: {
                // Has exactly one parameter (the type)
                auto &type_var = fn->vars[op.params.get_param( 0 )];
                if ( expect_exactly_one_symbol( c_ctx, w_ctx, type_var.symbol_set, *type_var.original_expr ) ) {
                    fn->vars[var].value_type.add_requirement( c_ctx.symbol_graph[type_var.symbol_set.front()].value );
                }
            } break;
            case MirEntry::Type::member:
                if ( fn->vars[var].type == MirVariable::Type::undecided ) {
                    if ( !resolve_member_access( c_ctx, w_ctx, function, op ) )
                        break;
                    fn = &c_ctx.functions[function]; // update ref
                }
                break;
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
                fn = &c_ctx.functions[function]; // update ref
                fn->vars[var].value_type.bind_variable( &c_ctx, function, op.params.get_param( 0 ), var );
                break;
            case MirEntry::Type::merge:
                infer_struct_type( c_ctx, w_ctx, function, op ); // This will also handle parameter types
                break;
            case MirEntry::Type::inv:
                // fn->vars[var].value_type.add_requirement( c_ctx.boolean_type ); // TODO
                break;
            case MirEntry::Type::cast:
                if ( fn->vars[op.symbol].symbol_set.size() == 1 )
                    fn->vars[var].value_type.add_requirement(
                        c_ctx.symbol_graph[fn->vars[op.symbol].symbol_set.front()].value );
                break;
            case MirEntry::Type::ret: {
                auto &fn_symbol = c_ctx.symbol_graph[c_ctx.type_table[fn->type].symbol];
                if ( fn_symbol.identifier.eval_type.type != 0 )
                    fn->vars[var].value_type.add_requirement( fn_symbol.identifier.eval_type.type );
            } break;
            default:
                break; // does not change any types
            }
        } else if ( op.params.find( var ) != op.params.end() ) {
            switch ( op.type ) {
            case MirEntry::Type::type:
                fn->vars[var].value_type.add_requirement( c_ctx.type_type );
                break;
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
            case MirEntry::Type::member:
                // The return variable will handle the member typeing
                infer_type( c_ctx, w_ctx, function, op.ret, infer_stack );
                break;
            case MirEntry::Type::bind:
                // The return variable will create the binding
                infer_type( c_ctx, w_ctx, function, op.ret, infer_stack );
                fn = &c_ctx.functions[function]; // update ref
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

    fn->vars[var].type_inference_finished = true; // inference finished
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

    if ( fn.vars[var].type == MirVariable::Type::label )
        return true; // Skip typeless variables (labels)

    if ( fn.vars[var].value_type.is_final() )
        return true; // Final type was already specified

    if ( fn.vars[var].value_type.has_unfinalized_requirements( &c_ctx, function ) ) {
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

        fn.vars[var].value_type.set_final_type( &c_ctx, function, choose_final_type( c_ctx, w_ctx, typeset ) );
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

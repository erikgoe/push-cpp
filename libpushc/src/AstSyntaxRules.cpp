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
#include "libpushc/AstParser.h"
#include "libpushc/Expression.h"

// Translate a syntax into a syntax rule
void parse_rule( SyntaxRule &sr, LabelMap &lm, Syntax &syntax_list ) {
    sr.expr_list.clear();
    lm.clear();

    size_t ctr = 0;
    for ( auto &expr : syntax_list ) {
        lm[expr.second] = ctr;
        if ( expr.first == "expr" ) {
            sr.expr_list.push_back( AstNode{ ExprType::none, { ExprProperty::operand } } );
        } else if ( expr.first == "symbol" ) {
            sr.expr_list.push_back( AstNode{ ExprType::none, { ExprProperty::symbol } } );
        } else if ( expr.first == "symbol_like" ) {
            sr.expr_list.push_back( AstNode{ ExprType::none, { ExprProperty::symbol_like } } );
        } else if ( expr.first == "completed" ) {
            sr.expr_list.push_back( AstNode{ ExprType::none, { ExprProperty::completed } } );
        } else if ( expr.first == "assignment" ) {
            sr.expr_list.push_back( AstNode{ ExprType::none, { ExprProperty::assignment } } );
        } else if ( expr.first == "implication" ) {
            sr.expr_list.push_back( AstNode{ ExprType::none, { ExprProperty::implication } } );
        } else if ( expr.first == "fn_head" ) {
            sr.expr_list.push_back( AstNode{ ExprType::func_head } );
        } else if ( expr.first == "comma_list" ) {
            sr.expr_list.push_back( AstNode{ ExprType::comma_list } );
        } else if ( expr.first == "unit" ) {
            sr.expr_list.push_back( AstNode{ ExprType::unit } );
        } else if ( expr.first == "term" ) {
            sr.expr_list.push_back( AstNode{ ExprType::term } );
        } else if ( expr.first == "tuple" ) {
            sr.expr_list.push_back( AstNode{ ExprType::tuple } );
        } else if ( expr.first == "integer" ) {
            sr.expr_list.push_back( AstNode{ ExprType::numeric_literal } );
        } else if ( expr.first == "array_spec" ) {
            sr.expr_list.push_back( AstNode{ ExprType::array_specifier } );
        } else {
            // Keyword or operator
            auto node = AstNode{ ExprType::token };
            node.token = Token( Token::Type::op, expr.first, nullptr, 0, 0, 0, "", TokenLevel::normal );
            sr.expr_list.push_back( node );
        }
        ctr++;
    }
}

void load_syntax_rules( Worker &w_ctx, CrateCtx &c_ctx ) {
    auto pc = w_ctx.unit_ctx()->prelude_conf;

    SyntaxRule new_rule;
    LabelMap lm;

    std::map<String, AstChild> ast_child_map = { { "symbol", AstChild::symbol },
                                                 { "symbol_like", AstChild::symbol_like },
                                                 { "struct_symbol", AstChild::struct_symbol },
                                                 { "trait_symbol", AstChild::trait_symbol },
                                                 { "condition", AstChild::cond },
                                                 { "iterator", AstChild::itr },
                                                 { "selector", AstChild::select },
                                                 { "parameters", AstChild::parameters },
                                                 { "return_type", AstChild::return_type },
                                                 { "left", AstChild::left_expr },
                                                 { "right", AstChild::right_expr },
                                                 { "true_expr", AstChild::true_expr },
                                                 { "false_expr", AstChild::false_expr },
                                                 { "base", AstChild::base },
                                                 { "index", AstChild::index },
                                                 { "member", AstChild::member },
                                                 { "from", AstChild::from },
                                                 { "to", AstChild::to } };

    std::map<SyntaxType, ExprType> ast_type_map = {
        { SyntaxType::op, ExprType::op },
        { SyntaxType::scope_access, ExprType::scope_access },
        { SyntaxType::module_spec, ExprType::module },
        { SyntaxType::member_access, ExprType::member_access },
        { SyntaxType::array_access, ExprType::array_access },
        { SyntaxType::func_head, ExprType::func_head },
        { SyntaxType::func_def, ExprType::func },
        { SyntaxType::macro, ExprType::macro_call },
        { SyntaxType::annotation, ExprType::compiler_annotation },
        { SyntaxType::unsafe_block, ExprType::unsafe },
        { SyntaxType::static_statement, ExprType::static_statement },
        { SyntaxType::reference_attr, ExprType::reference },
        { SyntaxType::mutable_attr, ExprType::mutable_attr },
        { SyntaxType::typed, ExprType::typed_op },
        { SyntaxType::type_of, ExprType::typeof_op },
        { SyntaxType::range, ExprType::range },
        { SyntaxType::assignment, ExprType::op },
        { SyntaxType::implication, ExprType::op },
        { SyntaxType::decl_attr, ExprType::declaration },
        { SyntaxType::public_attr, ExprType::public_attr },
        { SyntaxType::comma, ExprType::comma_list },
        { SyntaxType::structure, ExprType::structure },
        { SyntaxType::trait, ExprType::trait },
        { SyntaxType::implementation, ExprType::implementation },
        { SyntaxType::simple_binding, ExprType::simple_bind },
        { SyntaxType::alias_binding, ExprType::alias_bind },
        { SyntaxType::if_cond, ExprType::if_cond },
        { SyntaxType::if_else, ExprType::if_else },
        { SyntaxType::pre_cond_loop_continue, ExprType::pre_loop },
        { SyntaxType::pre_cond_loop_abort, ExprType::pre_loop },
        { SyntaxType::post_cond_loop_continue, ExprType::post_loop },
        { SyntaxType::post_cond_loop_abort, ExprType::post_loop },
        { SyntaxType::inf_loop, ExprType::inf_loop },
        { SyntaxType::itr_loop, ExprType::itr_loop },
        { SyntaxType::match, ExprType::match },
        { SyntaxType::template_postfix, ExprType::template_postfix },

    };

    // TODO move to syntax_handler
    auto copy_syntax_properties = []( SyntaxRule &rule, const Operator &op ) {
        rule.precedence = op.precedence;
        rule.ltr = op.ltr;
        rule.ambiguous = op.ambiguous;
        rule.prec_class = op.prec_class;
        rule.prec_bias = op.prec_bias;
    };

    auto syntax_handler = [&]( Operator &op, SyntaxType type ) {
        parse_rule( new_rule, lm, op.syntax );
        copy_syntax_properties( new_rule, op );
        auto ast_type = ast_type_map.at( type );
        new_rule.create = [=]( std::vector<AstNode> &list, Worker &w_ctx ) {
            auto node = AstNode{ ast_type };
            node.generate_new_props();
            node.precedence = new_rule.precedence;
            node.original_list = list;
            for ( auto &mapping : lm ) {
                if ( mapping.first == "child" ) {
                    node.children.push_back( list[mapping.second] );
                } else if ( mapping.first == "head" ) {
                    if ( ast_type == ExprType::func || ast_type == ExprType::compiler_annotation ) {
                        auto head = list[mapping.second];
                        node.named.insert( head.named.begin(), head.named.end() );
                    } else {
                        node.children.push_back( list[mapping.second] ); // handle as normal child
                    }
                } else if ( mapping.first == "op" ) {
                    node.token = list[mapping.second].token;
                } else if ( mapping.first == "op1" ) {
                    node.token = list[mapping.second].token;
                    node.token.content = list[mapping.second].token.content + list[lm.at( "op2" )].token.content;
                } else if ( !mapping.first.empty() && mapping.first != "op2" && mapping.first != "child" ) {
                    // Special handling
                    if ( ast_type == ExprType::comma_list ) {
                        // Merge multiple comma lists
                        if ( list[mapping.second].type == ExprType::comma_list ) {
                            node.children.insert( node.children.end(), list[mapping.second].children.begin(),
                                                  list[mapping.second].children.end() );
                            // Fix original_list
                            // The erasing should work, because only one element will ever be removed from the list
                            node.original_list.erase( node.original_list.begin() + mapping.second );
                            node.original_list.insert( node.original_list.end(),
                                                       list[mapping.second].original_list.begin(),
                                                       list[mapping.second].original_list.end() );
                        } else {
                            // Ignore the label of the entries in the prelude
                            node.children.push_back( list[mapping.second] );
                        }
                    } else if ( ast_type == ExprType::array_access &&
                                ast_child_map.at( mapping.first ) == AstChild::index ) {
                        // Merge child content
                        // TODO move this logic into basic_semantic_check and first_transformation passes
                        if ( list[mapping.second].children.size() != 1 ) {
                            LOG_ERR( "Array access index contains not exactly one element. Size: " +
                                     to_string( list[mapping.second].children.size() ) );
                        } else {
                            node.named[ast_child_map.at( mapping.first )] = list[mapping.second].children.front();
                        }
                    } else {
                        // Normal named elements
                        node.named[ast_child_map.at( mapping.first )] = list[mapping.second];
                    }
                }
            }

            node.symbol_name = op.fn;
            node.range_type = op.range;
            if ( ast_type == ExprType::pre_loop || ast_type == ExprType::post_loop ) {
                if ( type == SyntaxType::pre_cond_loop_abort || type == SyntaxType::post_cond_loop_abort ) {
                    node.continue_eval = false;
                }
            } else if ( type == SyntaxType::assignment ) {
                node.props.insert( ExprProperty::assignment );
            } else if ( type == SyntaxType::implication ) {
                node.props.insert( ExprProperty::implication );
            }

            return node;
        };
    };

    for ( auto &syntax : pc.syntaxes ) {
        for ( auto &op : syntax.second ) {
            syntax_handler( op, syntax.first );
            c_ctx.rules.push_back( new_rule );
        }
    }

    // Sort rules after precedence
    std::stable_sort( c_ctx.rules.begin(), c_ctx.rules.end(), []( auto &l, auto &r ) {
        return l.prec_bias > r.prec_bias || ( l.prec_bias == r.prec_bias && l.precedence > r.precedence );
    } );
}

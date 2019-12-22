// Copyright 2019 Erik Götzfried
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
            sr.expr_list.push_back( make_shared<Expr>() );
        } else if ( expr.first == "identifier" ) {
            sr.expr_list.push_back( make_shared<SymbolExpr>() );
        } else if ( expr.first == "block" ) {
            sr.expr_list.push_back( make_shared<BlockExpr>() );
        } else if ( expr.first == "completed" ) {
            sr.expr_list.push_back( make_shared<CompletedExpr>() );
        } else if ( expr.first == "tuple" ) {
            sr.expr_list.push_back( make_shared<TupleExpr>() );
        } else if ( expr.first == "fn_head" ) {
            sr.expr_list.push_back( make_shared<FuncCallExpr>() );
        } else if ( expr.first == "integer" ) {
            sr.expr_list.push_back( make_shared<BasicBlobLiteralExpr>() );
        } else if ( expr.first == "if_expr" ) {
            sr.expr_list.push_back( make_shared<IfExpr>() );
        } else if ( expr.first == "array_spec" ) {
            sr.expr_list.push_back( make_shared<ArraySpecifierExpr>() );
        } else {
            // Keyword or operator
            sr.expr_list.push_back( make_shared<TokenExpr>(
                Token( Token::Type::op, expr.first, nullptr, 0, 0, 0, "", TokenLevel::normal ) ) );
        }
        ctr++;
    }
}

void load_syntax_rules( Worker &w_ctx, AstCtx &a_ctx ) {
    auto pc = w_ctx.unit_ctx()->prelude_conf;

    SyntaxRule new_rule;
    LabelMap lm;

    // Special statements
    for ( auto &sb : pc.simple_bindings ) {
        parse_rule( new_rule, lm, sb.syntax );
        new_rule.precedence = sb.precedence;
        new_rule.ltr = sb.ltr;
        new_rule.create = [=]( auto &list, Worker &w_ctx ) {
            return make_shared<SimpleBindExpr>( list[lm.at( "expression" )], new_rule.precedence, list );
        };
        a_ctx.rules.push_back( new_rule );
    }
    for ( auto &sb : pc.alias_bindings ) {
        parse_rule( new_rule, lm, sb.syntax );
        new_rule.precedence = sb.precedence;
        new_rule.ltr = sb.ltr;
        new_rule.create = [=]( auto &list, Worker &w_ctx ) {
            return make_shared<AliasBindExpr>( list[lm.at( "expression" )], new_rule.precedence, list );
        };
        a_ctx.rules.push_back( new_rule );
    }

    // OOP
    for ( auto &sb : pc.structs ) {
        parse_rule( new_rule, lm, sb.syntax );
        new_rule.precedence = sb.precedence;
        new_rule.ltr = sb.ltr;
        new_rule.create = [=]( auto &list, Worker &w_ctx ) {
            return make_shared<StructExpr>(
                ( lm.find( "name" ) != lm.end() ? list[lm.at( "name" )] : nullptr ),
                ( lm.find( "body" ) != lm.end() ? std::dynamic_pointer_cast<CompletedExpr>( list[lm.at( "body" )] )
                                                : nullptr ),
                new_rule.precedence, list );
        };
        a_ctx.rules.push_back( new_rule );
    }
    for ( auto &sb : pc.trait ) {
        parse_rule( new_rule, lm, sb.syntax );
        new_rule.precedence = sb.precedence;
        new_rule.ltr = sb.ltr;
        new_rule.create = [=]( auto &list, Worker &w_ctx ) {
            return make_shared<TraitExpr>( list[lm.at( "name" )],
                                           std::dynamic_pointer_cast<CompletedExpr>( list[lm.at( "body" )] ),
                                           new_rule.precedence, list );
        };
        a_ctx.rules.push_back( new_rule );
    }
    for ( auto &sb : pc.impl ) {
        parse_rule( new_rule, lm, sb.syntax );
        new_rule.precedence = sb.precedence;
        new_rule.ltr = sb.ltr;
        new_rule.create = [=]( auto &list, Worker &w_ctx ) {
            return make_shared<ImplExpr>(
                list[lm.at( "type" )], ( lm.find( "trait" ) != lm.end() ? list[lm.at( "trait" )] : nullptr ),
                std::dynamic_pointer_cast<CompletedExpr>( list[lm.at( "body" )] ), new_rule.precedence, list );
        };
        a_ctx.rules.push_back( new_rule );
    }

    // Control flow
    for ( auto &sb : pc.if_condition ) {
        parse_rule( new_rule, lm, sb.syntax );
        new_rule.precedence = sb.precedence;
        new_rule.ltr = sb.ltr;
        new_rule.create = [=]( auto &list, Worker &w_ctx ) {
            return make_shared<IfExpr>( list[lm.at( "condition" )], list[lm.at( "exec0" )], new_rule.precedence, list );
        };
        a_ctx.rules.push_back( new_rule );
    }
    for ( auto &sb : pc.if_else_condition ) {
        parse_rule( new_rule, lm, sb.syntax );
        new_rule.precedence = sb.precedence;
        new_rule.ltr = sb.ltr;
        new_rule.create = [=]( auto &list, Worker &w_ctx ) {
            return make_shared<IfElseExpr>( list[lm.at( "condition" )], list[lm.at( "exec0" )], list[lm.at( "exec1" )],
                                            new_rule.precedence, list );
        };
        a_ctx.rules.push_back( new_rule );
    }
    for ( auto &sb : pc.pre_loop ) {
        parse_rule( new_rule, lm, sb.first.syntax );
        new_rule.precedence = sb.first.precedence;
        new_rule.ltr = sb.first.ltr;
        new_rule.create = [=]( auto &list, Worker &w_ctx ) {
            return make_shared<PreLoopExpr>( list[lm.at( "condition" )], list[lm.at( "exec" )], sb.second,
                                             new_rule.precedence, list );
        };
        a_ctx.rules.push_back( new_rule );
    }
    for ( auto &sb : pc.post_loop ) {
        parse_rule( new_rule, lm, sb.first.syntax );
        new_rule.precedence = sb.first.precedence;
        new_rule.ltr = sb.first.ltr;
        new_rule.create = [=]( auto &list, Worker &w_ctx ) {
            return make_shared<PostLoopExpr>( list[lm.at( "condition" )], list[lm.at( "exec" )], sb.second,
                                              new_rule.precedence, list );
        };
        a_ctx.rules.push_back( new_rule );
    }
    for ( auto &sb : pc.inf_loop ) {
        parse_rule( new_rule, lm, sb.syntax );
        new_rule.precedence = sb.precedence;
        new_rule.ltr = sb.ltr;
        new_rule.create = [=]( auto &list, Worker &w_ctx ) {
            return make_shared<InfLoopExpr>( list[lm.at( "exec" )], new_rule.precedence, list );
        };
        a_ctx.rules.push_back( new_rule );
    }
    for ( auto &sb : pc.interator_loop ) {
        parse_rule( new_rule, lm, sb.syntax );
        new_rule.precedence = sb.precedence;
        new_rule.ltr = sb.ltr;
        new_rule.create = [=]( auto &list, Worker &w_ctx ) {
            return make_shared<ItrLoopExpr>( list[lm.at( "iterator" )], list[lm.at( "exec" )], new_rule.precedence,
                                             list );
        };
        a_ctx.rules.push_back( new_rule );
    }
    for ( auto &sb : pc.matching ) {
        parse_rule( new_rule, lm, sb.syntax );
        new_rule.precedence = sb.precedence;
        new_rule.ltr = sb.ltr;
        new_rule.create = [=]( auto &list, Worker &w_ctx ) {
            return make_shared<MatchExpr>( list[lm.at( "selector" )], list[lm.at( "cases" )], new_rule.precedence,
                                           list );
        };
        a_ctx.rules.push_back( new_rule );
    }

    // Functions
    for ( auto &f : pc.fn_call ) {
        parse_rule( new_rule, lm, f.syntax );
        new_rule.precedence = f.precedence;
        new_rule.ltr = f.ltr;
        new_rule.create = [=]( auto &list, Worker &w_ctx ) {
            return make_shared<FuncCallExpr>(
                std::dynamic_pointer_cast<SymbolExpr>( list[lm.at( "exec" )] ), 0,
                ( lm.find( "parameters" ) == lm.end()
                      ? nullptr
                      : std::dynamic_pointer_cast<TupleExpr>( list[lm.at( "parameters" )] ) ),
                new_rule.precedence, list );
        };
        a_ctx.rules.push_back( new_rule );
    }
    for ( auto &f : pc.fn_declarations ) {
        parse_rule( new_rule, lm, f.op.syntax );
        new_rule.precedence = f.op.precedence;
        new_rule.ltr = f.op.ltr;
        new_rule.create = [=]( auto &list, Worker &w_ctx ) {
            return make_shared<FuncDecExpr>( std::dynamic_pointer_cast<SymbolExpr>( list[lm.at( "name" )] ), 0,
                                             new_rule.precedence, list );
        };
        a_ctx.rules.push_back( new_rule );
    }
    for ( auto &f : pc.fn_definitions ) {
        parse_rule( new_rule, lm, f.op.syntax );
        new_rule.precedence = f.op.precedence;
        new_rule.ltr = f.op.ltr;
        new_rule.create = [=]( auto &list, Worker &w_ctx ) {
            if ( lm.find( "head" ) != lm.end() ) { // function with parameters
                auto head = std::dynamic_pointer_cast<FuncCallExpr>( list[lm.at( "head" )] );
                return make_shared<FuncExpr>( head->symbol, head->type, head->parameters,
                                              std::dynamic_pointer_cast<CompletedExpr>( list[lm.at( "body" )] ),
                                              new_rule.precedence, list );
            } else { // function with no parameters
                return make_shared<FuncExpr>(
                    std::dynamic_pointer_cast<SymbolExpr>( list[lm.at( "name" )] ), 0,
                    ( lm.find( "parameters" ) == lm.end()
                          ? nullptr
                          : std::dynamic_pointer_cast<TupleExpr>( list[lm.at( "parameters" )] ) ),
                    std::dynamic_pointer_cast<CompletedExpr>( list[lm.at( "body" )] ), new_rule.precedence, list );
            }
        };
        a_ctx.rules.push_back( new_rule );
    }

    // Relative access
    for ( auto &sb : pc.array_access_op ) {
        parse_rule( new_rule, lm, sb.syntax );
        new_rule.precedence = sb.precedence;
        new_rule.ltr = sb.ltr;
        new_rule.create = [=]( auto &list, Worker &w_ctx ) {
            auto index = std::dynamic_pointer_cast<ArraySpecifierExpr>( list[lm.at( "index" )] );
            if ( index->sub_expr.size() != 1 ) {
                // Array access must be done with a single index
                w_ctx.print_msg<MessageType::err_array_access_with_multiple_expr>(
                    MessageInfo( index->get_position_info(), 0, FmtStr::Color::Red ) );
            }
            return make_shared<ArrayAccessExpr>( list[lm.at( "value" )], index->sub_expr[0], new_rule.precedence,
                                                 list );
        };
        a_ctx.rules.push_back( new_rule );
    }

    // Ranges
    for ( auto &sb : pc.range_op ) {
        parse_rule( new_rule, lm, sb.op.syntax );
        new_rule.precedence = sb.op.precedence;
        new_rule.ltr = sb.op.ltr;
        new_rule.create = [=]( auto &list, Worker &w_ctx ) {
            return make_shared<RangeExpr>( ( lm.find( "lvalue" ) != lm.end() ? list[lm.at( "lvalue" )] : nullptr ),
                                           ( lm.find( "rvalue" ) != lm.end() ? list[lm.at( "rvalue" )] : nullptr ),
                                           sb.type, new_rule.precedence, list );
        };
        a_ctx.rules.push_back( new_rule );
    }

    // Operators
    for ( auto &o : pc.operators ) {
        parse_rule( new_rule, lm, o.op.syntax );
        new_rule.precedence = o.op.precedence;
        new_rule.ltr = o.op.ltr;
        new_rule.create = [=]( auto &list, Worker &w_ctx ) {
            return make_shared<OperatorExpr>( std::dynamic_pointer_cast<TokenExpr>( list[lm.at( "op" )] )->t.content,
                                              ( lm.find( "lvalue" ) == lm.end() ? nullptr : list[lm.at( "lvalue" )] ),
                                              ( lm.find( "rvalue" ) == lm.end() ? nullptr : list[lm.at( "rvalue" )] ),
                                              o.op.precedence, list );
        };
        a_ctx.rules.push_back( new_rule );
    }

    // Sort rules after precedence
    std::stable_sort( a_ctx.rules.begin(), a_ctx.rules.end(),
                      []( auto &l, auto &r ) { return l.precedence > r.precedence; } );
}

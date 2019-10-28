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
#include "libpushc/Prelude.h"
#include "libpushc/Expression.h"
#include "libpushc/Ast.h"

using TT = Token::Type;

// Consume any amount of comments if any TODO use impl in Util.h
void consume_comment( SourceInput &input ) {
    while ( input.preview_token().type == TT::comment_begin ) {
        input.get_token(); // consume consume comment begin
        // Consume any token until the comment ends
        while ( input.get_token().type != TT::comment_end )
            ;
    }
}

// Check for an expected token and returns it. All preceding comments are ignored
template <MessageType MesT>
Token expect_token_or_comment( TT type, SourceInput &input, Worker &w_ctx ) {
    consume_comment( input );
    Token t = input.get_token();
    if ( t.type != type ) {
        w_ctx.print_msg<MesT>( MessageInfo( t, 0, FmtStr::Color::Red ), std::vector<MessageInfo>{},
                               Token::get_name( type ) );
    }
    return t;
}

// Parse a following string. Including the string begin and end tokens TODO use impl in Util.h
String extract_string( SourceInput &input, Worker &w_ctx ) {
    expect_token_or_comment<MessageType::err_expected_string>( TT::string_begin, input, w_ctx );

    String content;
    Token t;
    while ( ( t = input.get_token() ).type != TT::string_end ) {
        if ( t.type == TT::eof ) {
            w_ctx.print_msg<MessageType::err_unexpected_eof>( MessageInfo( t, 0, FmtStr::Color::Red ) );
            break;
        }
        content += t.leading_ws + t.content;
    }
    content += t.leading_ws;

    return content;
}

// Checks if a prelude is defined and loads the proper prelude.
// Should be called at the beginning of a file
void select_prelude( SourceInput &input, Worker &w_ctx ) {
    // Load prelude-prelude first
    w_ctx.unit_ctx()->prelude_conf =
        w_ctx.do_query( load_prelude, make_shared<String>( "prelude" ) )->jobs.front()->to<PreludeConfig>();
    input.configure( w_ctx.unit_ctx()->prelude_conf.token_conf );

    // Consume any leading comment
    consume_comment( input );

    // Parse prelude command
    Token t = input.preview_token();
    PreludeConfig p_conf;
    if ( t.type == TT::op && t.content == "#" ) {
        t = input.preview_next_token();
        if ( t.type != TT::identifier || t.content != "prelude" ) {
            w_ctx.print_msg<MessageType::err_malformed_prelude_command>( MessageInfo( t, 0, FmtStr::Color::Red ),
                                                                         std::vector<MessageInfo>{},
                                                                         Token::get_name( TT::identifier ) );
        }
        input.get_token(); // consume
        input.get_token(); // consume

        // Found a prelude
        expect_token_or_comment<MessageType::err_malformed_prelude_command>( TT::term_begin, input, w_ctx );

        t = input.preview_token();
        if ( t.type == TT::identifier ) {
            input.get_token(); // consume
            p_conf = w_ctx.do_query( load_prelude, make_shared<String>( t.content ) )->jobs.back()->to<PreludeConfig>();
        } else if ( t.type == TT::string_begin ) {
            String path = extract_string( input, w_ctx );
            p_conf = *w_ctx.do_query( load_prelude_file, make_shared<String>( path ) )
                          ->jobs.back()
                          ->to<sptr<PreludeConfig>>();
        } else {
            // invalid prelude identifier
            w_ctx.print_msg<MessageType::err_unexpected_eof>( MessageInfo( t, 0, FmtStr::Color::Red ) );
            // load default prelude as fallback
            p_conf = w_ctx.do_query( load_prelude, make_shared<String>( "push" ) )->jobs.back()->to<PreludeConfig>();
        }

        expect_token_or_comment<MessageType::err_malformed_prelude_command>( TT::term_end, input, w_ctx );
    } else {
        // Load default prelude
        p_conf = w_ctx.do_query( load_prelude, make_shared<String>( "push" ) )->jobs.back()->to<PreludeConfig>();
    }

    // Load the actual configuration
    w_ctx.unit_ctx()->prelude_conf = p_conf;
    input.configure( w_ctx.unit_ctx()->prelude_conf.token_conf );
}

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
        } else if ( expr.first == "attributes_list" ) {
            // TODO
        } else if ( expr.first == "expr_block" ) {
            sr.expr_list.push_back( make_shared<BlockExpr>() );
        } else {
            // Keyword or operator
            sr.expr_list.push_back(
                make_shared<TokenExpr>( Token( TT::op, expr.first, nullptr, 0, 0, 0, "", TokenLevel::normal ) ) );
        }
        ctr++;
    }
}

// Translates the prelude syntax rules into ast syntax rules
void load_syntax_rules( Worker &w_ctx, AstCtx &a_ctx ) {
    auto pc = w_ctx.unit_ctx()->prelude_conf;

    SyntaxRule new_rule;
    LabelMap lm;

    // TODO handle function definitions and declarations separately
    for ( auto &f : pc.fn_declarations ) {
        parse_rule( new_rule, lm, f.syntax );
        new_rule.matching_expr = make_shared<FuncDecExpr>();
        new_rule.create = [=]( auto &list ) {
            return make_shared<FuncDecExpr>( std::dynamic_pointer_cast<SymbolExpr>( list[lm.at( "name" )] ), 0, list );
        };
        a_ctx.rules.push_back( new_rule );
    }
    for ( auto &f : pc.fn_definitions ) {
        parse_rule( new_rule, lm, f.syntax );
        new_rule.matching_expr = make_shared<FuncExpr>();
        new_rule.create = [=]( auto &list ) {
            return make_shared<FuncExpr>( std::dynamic_pointer_cast<SymbolExpr>( list[lm.at( "name" )] ), 0,
                                          std::dynamic_pointer_cast<BlockExpr>( list[lm.at( "body" )] ), list );
        };
        a_ctx.rules.push_back( new_rule );
    }

    for ( auto &o : pc.operators ) {
        parse_rule( new_rule, lm, o.op.syntax );
        new_rule.precedence = o.op.precedence;
        new_rule.ltr = o.op.ltr;
        new_rule.matching_expr = make_shared<OperatorExpr>();
        new_rule.create = [=]( auto &list ) {
            return make_shared<OperatorExpr>( std::dynamic_pointer_cast<TokenExpr>( list[lm.at( "op" )] )->t.content,
                                              list[lm.at( "lvalue" )], list[lm.at( "rvalue" )], o.op.precedence, list );
        };
        a_ctx.rules.push_back( new_rule );
    }

    // Sort rules after precedence
    std::sort( a_ctx.rules.begin(), a_ctx.rules.end(), []( auto l, auto r ) { return l.precedence > r.precedence; } );
}

// Parses a scope into the ast. Used recursively
sptr<Expr> parse_scope( SourceInput &input, Worker &w_ctx, AstCtx &a_ctx, TT end_token ) {
    std::vector<sptr<Expr>> expr_list;

    // Iterate through all tokens in this scope
    while ( true ) {
        // Load the next token
        consume_comment( input );
        auto t = input.get_token();

        if ( t.type == end_token ) {
            break;
        } else if ( t.type == TT::eof ) {
            w_ctx.print_msg<MessageType::err_unexpected_eof>( MessageInfo( t, 0, FmtStr::Color::Red ) );
            break;
        } else if ( t.type == TT::block_begin || t.type == TT::term_begin ) {
            // TODO change a_ctx.next_symbol.name_chain to procede into new scope
            expr_list.push_back(
                parse_scope( input, w_ctx, a_ctx, ( t.type == TT::block_begin ? TT::block_end : TT::term_end ) ) );
            continue;
        } else if ( t.type == TT::identifier ) {
            auto expr = make_shared<SymbolExpr>();

            // Create a new symbol TODO maybe extract into own function
            expr->symbol = a_ctx.next_symbol.id++;
            auto &sm = a_ctx.ast.symbol_map;
            if ( sm.size() <= expr->symbol )
                sm.resize( expr->symbol + 1 );
            sm[expr->symbol].id = expr->symbol;
            sm[expr->symbol].name_chain = a_ctx.next_symbol.name_chain;
            sm[expr->symbol].name_chain.push_back( t.content );

            expr_list.push_back( expr );
        } else {
            expr_list.push_back( make_shared<TokenExpr>( t ) );
        }

        // Test new token
        bool recheck = false; // used to test if a new rule matches after one was applied
        do {
            recheck = false;
            // Check each syntax rule
            for ( auto &rule : a_ctx.rules ) {
                u8 rule_length = rule.expr_list.size();
                if ( expr_list.size() >= rule_length ) {
                    // Prepare backtracing
                    std::vector<sptr<Expr>> deep_expr_list;
                    auto back_expr =
                        *( expr_list.end() - rule_length ); // start with the left element of the syntax rule
                    auto back = std::dynamic_pointer_cast<SeparableExpr>( back_expr );
                    // Interate though the rightmost elements and split them
                    while ( true ) {
                        if ( back &&
                             ( back->prec() > rule.precedence || ( !rule.ltr && back->prec() == rule.precedence ) ) ) {
                            // Is separable and has lower precedence
                            auto &splitted = back->split();
                            deep_expr_list.insert( deep_expr_list.end(), splitted.begin(), splitted.end() - 1 );
                            back_expr = splitted.back();
                            back = std::dynamic_pointer_cast<SeparableExpr>( back_expr );
                        } else {
                            deep_expr_list.push_back( back_expr );
                            break;
                        }
                    }

                    // Append end
                    deep_expr_list.insert( deep_expr_list.end(), expr_list.end() - rule_length + 1, expr_list.end() );

                    // Check if syntax matches
                    if ( rule.matches_end( deep_expr_list ) ) {
                        expr_list.resize( expr_list.size() - rule_length );
                        expr_list.insert( expr_list.end(), deep_expr_list.begin(),
                                          ( rule_length < deep_expr_list.size() ? deep_expr_list.end() - rule_length
                                                                                : deep_expr_list.begin() ) );
                        deep_expr_list.erase( deep_expr_list.begin(), deep_expr_list.end() - rule_length );
                        expr_list.push_back( rule.create( deep_expr_list ) );

                        recheck = true;
                        break;
                    }
                }
            }
        } while ( recheck );
    }

    // Create block
    if ( end_token == TT::eof ) {
        auto block = make_shared<DeclExpr>();
        block->sub_expr = expr_list;
        return block;
    } else if ( end_token == TT::block_end ) {
        auto block = make_shared<BlockExpr>();
        block->sub_expr = expr_list;
        return block;
    } else {
        LOG_ERR( "Try to parse a block which is no block" );
        return nullptr;
    }
}


void get_ast( JobsBuilder &jb, UnitCtx &parent_ctx ) {
    jb.add_job<void>( []( Worker &w_ctx ) { w_ctx.do_query( parse_ast ); } );
}

void parse_ast( JobsBuilder &jb, UnitCtx &parent_ctx ) {
    jb.add_job<void>( []( Worker &w_ctx ) {
        auto start_time = std::chrono::system_clock::now();
        auto input = get_source_input( w_ctx.unit_ctx()->root_file, w_ctx );
        if ( !input )
            return;

        select_prelude( *input, w_ctx );

        // Create a new AST context
        AstCtx a_ctx;
        a_ctx.next_symbol.id = 1;
        load_syntax_rules( w_ctx, a_ctx );

        // parse global scope
        a_ctx.ast.block = parse_scope( *input, w_ctx, a_ctx, TT::eof );

        // DEBUG print AST
        log( "AST ----------" );
        log( " " + a_ctx.ast.block->get_debug_repr().replace_all( "{", "{\n" ).replace_all( "}", "\n }" ) );
        log( "SYMBOLS ------" );
        for ( auto &s : a_ctx.ast.symbol_map ) {
            String name;
            for ( auto &c : s.name_chain ) {
                name += "::" + c;
            }
            log( " " + to_string( s.id ) + " - " + name );
        }
        log( "--------------" );
        auto duration = std::chrono::system_clock::now() - start_time;
        log( "Took " + to_string( duration.count() / 1000000 ) + " milliseconds" );
    } );
}

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
#include "libpushc/Util.h"

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
            w_ctx.print_msg<MessageType::err_unexpected_eof_after>( MessageInfo( t, 0, FmtStr::Color::Red ) );
            break;
        }
        content += t.leading_ws + t.content;
    }
    content += t.leading_ws;

    return content;
}

// Splits a symbol string into a chain of strings
sptr<std::vector<String>> split_symbol_chain( const String &chained, String separator ) {
    auto ret = make_shared<std::vector<String>>();
    size_t pos = 0;
    size_t prev_pos = pos;
    while ( pos != String::npos ) {
        pos = chained.find( separator, pos );
        ret->push_back( chained.slice( prev_pos, pos - prev_pos ) );
        if ( pos != String::npos )
            pos += separator.size();
        prev_pos = pos;
    }
    return ret;
}

// Creates a new symbol from a global name
SymbolId create_new_absolute_symbol( AstCtx &a_ctx, const sptr<std::vector<String>> &name ) {
    SymbolId sym_id = a_ctx.next_symbol.id++;
    auto &sm = a_ctx.ast.symbol_map;
    if ( sm.size() <= sym_id )
        sm.resize( sym_id + 1 );
    sm[sym_id].id = sym_id;
    sm[sym_id].name_chain = *name;
    return sym_id;
}
// Creates a new symbol from a local name
SymbolId create_new_relative_symbol( AstCtx &a_ctx, const String &name ) {
    auto tmp_name = make_shared<std::vector<String>>();
    tmp_name->push_back( name );
    return create_new_absolute_symbol( a_ctx, tmp_name );
}
// Creates a new type from a global name
TypeId create_new_absolute_type( AstCtx &a_ctx, const sptr<std::vector<String>> &name, TypeMemSize size ) {
    SymbolId sym_id = create_new_absolute_symbol( a_ctx, name );
    TypeId type_id = a_ctx.next_type++;
    auto &tm = a_ctx.ast.type_map;
    if ( tm.size() <= type_id )
        tm.resize( type_id + 1 );
    tm[type_id].id = type_id;
    tm[type_id].symbol = sym_id;
    tm[type_id].mem_size = size;

    a_ctx.type_id_map[*name] = type_id;
    return type_id;
}
// Creates a new type from a local name
TypeId create_new_relative_type( AstCtx &a_ctx, const String &name, TypeMemSize size ) {
    auto tmp_name = make_shared<std::vector<String>>();
    tmp_name->push_back( name );
    return create_new_absolute_type( a_ctx, tmp_name, size );
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
            w_ctx.print_msg<MessageType::err_unexpected_eof_after>( MessageInfo( t, 0, FmtStr::Color::Red ) );
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

// Parses a scope into the ast. Used recursively. @param last_token may be nullptr
sptr<Expr> parse_scope( SourceInput &input, Worker &w_ctx, AstCtx &a_ctx, TT end_token, Token *last_token ) {
    auto &conf = w_ctx.unit_ctx()->prelude_conf;
    std::vector<sptr<Expr>> expr_list;

    // Iterate through all tokens in this scope
    while ( true ) {
        // Load the next token
        consume_comment( input );
        auto t = input.get_token();

        if ( t.type == end_token ) {
            break;
        } else if ( t.type == TT::eof ) {
            if ( last_token ) {
                w_ctx.print_msg<MessageType::err_unexpected_eof_after>(
                    MessageInfo( *last_token, 0, FmtStr::Color::Red ) );
            } else {
                w_ctx.print_msg<MessageType::err_unexpected_eof_after>( MessageInfo( 0, FmtStr::Color::Red ) );
            }
            break;
        } else if ( t.type == TT::block_begin || t.type == TT::term_begin ) {
            // TODO change a_ctx.next_symbol.name_chain to procede into new scope
            expr_list.push_back(
                parse_scope( input, w_ctx, a_ctx, ( t.type == TT::block_begin ? TT::block_end : TT::term_end ), &t ) );
        } else if ( t.type == TT::identifier ) {
            if ( a_ctx.literals_map.find( t.content ) != a_ctx.literals_map.end() ) {
                // Found a special literal keyword
                auto literal = a_ctx.literals_map[t.content];
                auto expr = make_shared<BlobLiteralExpr<sizeof( Number )>>();
                expr->type = literal.first;

                u8 size = a_ctx.ast.type_map[literal.first].mem_size;
                expr->load_from_number( literal.second, size );

                expr->pos_info = { t.file, t.line, t.column, t.length };

                expr_list.push_back( expr );
            } else {
                // Normal identifier/symbol
                auto expr = make_shared<SymbolExpr>();

                // Create a new symbol TODO maybe extract into own function
                expr->symbol = a_ctx.next_symbol.id++;
                auto &sm = a_ctx.ast.symbol_map;
                if ( sm.size() <= expr->symbol )
                    sm.resize( expr->symbol + 1 );
                sm[expr->symbol].id = expr->symbol;
                sm[expr->symbol].name_chain = a_ctx.next_symbol.name_chain;
                sm[expr->symbol].name_chain.push_back( t.content );

                expr->pos_info = { t.file, t.line, t.column, t.length };

                expr_list.push_back( expr );
            }
        } else if ( t.type == TT::number ) {
            auto expr = make_shared<BlobLiteralExpr<sizeof( Number )>>();
            expr->type = a_ctx.int_type;

            Number val = stoull( t.content );
            expr->load_from_number( val, sizeof( Number ) );

            expr->pos_info = { t.file, t.line, t.column, t.length };

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
                if ( expr_list.size() >= rule_length && rule.end_matches( expr_list ) ) {
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
                    if ( rule.front_matches( *( deep_expr_list.end() - rule_length ) ) ) {
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
        block->pos_info = { last_token->file, last_token->line, last_token->column, last_token->length };
        block->sub_expr = expr_list;
        return block;
    } else if ( end_token == TT::term_end ) {
        auto block = make_shared<TermExpr>();
        block->pos_info = { last_token->file, last_token->line, last_token->column, last_token->length };
        if ( expr_list.size() > 1 ) {
            w_ctx.print_msg<MessageType::err_term_with_multiple_expr>(
                MessageInfo( ( *( expr_list.begin() + 1 ) )->get_position_info(), 0, FmtStr::Color::Red ) );
        }
        block->sub_expr = expr_list.empty() ? nullptr : expr_list.back();
        return block;
    } else {
        LOG_ERR( "Try to parse a block which is no block" );
        return nullptr;
    }
}

void load_base_types( AstCtx &a_ctx, PreludeConfig &cfg ) {
    // Most basic types/traits
    a_ctx.int_type =
        create_new_absolute_type( a_ctx, split_symbol_chain( cfg.integer_trait, cfg.scope_access_operator ), 0 );

    // Memblob types
    for ( auto &mbt : cfg.memblob_types ) {
        create_new_relative_type( a_ctx, mbt.first, mbt.second );
    }

    // Literals
    for ( auto &lit : cfg.literals ) {
        TypeId type = a_ctx.type_id_map[*split_symbol_chain( lit.second.first, cfg.scope_access_operator )];
        a_ctx.literals_map[lit.first] = std::make_pair( type, lit.second.second );
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
        load_base_types( a_ctx, w_ctx.unit_ctx()->prelude_conf );
        load_syntax_rules( w_ctx, a_ctx );

        // parse global scope
        a_ctx.ast.block = parse_scope( *input, w_ctx, a_ctx, TT::eof, nullptr );

        // DEBUG print AST
        log( "AST ----------" );
        log( " " + a_ctx.ast.block->get_debug_repr().replace_all( "{", "{\n" ).replace_all( "}", "\n }" ) );
        /*log( "SYMBOLS ------" );
        for ( auto &s : a_ctx.ast.symbol_map ) {
            if ( s.id != 0 ) {
                String name;
                for ( auto &c : s.name_chain ) {
                    name += "::" + c;
                }
                log( " " + to_string( s.id ) + " - " + name );
            }
        }*/
        log( "TYPES --------" );
        for ( auto &t : a_ctx.ast.type_map ) {
            if ( t.id != 0 ) {
                auto s = a_ctx.ast.symbol_map[t.symbol];
                String sym_name;
                for ( auto &c : s.name_chain ) {
                    sym_name += "::" + c;
                }
                log( " id " + to_string( t.id ) + " size " + to_string( t.mem_size ) + " - sym " +
                     to_string( t.symbol ) + " " + sym_name );
            }
        }
        log( "--------------" );
        auto duration = std::chrono::system_clock::now() - start_time;
        log( "Took " + to_string( duration.count() / 1000000 ) + " milliseconds" );
    } );
}

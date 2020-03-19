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
sptr<Expr> parse_scope( sptr<SourceInput> &input, Worker &w_ctx, AstCtx &a_ctx, TT end_token, Token *last_token ) {
    auto &conf = w_ctx.unit_ctx()->prelude_conf;
    std::vector<std::pair<std::vector<sptr<Expr>>, std::vector<std::pair<u32, u32>>>>
        expr_lists; // the paths with their expression lists and precedence lists as class-from-pairs
    expr_lists.push_back( std::make_pair(
        std::vector<sptr<Expr>>(),
        std::vector<std::pair<u32, u32>>{ std::make_pair( UINT32_MAX, UINT32_MAX ) } ) ); // add a starting path

    // Iterate through all tokens in this scope
    while ( true ) {
        // Load the next token
        consume_comment( *input );
        auto t = input->preview_token();

        // First check what token it is
        sptr<Expr> add_to_all_paths;
        if ( t.type == end_token ) {
            input->get_token(); // consume
            break;
        } else if ( t.type == TT::eof ) {
            if ( last_token ) {
                w_ctx.print_msg<MessageType::err_unexpected_eof_after>(
                    MessageInfo( *last_token, 0, FmtStr::Color::Red ) );
            } else {
                w_ctx.print_msg<MessageType::err_unexpected_eof_after>( MessageInfo( 0, FmtStr::Color::Red ) );
            }
            break;
        } else if ( t.type == TT::block_begin || t.type == TT::term_begin || t.type == TT::array_begin ) {
            // TODO change a_ctx.next_symbol.name_chain to proceed into the new scope
            input->get_token(); // consume
            add_to_all_paths = parse_scope(
                input, w_ctx, a_ctx,
                ( t.type == TT::block_begin ? TT::block_end : t.type == TT::term_begin ? TT::term_end : TT::array_end ),
                &t );
        } else if ( t.type == TT::identifier ) {
            input->get_token(); // consume
            if ( a_ctx.literals_map.find( t.content ) != a_ctx.literals_map.end() ) {
                // Found a special literal keyword
                auto literal = a_ctx.literals_map[t.content];
                auto expr = make_shared<BlobLiteralExpr<sizeof( Number )>>();
                expr->type = literal.first;

                u8 size = a_ctx.ast.type_map[literal.first].mem_size;
                expr->load_from_number( literal.second, size );

                expr->pos_info = { t.file, t.line, t.column, t.length };

                add_to_all_paths = expr;
            } else {
                // Normal identifier/symbol
                auto expr = make_shared<AtomicSymbolExpr>();

                // Create a new symbol TODO maybe extract into own function
                expr->symbol = a_ctx.next_symbol.id++;
                auto &sm = a_ctx.ast.symbol_map;
                if ( sm.size() <= expr->symbol )
                    sm.resize( expr->symbol + 1 );
                sm[expr->symbol].id = expr->symbol;
                sm[expr->symbol].name_chain = a_ctx.next_symbol.name_chain;
                sm[expr->symbol].name_chain.push_back( t.content );

                expr->pos_info = { t.file, t.line, t.column, t.length };

                add_to_all_paths = expr;
            }
        } else if ( t.type == TT::number ) {
            input->get_token(); // consume
            auto expr = make_shared<BlobLiteralExpr<sizeof( Number )>>();
            expr->type = a_ctx.int_type;

            Number val = stoull( t.content );
            expr->load_from_number( val, sizeof( Number ) );

            expr->pos_info = { t.file, t.line, t.column, t.length };

            add_to_all_paths = expr;
        } else if ( t.type == TT::stat_divider ) {
            input->get_token(); // consume
            for ( auto &expr_list : expr_lists ) {
                if ( expr_list.first.empty() ) {
                    if ( expr_list == expr_lists.front() ) { // only error once
                        w_ctx.print_msg<MessageType::err_semicolon_without_meaning>(
                            MessageInfo( t, 0, FmtStr::Color::Red ) );
                    }
                } else {
                    auto expr = make_shared<SingleCompletedExpr>();
                    expr->pos_info = { t.file, t.line, t.column, t.length };
                    expr->sub_expr = expr_list.first.back();
                    expr_list.first.back() = expr;
                }
            }
        } else if ( t.type == TT::string_begin ) {
            auto expr = make_shared<StringLiteralExpr>();
            expr->str = parse_string( input, w_ctx );
            expr->type = a_ctx.str_type;
            expr->pos_info = { t.file, t.line, t.column, t.length };
            add_to_all_paths = expr;
        } else {
            input->get_token(); // consume
            add_to_all_paths = make_shared<TokenExpr>( t );
        }

        // Update paths with new token
        if ( add_to_all_paths ) {
            for ( auto &expr_list : expr_lists ) {
                expr_list.first.push_back( add_to_all_paths );
            }
        }

        // Test new token for all paths
        u32 fold_counter = 0;
        size_t old_paths_count = expr_lists.size();
        for ( size_t i = 0; i < old_paths_count; i++ ) {
            auto *expr_list = &expr_lists[i];

            // Test new token
            bool recheck = false; // used to test if a new rule matches after one was applied
            size_t skip_ctr = 0; // skip already parsed token
            do {
                recheck = false;
                SyntaxRule *best_rule = nullptr;
                std::vector<sptr<Expr>> best_rule_rev_deep_expr_list;
                std::vector<sptr<StaticStatementExpr>> best_rule_stst_set;
                size_t best_rule_cutout_ctr;
                // Check each syntax rule
                for ( auto &rule : a_ctx.rules ) {
                    bool use_bias =
                        ( best_rule ? ( rule.prec_bias != NO_BIAS_VALUE && best_rule->prec_bias != NO_BIAS_VALUE &&
                                        rule.prec_bias != best_rule->prec_bias )
                                    : false ); // helper variable
                    if ( !best_rule || ( !use_bias && rule.precedence <= best_rule->precedence ) ||
                         ( use_bias && rule.prec_bias < best_rule->prec_bias ) ) {
                        u8 rule_length = rule.expr_list.size();

                        // Prepare backtracing
                        std::vector<sptr<Expr>> rev_deep_expr_list;
                        std::vector<sptr<StaticStatementExpr>> stst_set;
                        size_t cutout_ctr = 0;
                        for ( auto expr_itr = expr_list->first.rbegin();
                              expr_itr != expr_list->first.rend() && rev_deep_expr_list.size() < rule_length;
                              expr_itr++ ) {
                            auto stst_expr = std::dynamic_pointer_cast<StaticStatementExpr>( *expr_itr );
                            if ( stst_expr ) { // is a static statement
                                stst_set.push_back( stst_expr );
                            } else {
                                auto s_expr = std::dynamic_pointer_cast<SeparableExpr>( *expr_itr );
                                if ( cutout_ctr >= skip_ctr && rev_deep_expr_list.size() < rule_length && s_expr &&
                                     ( rule.precedence < s_expr->prec() ||
                                       ( !rule.ltr && rule.precedence == s_expr->prec() ) ) ) {
                                    // Split expr
                                    s_expr->split_prepend_recursively( rev_deep_expr_list, stst_set, rule.precedence,
                                                                       rule.ltr, rule_length );
                                } else { // Don't split expr
                                    rev_deep_expr_list.push_back( *expr_itr );
                                }
                            }
                            cutout_ctr++;
                        }

                        // Check if syntax matches
                        if ( rule.matches_reversed( rev_deep_expr_list ) ) {
                            best_rule = &rule;
                            best_rule_rev_deep_expr_list = rev_deep_expr_list;
                            best_rule_stst_set = stst_set;
                            best_rule_cutout_ctr = cutout_ctr;
                        }
                    }
                }

                // Apply rule
                if ( best_rule && ( !best_rule->ambiguous || skip_ctr <= 0 ) ) {
                    bool update_precedence_to_path = false;
                    if ( best_rule->ambiguous ) { // copy the not-changed path
                        expr_lists.push_back( *expr_list );
                        expr_list = &expr_lists[i]; // prevent interator invalidation
                        expr_lists.back().second.push_back( std::make_pair( UINT32_MAX, best_rule->prec_class.first ) );
                        expr_list->second.push_back(
                            std::make_pair( best_rule->prec_class.first, best_rule->prec_class.first ) );
                    } else if ( old_paths_count > 1 ) {
                        if ( expr_list->second.back().second == best_rule->prec_class.second &&
                             expr_list->second.back().first == UINT32_MAX ) {
                            // path precedence update, because class matches
                            expr_list->second.back().first = best_rule->prec_class.first;
                            update_precedence_to_path = true;
                            fold_counter++;
                        }
                    }

                    expr_list->first.resize( expr_list->first.size() - best_rule_cutout_ctr );
                    expr_list->first.insert( expr_list->first.end(), best_rule_rev_deep_expr_list.rbegin(),
                                             best_rule_rev_deep_expr_list.rend() - best_rule->expr_list.size() );
                    best_rule_rev_deep_expr_list.erase(
                        best_rule_rev_deep_expr_list.begin() + best_rule->expr_list.size(),
                        best_rule_rev_deep_expr_list.end() );
                    std::reverse( best_rule_rev_deep_expr_list.begin(), best_rule_rev_deep_expr_list.end() );
                    auto result_expr = best_rule->create( best_rule_rev_deep_expr_list, w_ctx );
                    result_expr->static_statements = best_rule_stst_set;
                    expr_list->first.push_back( result_expr );

                    if ( auto &&result_expr_separable = std::dynamic_pointer_cast<SeparableExpr>( result_expr );
                         result_expr && update_precedence_to_path ) { // path precedence overwrites normal precedence
                        result_expr_separable->update_precedence( best_rule->prec_class.first );
                    }

                    skip_ctr = 1; // 1 will always be desired even though more could technically be skipped
                    recheck = true;
                }
            } while ( recheck );
        }

        // Do path folding for optimization
        if ( fold_counter > 0 ) {
            size_t half_path_count = expr_lists.size() / 2;
            if ( fold_counter != half_path_count ) {
                LOG_ERR( "Path folding requested with " + to_string( fold_counter ) + " of " +
                         to_string( expr_lists.size() ) + " paths." );
            } else {
                for ( size_t i = 0; i < half_path_count; i++ ) {
                    if ( expr_lists[i].second.back().first > expr_lists[i + half_path_count].second.back().first ) {
                        // take the second path
                        expr_lists[i] = expr_lists[i + half_path_count];
                    }
                    // otherwise take the first path implicitly

                    // Shrink the path length
                    expr_lists[i].second.pop_back();
                }
                expr_lists.resize( half_path_count );
            }
        }
    }

    // Select the best path
    auto *best_list = &expr_lists.front();
    for ( auto &list : expr_lists ) {
        bool better = true, equal = true;
        for ( int i = 0; better && i < list.second.size(); i++ ) { // assuming all precedence lists have the same length
            if ( list.second[i] > best_list->second[i] )
                better = false;
            if ( list.second[i] != best_list->second[i] )
                equal = false;
        }
        if ( better && !equal )
            best_list = &list; // found a better path
    }
    auto &expr_list = best_list->first;

    // Create block
    if ( end_token == TT::eof ) {
        auto block = make_shared<DeclExpr>();
        block->sub_expr = expr_list;
        return block;
    } else if ( end_token == TT::block_end ) {
        if ( expr_list.size() == 1 && std::dynamic_pointer_cast<CommaExpr>( expr_list.front() ) ) { // set
            auto block = make_shared<SetExpr>();
            block->pos_info = { last_token->file, last_token->line, last_token->column, last_token->length };
            block->sub_expr = std::dynamic_pointer_cast<CommaExpr>( expr_list.front() )->exprs;
            return block;
        } else { // block
            auto block = make_shared<BlockExpr>();
            block->pos_info = { last_token->file, last_token->line, last_token->column, last_token->length };
            block->sub_expr = expr_list;
            return block;
        }
    } else if ( end_token == TT::term_end ) {
        if ( expr_list.size() > 1 ) { // malformed "tuple" or so
            w_ctx.print_msg<MessageType::err_term_with_multiple_expr>(
                MessageInfo( ( *( expr_list.begin() + 1 ) )->get_position_info(), 0, FmtStr::Color::Red ) );
            return make_shared<TupleExpr>(); // default
        } else { // normal term or tuple
            if ( expr_list.empty() ) { // Unit type
                auto block = make_shared<UnitExpr>();
                block->pos_info = { last_token->file, last_token->line, last_token->column, last_token->length };
                return block;
            } else if ( expr_list.size() == 1 && std::dynamic_pointer_cast<CommaExpr>( expr_list.front() ) ) { // tuple
                auto block = make_shared<TupleExpr>();
                block->pos_info = { last_token->file, last_token->line, last_token->column, last_token->length };
                block->sub_expr = std::dynamic_pointer_cast<CommaExpr>( expr_list.front() )->exprs;
                return block;
            } else { // term
                auto block = make_shared<TermExpr>();
                block->pos_info = { last_token->file, last_token->line, last_token->column, last_token->length };
                block->sub_expr = expr_list.empty() ? nullptr : expr_list.back();
                return block;
            }
        }
    } else if ( end_token == TT::array_end ) {
        auto block = make_shared<ArraySpecifierExpr>();
        block->pos_info = { last_token->file, last_token->line, last_token->column, last_token->length };
        block->sub_expr = expr_list;
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
    a_ctx.str_type =
        create_new_absolute_type( a_ctx, split_symbol_chain( cfg.string_trait, cfg.scope_access_operator ), 0 );

    // Memblob types
    for ( auto &mbt : cfg.memblob_types ) {
        create_new_absolute_type( a_ctx, split_symbol_chain( mbt.first, cfg.scope_access_operator ), mbt.second );
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
        a_ctx.ast.block = parse_scope( input, w_ctx, a_ctx, TT::eof, nullptr );

        // DEBUG print AST
        log( "AST ----------" );
        log( " " + a_ctx.ast.block->get_debug_repr() );
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
        log( "SYMBOLS ------" );
        for ( auto &s : a_ctx.ast.symbol_map ) {
            if ( s.id != 0 ) {
                String name;
                for ( auto &c : s.name_chain ) {
                    name += "::" + c;
                }
                log( " " + to_string( s.id ) + " - " + name );
            }
        }
        log( "--------------" );
        auto duration = std::chrono::system_clock::now() - start_time;
        log( "Took " + to_string( duration.count() / 1000000 ) + " milliseconds" );
    } );
}

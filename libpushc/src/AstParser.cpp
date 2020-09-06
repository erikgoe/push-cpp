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
#include "libpushc/SymbolParser.h"
#include "libpushc/SymbolUtil.h"

using TT = Token::Type;

// Consume any amount of comments if any TODO use impl in Util.h. First fix nested line comments
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
            String path = parse_string( input, w_ctx );
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
AstNode parse_scope( sptr<SourceInput> &input, Worker &w_ctx, CrateCtx &c_ctx, TT end_token, Token *last_token ) {
    auto &conf = w_ctx.unit_ctx()->prelude_conf;
    std::vector<std::pair<std::vector<AstNode>, std::vector<std::pair<u32, u32>>>>
        expr_lists; // the paths with their expression lists and precedence lists as class-from-pairs
    expr_lists.push_back( std::make_pair(
        std::vector<AstNode>(),
        std::vector<std::pair<u32, u32>>{ std::make_pair( UINT32_MAX, UINT32_MAX ) } ) ); // add a starting path

    // Iterate through all tokens in this scope
    while ( true ) {
        // Load the next token
        consume_comment( *input );
        auto t = input->preview_token();

        // First check what token it is
        AstNode add_to_all_paths{ ExprType::none };
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
        } else if ( t.type == TT::block_begin || t.type == TT::term_begin || t.type == TT::array_begin ) {
            input->get_token(); // consume
            add_to_all_paths = parse_scope(
                input, w_ctx, c_ctx,
                ( t.type == TT::block_begin ? TT::block_end : t.type == TT::term_begin ? TT::term_end : TT::array_end ),
                &t );
        } else if ( t.type == TT::identifier ) {
            input->get_token(); // consume
            if ( c_ctx.literals_map.find( t.content ) != c_ctx.literals_map.end() ) {
                // Found a special literal keyword
                auto literal = c_ctx.literals_map[t.content];
                auto expr = AstNode{ ExprType::numeric_literal };
                expr.generate_new_props();
                expr.literal_type = literal.first;

                TypeMemSize size = c_ctx.type_table[literal.first].additional_mem_size;
                expr.literal_number = literal.second;

                expr.pos_info = { t.file, t.line, t.column, t.length };

                add_to_all_paths = expr;
            } else {
                // Normal identifier/symbol
                auto expr = AstNode{ ExprType::atomic_symbol };
                expr.generate_new_props();
                expr.symbol_name = t.content;
                expr.pos_info = { t.file, t.line, t.column, t.length };
                add_to_all_paths = expr;
            }
        } else if ( t.type == TT::number ) {
            input->get_token(); // consume
            auto expr = AstNode{ ExprType::numeric_literal };
            expr.generate_new_props();
            expr.literal_type = c_ctx.int_type;

            Number val = stoull( t.content );
            expr.literal_number = val;

            expr.pos_info = { t.file, t.line, t.column, t.length };

            add_to_all_paths = expr;
        } else if ( t.type == TT::stat_divider ) {
            input->get_token(); // consume
            for ( auto &expr_list : expr_lists ) {
                if ( expr_list.first.empty() ) {
                    if ( &expr_list == &expr_lists.front() ) { // only error once
                        w_ctx.print_msg<MessageType::err_semicolon_without_meaning>(
                            MessageInfo( t, 0, FmtStr::Color::Red ) );
                    }
                } else {
                    auto expr = AstNode{ ExprType::single_completed };
                    expr.generate_new_props();
                    expr.pos_info = { t.file, t.line, t.column, t.length };
                    expr.children.push_back( expr_list.first.back() );
                    expr_list.first.back() = expr;
                }
            }
        } else if ( t.type == TT::string_begin ) {
            auto expr = AstNode{ ExprType::string_literal };
            expr.generate_new_props();
            expr.literal_string = parse_string( *input, w_ctx );
            expr.literal_type = c_ctx.str_type;
            expr.pos_info = { t.file, t.line, t.column, t.length };
            add_to_all_paths = expr;
        } else {
            input->get_token(); // consume
            add_to_all_paths.type = ExprType::token;
            add_to_all_paths.token = t;
            add_to_all_paths.pos_info = PosInfo{ t.file, t.line, t.column, t.length };
            add_to_all_paths.generate_new_props();
        }

        // Update paths with new token
        if ( add_to_all_paths.type != ExprType::none ) {
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
                std::vector<AstNode> best_rule_rev_deep_expr_list;
                std::vector<AstNode> best_rule_stst_set;
                size_t best_rule_cutout_ctr;
                // Check each syntax rule
                for ( auto &rule : c_ctx.rules ) {
                    bool use_bias =
                        ( best_rule ? ( rule.prec_bias != NO_BIAS_VALUE && best_rule->prec_bias != NO_BIAS_VALUE &&
                                        rule.prec_bias != best_rule->prec_bias )
                                    : false ); // helper variable
                    if ( !best_rule || ( !use_bias && rule.precedence <= best_rule->precedence ) ||
                         ( use_bias && rule.prec_bias < best_rule->prec_bias ) ) {
                        u8 rule_length = rule.expr_list.size();

                        // Prepare backtracing
                        std::vector<AstNode> rev_deep_expr_list;
                        std::vector<AstNode> stst_set;
                        size_t cutout_ctr = 0;
                        for ( auto expr_itr = expr_list->first.rbegin();
                              expr_itr != expr_list->first.rend() && rev_deep_expr_list.size() < rule_length;
                              expr_itr++ ) {
                            if ( expr_itr->type == ExprType::static_statement ) { // is a static statement
                                stst_set.push_back( *expr_itr );
                            } else {
                                if ( cutout_ctr >= skip_ctr && rev_deep_expr_list.size() < rule_length &&
                                     expr_itr->has_prop( ExprProperty::separable ) &&
                                     ( rule.precedence < expr_itr->precedence ||
                                       ( !rule.ltr && rule.precedence == expr_itr->precedence ) ) ) {
                                    // Split expr
                                    expr_itr->split_prepend_recursively( rev_deep_expr_list, stst_set, rule.precedence,
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
                    result_expr.static_statements = best_rule_stst_set;

                    if ( result_expr.has_prop( ExprProperty::separable ) &&
                         update_precedence_to_path ) { // path precedence overwrites normal precedence
                        result_expr.precedence = best_rule->prec_class.first;
                    }

                    expr_list->first.push_back( result_expr );

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

    auto ending_token = input->get_token(); // consume ending token

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
        auto block = AstNode{ ExprType::decl_scope };
        block.generate_new_props();
        block.children = expr_list;
        return block;
    } else if ( end_token == TT::block_end ) {
        PosInfo pos_info = PosInfo{ last_token->file, last_token->line, last_token->column, last_token->length };
        if ( expr_list.size() == 1 && expr_list.front().type == ExprType::comma_list ) { // set
            auto block = AstNode{ ExprType::set };
            block.generate_new_props();
            block.pos_info = pos_info;
            block.children = expr_list.front().children;
            return block;
        } else { // block
            auto block = AstNode{ ExprType::block };
            block.generate_new_props();
            block.pos_info = pos_info;
            block.children = expr_list;
            return block;
        }
    } else if ( end_token == TT::term_end ) {
        if ( expr_list.size() > 1 ) { // malformed "tuple" or so
            w_ctx.print_msg<MessageType::err_term_with_multiple_expr>(
                MessageInfo( *( expr_list.begin() + 1 ), 0, FmtStr::Color::Red ) );
            auto block = AstNode{ ExprType::tuple }; // default
            block.generate_new_props();
            return block;
        } else { // normal term or tuple
            PosInfo pos_info = merge_pos_infos(
                PosInfo{ last_token->file, last_token->line, last_token->column, last_token->length },
                PosInfo{ ending_token.file, ending_token.line, ending_token.column, ending_token.length } );

            if ( expr_list.empty() ) { // Unit type
                auto block = AstNode{ ExprType::unit };
                block.generate_new_props();
                block.pos_info = pos_info;
                return block;
            } else if ( expr_list.front().type == ExprType::comma_list ) { // tuple
                auto block = AstNode{ ExprType::tuple };
                block.generate_new_props();
                block.pos_info = pos_info;
                block.children = expr_list.front().children;
                return block;
            } else { // term
                auto block = AstNode{ ExprType::term };
                block.generate_new_props();
                block.pos_info = pos_info;
                block.children = expr_list;
                return block;
            }
        }
    } else if ( end_token == TT::array_end ) {
        auto block = AstNode{ ExprType::array_specifier };
        block.generate_new_props();
        block.pos_info = merge_pos_infos(
            PosInfo{ last_token->file, last_token->line, last_token->column, last_token->length },
            PosInfo{ ending_token.file, ending_token.line, ending_token.column, ending_token.length } );
        block.children = expr_list;
        return block;
    } else {
        LOG_ERR( "Try to parse a block which is no block" );
        return AstNode{ ExprType::none };
    }
}

void load_base_types( CrateCtx &c_ctx, Worker &w_ctx, PreludeConfig &cfg ) {
    // Internal types
    c_ctx.type_type = create_new_internal_type( c_ctx, w_ctx );
    c_ctx.struct_type = create_new_internal_type( c_ctx, w_ctx );
    c_ctx.trait_type = create_new_internal_type( c_ctx, w_ctx );
    c_ctx.fn_type = create_new_internal_type( c_ctx, w_ctx );
    c_ctx.template_struct_type = create_new_internal_type( c_ctx, w_ctx );
    c_ctx.template_fn_type = create_new_internal_type( c_ctx, w_ctx );
    c_ctx.mod_type = create_new_internal_type( c_ctx, w_ctx );

    // Most basic types/traits
    SymbolId new_symbol = create_new_global_symbol_from_name_chain(
        c_ctx, w_ctx, make_shared<std::vector<SymbolIdentifier>>( 1, SymbolIdentifier{ "()" } ) );
    c_ctx.unit_type = create_new_type( c_ctx, w_ctx, new_symbol );

    new_symbol = create_new_global_symbol_from_name_chain(
        c_ctx, w_ctx, split_symbol_chain( cfg.integer_trait, cfg.scope_access_operator ) );
    c_ctx.int_type = create_new_type( c_ctx, w_ctx, new_symbol );

    new_symbol = create_new_global_symbol_from_name_chain(
        c_ctx, w_ctx, split_symbol_chain( cfg.string_trait, cfg.scope_access_operator ) );
    c_ctx.str_type = create_new_type( c_ctx, w_ctx, new_symbol );

    new_symbol = create_new_global_symbol_from_name_chain(
        c_ctx, w_ctx, split_symbol_chain( cfg.tuple_trait, cfg.scope_access_operator ) );
    c_ctx.tuple_type = create_new_type( c_ctx, w_ctx, new_symbol );
    c_ctx.symbol_graph[new_symbol].identifier.template_values.push_back(
        std::make_pair( c_ctx.type_type, ConstValue() ) ); // Make it a template TODO make variadic

    new_symbol = create_new_global_symbol_from_name_chain(
        c_ctx, w_ctx, split_symbol_chain( cfg.array_trait, cfg.scope_access_operator ) );
    c_ctx.array_type = create_new_type( c_ctx, w_ctx, new_symbol );
    c_ctx.symbol_graph[new_symbol].identifier.template_values.push_back(
        std::make_pair( c_ctx.type_type, ConstValue() ) ); // Make it a template TODO make variadic

    new_symbol = create_new_global_symbol_from_name_chain(
        c_ctx, w_ctx, split_symbol_chain( cfg.iterator_trait, cfg.scope_access_operator ) );
    c_ctx.iterator_type = create_new_type( c_ctx, w_ctx, new_symbol );

    // Most basic functions
    new_symbol = create_new_global_symbol_from_name_chain(
        c_ctx, w_ctx, split_symbol_chain( cfg.drop_fn, cfg.scope_access_operator ) );
    create_new_type( c_ctx, w_ctx, new_symbol );
    c_ctx.drop_fn.push_back( new_symbol );

    new_symbol = create_new_global_symbol_from_name_chain(
        c_ctx, w_ctx, split_symbol_chain( cfg.equals_fn, cfg.scope_access_operator ) );
    c_ctx.equals_fn = create_new_type( c_ctx, w_ctx, new_symbol );

    new_symbol = create_new_global_symbol_from_name_chain(
        c_ctx, w_ctx, split_symbol_chain( cfg.itr_valid_fn, cfg.scope_access_operator ) );
    c_ctx.itr_valid_fn = create_new_type( c_ctx, w_ctx, new_symbol );

    new_symbol = create_new_global_symbol_from_name_chain(
        c_ctx, w_ctx, split_symbol_chain( cfg.itr_get_fn, cfg.scope_access_operator ) );
    c_ctx.itr_get_fn = create_new_type( c_ctx, w_ctx, new_symbol );

    new_symbol = create_new_global_symbol_from_name_chain(
        c_ctx, w_ctx, split_symbol_chain( cfg.itr_next_fn, cfg.scope_access_operator ) );
    c_ctx.itr_next_fn = create_new_type( c_ctx, w_ctx, new_symbol );

    // Memblob types
    for ( auto &mbt : cfg.memblob_types ) {
        new_symbol = create_new_global_symbol_from_name_chain(
            c_ctx, w_ctx, split_symbol_chain( mbt.first, cfg.scope_access_operator ) );
        TypeId new_type = create_new_type( c_ctx, w_ctx, new_symbol );
        c_ctx.type_table[new_type].additional_mem_size = mbt.second;
    }

    // Literals
    for ( auto &lit : cfg.literals ) {
        SymbolId type_symbol = find_global_symbol_by_identifier_chain(
                                   c_ctx, w_ctx, split_symbol_chain( lit.second.first, cfg.scope_access_operator ) )
                                   .front();
        c_ctx.literals_map[lit.first] = std::make_pair( type_symbol, lit.second.second );
    }
}

void get_ast( JobsBuilder &jb, UnitCtx &parent_ctx ) {
    jb.add_job<sptr<CrateCtx>>(
        []( Worker &w_ctx ) { return w_ctx.do_query( parse_ast )->jobs.back()->to<sptr<CrateCtx>>(); } );
}

void parse_ast( JobsBuilder &jb, UnitCtx &parent_ctx ) {
    jb.add_job<sptr<CrateCtx>>( []( Worker &w_ctx ) -> sptr<CrateCtx> {
        auto start_time = std::chrono::system_clock::now();
        auto input = get_source_input( w_ctx.unit_ctx()->root_file, w_ctx );
        if ( !input )
            return nullptr;

        select_prelude( *input, w_ctx );

        // Create a new crate context
        sptr<CrateCtx> c_ctx = make_shared<CrateCtx>();
        load_base_types( *c_ctx, w_ctx, w_ctx.unit_ctx()->prelude_conf );
        load_syntax_rules( w_ctx, *c_ctx );

        // parse global scope
        *c_ctx->ast = parse_scope( input, w_ctx, *c_ctx, TT::eof, nullptr );
        w_ctx.do_query( parse_symbols, c_ctx );

        // DEBUG print AST
        log( "AST ------------" );
        log( " " + c_ctx->ast->get_debug_repr() );
        log( "SYMBOLS --------" );
        for ( size_t i = 1; i < c_ctx->symbol_graph.size(); i++ ) {
            log( " " + to_string( i ) + " - " + get_full_symbol_name( *c_ctx, w_ctx, i ) + " - val " +
                 to_string( c_ctx->symbol_graph[i].value ) + " - type " + to_string( c_ctx->symbol_graph[i].type ) );
        }
        log( "TYPES ----------" );
        for ( size_t i = 1; i < c_ctx->type_table.size(); i++ ) {
            auto type = c_ctx->type_table[i];
            log( " " + to_string( i ) + " add_size " + to_string( type.additional_mem_size ) + " - sym " +
                 get_full_symbol_name( *c_ctx, w_ctx, type.symbol ) );
            for ( const auto &m : c_ctx->type_table[i].members ) {
                log( "  member " + m.identifier.name );
            }
        }
        log( "----------------" );
        auto duration = std::chrono::system_clock::now() - start_time;
        log( "Took " + to_string( duration.count() / 1000000 ) + " milliseconds" );

        return c_ctx;
    } );
}

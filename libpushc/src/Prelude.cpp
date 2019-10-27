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
#include "libpushc/Prelude.h"
#include "libpushc/Util.h"

// Returns a special prelude used to load a prelude file
PreludeConfig get_prelude_prelude() {
    PreludeConfig pc;
    pc.is_prelude = true;
    pc.is_prelude_library = false;
    pc.token_conf = TokenConfig::get_prelude_cfg();

    pc.spaces_bind_identifiers = false;
    pc.function_case = IdentifierCase::snake;
    pc.method_case = IdentifierCase::snake;
    pc.variable_case = IdentifierCase::snake;
    pc.module_case = IdentifierCase::snake;
    pc.struct_case = IdentifierCase::pascal;
    pc.trait_case = IdentifierCase::pascal;
    pc.unused_prefix.clear();
    pc.string_rules.push_back( StringRule{ "\"", "\"", "", "", "", true, false, true } );

    pc.fn_declarations.clear();
    pc.fn_definitions.clear();

    pc.scope_access_op.clear();
    // pc.scope_access_op.push_back( Operator{1, true,Syntax{ std::make_pair( "expr", "base" ), std::make_pair( "::", ""
    // ), std::make_pair( "expr", "name" ) } } );
    pc.member_access_op.clear();

    pc.operators.clear();
    pc.reference_op.clear();
    pc.type_of_op.clear();
    pc.struct_to_tuple_op.clear();
    pc.type_op.clear();
    pc.range_op.clear();

    pc.special_types.clear();
    pc.memblob_types.clear();
    pc.literals.clear();

    return pc;
}

void load_prelude( sptr<String> prelude, JobsBuilder &jb, UnitCtx &ctx ) {
    jb.add_job<PreludeConfig>( [prelude]( Worker &w_ctx ) {
        auto ctx = w_ctx.unit_ctx();
        PreludeConfig prelude_conf;
        if ( *prelude == "prelude" ) {
            prelude_conf = get_prelude_prelude();
        } else {
            sptr<String> filepath = get_std_dir();
            if ( *prelude == "push" ) {
                ( *filepath ) += "/prelude/push.push";
            } else if ( *prelude == "project" ) {
                ( *filepath ) += "/prelude/project.push";
            } else {
                w_ctx.print_msg<MessageType::err_invalid_prelude>( MessageInfo() );
            }
            ctx->prelude_conf = get_prelude_prelude();
            prelude_conf = *w_ctx.do_query( load_prelude_file, filepath )->jobs.front()->to<sptr<PreludeConfig>>();
        }

        return prelude_conf;
    } );
}

// Extract mci rule into a PreludeConfig
bool parse_mci_rule( sptr<PreludeConfig> &conf, sptr<SourceInput> &input, Worker &w_ctx );

void load_prelude_file( sptr<String> path, JobsBuilder &jb, UnitCtx &ctx ) {
    jb.add_job<sptr<PreludeConfig>>( [path]( Worker &w_ctx ) {
        auto input = get_source_input( path, w_ctx );
        input->configure( w_ctx.unit_ctx()->prelude_conf.token_conf );
        auto conf = make_shared<PreludeConfig>();
        bool parse_error = false;

        while ( true ) {
            auto token = input->preview_token();
            if ( token.type == Token::Type::eof ) {
                break;
            } else if ( token.type == Token::Type::comment_begin ) {
                size_t comment_count = 0;
                consume_comment( input, w_ctx.unit_ctx()->prelude_conf.token_conf );
            } else if ( token.type == Token::Type::identifier ) {
                if ( token.content == "define_mci_rule" ) {
                    if ( !parse_mci_rule( conf, input, w_ctx ) )
                        parse_error = true;
                } else { // Not allowed identifier
                    w_ctx.print_msg<MessageType::err_not_allowed_token_in_prelude>(
                        MessageInfo( path, token.line, token.line, token.column, token.length, 0,
                                     FmtStr::Color::BoldRed ),
                        {}, token.content );
                }
            } else { // Not allowed token
                w_ctx.print_msg<MessageType::err_not_allowed_token_in_prelude>(
                    MessageInfo( path, token.line, token.line, token.column, token.length, 0, FmtStr::Color::BoldRed ),
                    {}, token.content );
            }
            if ( parse_error ) { // any error occurred
                w_ctx.print_msg<MessageType::ferr_failed_prelude>( MessageInfo(), {}, *path );
            }
        }
        return conf;
    } );
}


void create_prelude_error_msg( Worker &w_ctx, const Token &token ) {
    w_ctx.print_msg<MessageType::err_parse_mci_rule>(
        MessageInfo( token.file, token.line, token.line, token.column, token.length, 0, FmtStr::Color::BoldRed ), {} );
}

void create_not_supported_error_msg( Worker &w_ctx, const Token &token, const String &feature_description ) {
    w_ctx.print_msg<MessageType::err_feature_curr_not_supported>(
        MessageInfo( token.file, token.line, token.line, token.column, token.length, 0, FmtStr::Color::BoldRed ), {},
        feature_description );
}

// Translate strings like "semicolon" into ";"
String parse_string_literal( sptr<SourceInput> &input, Worker &w_ctx ) {
    auto token = input->preview_token();
    if ( token.type == Token::Type::string_begin ) { // regular string
        return parse_string( input, w_ctx );
    } else if ( token.type == Token::Type::identifier ) { // named string
        input->get_token(); // consume
        if ( token.content == "semicolon" )
            return ";";
        else if ( token.content == "left_brace" )
            return "{";
        else if ( token.content == "right_brace" )
            return "}";
        else if ( token.content == "left_parenthesis" )
            return "(";
        else if ( token.content == "right_parenthesis" )
            return ")";
        else if ( token.content == "newline" )
            return "\n";
        else if ( token.content == "horizontal_tab" )
            return "\t";
        else if ( token.content == "vertical_tab" )
            return "\v";
        else if ( token.content == "carriage_return" )
            return "\r";
        else if ( token.content == "backslash" )
            return "\\";
        else if ( token.content == "quote" )
            return "\'";
        else if ( token.content == "double_quotes" )
            return "\"";
        else if ( token.content == "null" )
            return String( 1, '\0' );
        else if ( token.content == "tree_double_quotes" )
            return "\"\"\"";
        else if ( token.content == "operators" || token.content == "keywords" || token.content == "ascii_oct" ||
                  token.content == "ascii_hex" || token.content == "unicode_32_hex" ) // special identifiers
            return "\x2" + token.content;
        else {
            create_prelude_error_msg( w_ctx, token );
            return "";
        }
    } else {
        create_prelude_error_msg( w_ctx, token );
        return "";
    }
}

// Returns the size of a syntax list
size_t parse_list_size( sptr<SourceInput> &input ) {
    auto token = input->get_token();
    if ( token.content == "single_list" )
        return 1;
    else if ( token.content == "double_list" )
        return 2;
    else if ( token.content == "triple_list" )
        return 3;
    else if ( token.content == "quadruple_list" )
        return 4;
    else if ( token.content == "quintuple_list" )
        return 5;
    else if ( token.content == "sextuple_list" )
        return 6;
    else
        return 0;
}

// Parses a syntax definition and adds keywords or operators to the prelude configuration. Returns true if was
// successful.
bool parse_syntax( Syntax &output, sptr<PreludeConfig> &conf, size_t list_size, sptr<SourceInput> &input,
                   Worker &w_ctx ) {
    for ( size_t i = 0; i < list_size; i++ ) {
        auto token = input->preview_token();
        String type; // or operator/keyword
        if ( token.type == Token::Type::string_begin ) {
            type = parse_string( input, w_ctx );

            if ( is_operator_token( type ) )
                conf->token_conf.operators.push_back( type );
            else
                conf->token_conf.keywords.push_back( type );
        } else {
            auto token = input->get_token();
            type = token.content;
            if ( token.type != Token::Type::identifier ) {
                create_prelude_error_msg( w_ctx, token );
                return false;
            }
        }

        token = input->preview_token();
        if ( token.type == Token::Type::op && token.content == "->" ) { // pair
            input->get_token(); // consume
            token = input->get_token();
            if ( token.type != Token::Type::identifier ) {
                create_prelude_error_msg( w_ctx, token );
                return false;
            }

            output.push_back( std::make_pair( type, token.content ) );
        } else { // single string
            output.push_back( std::make_pair( type, String() ) );
        }

        if ( i + 1 < list_size ) {
            token = input->get_token();
            if ( token.content != "," ) { // consume comma
                create_prelude_error_msg( w_ctx, token );
                return false;
            }
        }
    }
    return true;
}


#define CONSUME_COMMA( token )                    \
    token = input->get_token();                   \
    if ( token.content != "," ) {                 \
        create_prelude_error_msg( w_ctx, token ); \
        return false;                             \
    }



// Parses a simple operator definition and adds keywords or operators to the prelude configuration. Returns true if was
// successful.
bool parse_operator( Operator &output, sptr<PreludeConfig> &conf, sptr<SourceInput> &input, Worker &w_ctx ) {
    // Precedence
    output.precedence = parse_number( input, w_ctx );
    Token token;
    CONSUME_COMMA( token );

    // Alignment
    token = input->get_token();
    if ( token.type != Token::Type::identifier && token.content != "ltr" && token.content != "rtl" ) {
        create_prelude_error_msg( w_ctx, token );
        return false;
    }
    output.ltr = token.content == "ltr";
    CONSUME_COMMA( token );

    // Syntax
    size_t list_size = parse_list_size( input );
    CONSUME_COMMA( token );

    if ( !parse_syntax( output.syntax, conf, list_size, input, w_ctx ) )
        return false;

    // TODO add keywords and operators to prelude config

    // Alias
    token = input->preview_token();
    if ( token.content == "," ) {
        input->get_token(); // consume

        token = input->get_token();
        if ( token.type != Token::Type::identifier ) {
            create_prelude_error_msg( w_ctx, token );
            return false;
        }

        output.aliases.push_back( token.content );
        // conf->subtypes[token.content].push_back( output ); TODO del subtypes
    }
    return true;
}

bool parse_mci_rule( sptr<PreludeConfig> &conf, sptr<SourceInput> &input, Worker &w_ctx ) {
    auto filename = input->get_filename();

    auto token = input->get_token();
    // function
    if ( token.type != Token::Type::identifier || token.content != "define_mci_rule" ) {
        create_prelude_error_msg( w_ctx, token );
        return false;
    }
    token = input->get_token();
    // parenthesis
    if ( token.type != Token::Type::term_begin ) {
        create_prelude_error_msg( w_ctx, token );
        return false;
    }

    token = input->get_token();
    // MCI
    if ( token.type != Token::Type::identifier ) {
        create_prelude_error_msg( w_ctx, token );
        return false;
    }
    String mci = token.content;

    token = input->preview_token();
    // first comma
    if ( token.type != Token::Type::op || token.content != "," ) {
        create_prelude_error_msg( w_ctx, token );
        return false;
    }

    // Iterate MCI parameters
    while ( token.type == Token::Type::op && token.content == "," ) {
        token = input->get_token(); // consume comma

// Using this here because the code occurs soo often and cannot be reasonably extracted into a function
#define PARSE_LITERAL( str )                         \
    auto str = parse_string_literal( input, w_ctx ); \
    if ( str.empty() )                               \
    return false

        // Find and handle MCI content
        if ( mci == "EXPRESSION_RULES" ) {
            token = input->get_token();
            if ( token.content == "divide" ) {
                PARSE_LITERAL( str );
                conf->token_conf.stat_divider.push_back( str );
            } else if ( token.content == "block" ) {
                PARSE_LITERAL( str );
                PARSE_LITERAL( str2 );
                conf->token_conf.block.push_back( std::make_pair( str, str2 ) );
            } else if ( token.content == "term" ) {
                PARSE_LITERAL( str );
                PARSE_LITERAL( str2 );
                conf->token_conf.term.push_back( std::make_pair( str, str2 ) );
            } else { // Unknown token
                create_prelude_error_msg( w_ctx, token );
                return false;
            }
        } else if ( mci == "ALLOWED_CHARS" ) {
            token = input->get_token();
            if ( token.content == "any_unicode" ) {
                conf->token_conf.allowed_chars = std::make_pair( 0, 0xffffffff );
            }
        } else if ( mci == "IDENTIFIER_RULES" ) {
            token = input->get_token();
            if ( token.content == "no_spaces" ) {
                conf->spaces_bind_identifiers = false;
            } else if ( token.content == "spaces" ) {
                conf->spaces_bind_identifiers = true;
            } else if ( token.content == "unused" ) {
                if ( input->get_token().content != "begin" ) {
                    create_not_supported_error_msg( w_ctx, token, "Unused variable not with prefix." );
                    return false;
                }
                PARSE_LITERAL( str );
                conf->unused_prefix.push_back( str );
            }
        } else if ( mci == "IDENTIFIER_CASE" ) {
            token = input->get_token();
            if ( token.type != Token::Type::identifier ) {
                create_prelude_error_msg( w_ctx, token );
                return false;
            }
            String obj = token.content;
            token = input->get_token();
            if ( token.type != Token::Type::identifier ) {
                create_prelude_error_msg( w_ctx, token );
                return false;
            }

            auto i_case = token.content == "snake"
                              ? IdentifierCase::snake
                              : token.content == "pascal"
                                    ? IdentifierCase::pascal
                                    : token.content == "camel" ? IdentifierCase::camel : IdentifierCase::count;

            if ( obj == "functions" )
                conf->function_case = i_case;
            else if ( obj == "method" )
                conf->method_case = i_case;
            else if ( obj == "variable" )
                conf->variable_case = i_case;
            else if ( obj == "module" )
                conf->module_case = i_case;
            else if ( obj == "struct" )
                conf->struct_case = i_case;
            else if ( obj == "trait" )
                conf->trait_case = i_case;
        } else if ( mci == "LITERAL_CHARACTER_ESCAPES" ) {
            PARSE_LITERAL( str );
            PARSE_LITERAL( str2 );
            conf->token_conf.char_escapes[str] = str2;
        } else if ( mci == "NEW_RANGE" ) {
            token = input->get_token();
            if ( token.type != Token::Type::identifier ) {
                create_prelude_error_msg( w_ctx, token );
                return false;
            }
            String str = token.content;
            auto rt = str == "identifier"
                          ? CharRangeType::identifier
                          : str == "operator"
                                ? CharRangeType::op
                                : str == "integer"
                                      ? CharRangeType::integer
                                      : str == "whitespace" ? CharRangeType::ws
                                                            : str == "opt_identifier" ? CharRangeType::opt_identifier
                                                                                      : CharRangeType::count;
            while ( input->preview_token().type != Token::Type::term_end ) {
                CONSUME_COMMA( token );
                PARSE_LITERAL( str );
                token = input->preview_token();
                if ( token.type != Token::Type::term_end && token.content != "," ) {
                    PARSE_LITERAL( str2 );
                    conf->token_conf.char_ranges[rt].push_back( std::make_pair( str[0], str2[0] ) );
                } else {
                    // only one token
                    conf->token_conf.char_ranges[rt].push_back( std::make_pair( str[0], str[0] ) );
                }
            }
        } else if ( mci == "NEW_LEVEL" ) {
            token = input->get_token();
            TokenLevel level = token.content == "NORMAL"
                                   ? TokenLevel::normal
                                   : token.content == "COMMENT"
                                         ? TokenLevel::comment
                                         : token.content == "COMMENT_LINE"
                                               ? TokenLevel::comment_line
                                               : token.content == "STRING" ? TokenLevel::string : TokenLevel::count;
            CONSUME_COMMA( token );
            token = input->get_token();
            String name = token.content;

            StringRule string_rule; // optional
            while ( input->preview_token().content == "," ) {
                input->get_token(); // consume
                token = input->preview_token();
                if ( token.content == "overlay" ) {
                    input->get_token(); // consume
                    do {
                        token = input->get_token();
                        auto begin = ( conf->token_conf.level_map[level].find( name ) ==
                                               conf->token_conf.level_map[level].end()
                                           ? ""
                                           : conf->token_conf.level_map[level][name]
                                                 .begin_token ); // find begin token of normal (if any)
                        conf->token_conf.allowed_level_overlay[begin].push_back( token.content );
                        token = input->preview_token();
                    } while ( token.type != Token::Type::term_end && token.content != "," );
                } else if ( token.content == "prefix" ) {
                    // Not allowed if not a string rule
                    if ( level != TokenLevel::string ) {
                        create_prelude_error_msg( w_ctx, token );
                        return false;
                    }

                    input->get_token(); // consume
                    PARSE_LITERAL( str );
                    string_rule.prefix = str;
                } else if ( token.content == "rep_delimiter" ) {
                    // Not allowed if not a string rule
                    if ( level != TokenLevel::string ) {
                        create_prelude_error_msg( w_ctx, token );
                        return false;
                    }

                    input->get_token(); // consume
                    PARSE_LITERAL( str );
                    PARSE_LITERAL( str2 );
                    string_rule.rep_begin = str;
                    string_rule.rep_end = str2;
                } else { // normal begin and end delimiter
                    PARSE_LITERAL( str );
                    PARSE_LITERAL( str2 );
                    string_rule.begin = str;
                    string_rule.end = str;
                    conf->token_conf.level_map[level][name] = { str, str2 };
                }
            }
            if ( level == TokenLevel::string ) {
                conf->string_rules.push_back( string_rule );
            }
        } else if ( mci == "ALIAS_EXPRESSION" ) { // TODO
        } else if ( mci == "LET_STATEMENT" ) { // TODO
        } else if ( mci == "SELF_EXPRESSION" ) { // TODO
        } else if ( mci == "STRUCT_DEFINITION" ) { // TODO
        } else if ( mci == "TRAIT_DEFINITION" ) { // TODO
        } else if ( mci == "IMPL_DEFINITION" ) { // TODO
        } else if ( mci == "IF_EXPRESSION" ) { // TODO
        } else if ( mci == "IF_ELSE_EXPRESSION" ) { // TODO
        } else if ( mci == "WHILE_EXPRESSION" ) { // TODO
        } else if ( mci == "FOR_EXPRESSION" ) { // TODO
        } else if ( mci == "MATCH_EXPRESSION" ) { // TODO
        } else if ( mci == "FUNCTION_DECLARATION" ) {
            token = input->get_token();
            if ( token.type != Token::Type::identifier ) {
                create_prelude_error_msg( w_ctx, token );
                return false;
            }
            auto trait = token.content;
            CONSUME_COMMA( token );

            token = input->get_token();
            if ( token.type != Token::Type::identifier ) {
                create_prelude_error_msg( w_ctx, token );
                return false;
            }
            auto function = token.content;
            CONSUME_COMMA( token );

            Syntax syntax;
            auto list_size = parse_list_size( input );
            CONSUME_COMMA( token );

            if ( !parse_syntax( syntax, conf, list_size, input, w_ctx ) ) {
                return false;
            }

            conf->fn_declarations.push_back( FunctionDefinition{ trait, function, syntax } );
        } else if ( mci == "FUNCTION_DEFINITION" ) {
            token = input->get_token();
            if ( token.type != Token::Type::identifier ) {
                create_prelude_error_msg( w_ctx, token );
                return false;
            }
            auto trait = token.content;
            CONSUME_COMMA( token );

            token = input->get_token();
            if ( token.type != Token::Type::identifier ) {
                create_prelude_error_msg( w_ctx, token );
                return false;
            }
            auto function = token.content;
            CONSUME_COMMA( token );

            Syntax syntax;
            auto list_size = parse_list_size( input );
            CONSUME_COMMA( token );

            if ( !parse_syntax( syntax, conf, list_size, input, w_ctx ) ) {
                return false;
            }

            conf->fn_definitions.push_back( FunctionDefinition{ trait, function, syntax } );
        } else if ( mci == "DEFINE_TEMPLATE" ) { // TODO
        } else if ( mci == "SCOPE_ACCESS" ) {
            Operator op;
            if ( !parse_operator( op, conf, input, w_ctx ) ) {
                return false;
            }
            conf->scope_access_op.push_back( op );
        } else if ( mci == "MEMBER_ACCESS" ) {
            Operator op;
            if ( !parse_operator( op, conf, input, w_ctx ) ) {
                return false;
            }
            conf->member_access_op.push_back( op );
        } else if ( mci == "ARRAY_SPECIFIER" ) { // TODO
        } else if ( mci == "NEW_OPERATOR" ) {
            token = input->get_token();
            if ( token.type != Token::Type::identifier ) {
                create_prelude_error_msg( w_ctx, token );
                return false;
            }
            auto fn = token.content;

            CONSUME_COMMA( token );

            Operator op;
            parse_operator( op, conf, input, w_ctx );
            conf->operators.push_back( TraitOperator{ op, fn } );
        } else if ( mci == "REFERENCE_TYPE" ) {
            Operator op;
            if ( !parse_operator( op, conf, input, w_ctx ) ) {
                return false;
            }
            conf->reference_op.push_back( op );
        } else if ( mci == "TYPE_OF" ) {
            Operator op;
            if ( !parse_operator( op, conf, input, w_ctx ) ) {
                return false;
            }
            conf->type_of_op.push_back( op );
        } else if ( mci == "STRUCT_TO_TUPLE" ) {
            Operator op;
            if ( !parse_operator( op, conf, input, w_ctx ) ) {
                return false;
            }
            conf->struct_to_tuple_op.push_back( op );
        } else if ( mci == "OPERATION_TYPE" ) {
            Operator op;
            if ( !parse_operator( op, conf, input, w_ctx ) ) {
                return false;
            }
            conf->type_op.push_back( op );
        } else if ( mci == "RANGE_DEFINITION_EXC" || mci == "RANGE_DEFINITION_FROM_EXC" ||
                    mci == "RANGE_DEFINITION_TO_EXC" || mci == "RANGE_DEFINITION_INC" ||
                    mci == "RANGE_DEFINITION_TO_INC" ) {
            Operator op;
            if ( !parse_operator( op, conf, input, w_ctx ) ) {
                return false;
            }

            RangeOperator::Type type;
            if ( mci == "RANGE_DEFINITION_EXC" )
                type = RangeOperator::Type::exclude;
            else if ( mci == "RANGE_DEFINITION_FROM_EXC" )
                type = RangeOperator::Type::exclude_from;
            else if ( mci == "RANGE_DEFINITION_TO_EXC" )
                type = RangeOperator::Type::exclude_to;
            else if ( mci == "RANGE_DEFINITION_INC" )
                type = RangeOperator::Type::include;
            else if ( mci == "RANGE_DEFINITION_TO_INC" )
                type = RangeOperator::Type::include_to;
            else
                type = RangeOperator::Type::count; // never reached

            conf->range_op.push_back( RangeOperator{ type, op } );
        } else if ( mci == "SPECIAL_TYPE" ) {
            token = input->get_token();
            if ( token.type != Token::Type::identifier ) {
                create_prelude_error_msg( w_ctx, token );
                return false;
            }
            String intrinsic = token.content;
            CONSUME_COMMA( token );
            PARSE_LITERAL( str );
            conf->special_types[str] = intrinsic;
        } else if ( mci == "TYPE_MEMORY_BLOB" ) {
            PARSE_LITERAL( str );
            CONSUME_COMMA( token );
            if ( ( token = input->get_token() ).type != Token::Type::number ) {
                create_prelude_error_msg( w_ctx, token );
                return false;
            }
            conf->memblob_types[str] = stoi( token.content );
        } else if ( mci == "NEW_LITERAL" ) {
            PARSE_LITERAL( str );
            CONSUME_COMMA( token );
            PARSE_LITERAL( str2 );
            CONSUME_COMMA( token );
            auto val = parse_number( input, w_ctx );
            conf->literals[str] = std::make_pair( str2, val );
        } else { // Unknown MCI
            w_ctx.print_msg<MessageType::err_unknown_mci>(
                MessageInfo( input->get_filename(), token.line, token.line, token.column, token.length, 0,
                             FmtStr::Color::BoldRed ),
                {}, mci );
        }
        token = input->preview_token();
    }
#undef PARSE_LITERAL
    // parenthesis
    token = input->get_token(); // consume parenthesis
    if ( token.type != Token::Type::term_end ) {
        create_prelude_error_msg( w_ctx, token );
        return false;
    }
    token = input->get_token();
    // semicolon
    if ( token.type != Token::Type::stat_divider ) {
        create_prelude_error_msg( w_ctx, token );
        return false;
    }

    return true;
}

#undef CONSUME_COMMA

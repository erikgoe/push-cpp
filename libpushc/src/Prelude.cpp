// Copyright 2018 Erik Götzfried
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
    pc.token_conf.stat_divider.push_back( ";" );
    pc.token_conf.term.push_back( std::make_pair( "(", ")" ) );
    pc.token_conf.comment.push_back( std::make_pair( "/*", "*/" ) );
    pc.token_conf.comment.push_back( std::make_pair( "//", "\n" ) );
    pc.token_conf.comment.push_back( std::make_pair( "//", "\r" ) );
    pc.token_conf.nested_comments = false;
    pc.token_conf.allowed_chars = std::make_pair<u32, u32>( 0, 0xffffffff );
    pc.token_conf.nested_strings = false;
    pc.token_conf.char_escapes.push_back( std::make_pair( "\\n", "\n" ) );
    pc.token_conf.char_escapes.push_back( std::make_pair( "\\t", "\t" ) );
    pc.token_conf.char_escapes.push_back( std::make_pair( "\\v", "\v" ) );
    pc.token_conf.char_escapes.push_back( std::make_pair( "\\r", "\r" ) );
    pc.token_conf.char_escapes.push_back( std::make_pair( "\\\\", "\\" ) );
    pc.token_conf.char_escapes.push_back( std::make_pair( "\\\'", "\'" ) );
    pc.token_conf.char_escapes.push_back( std::make_pair( "\\\"", "\"" ) );
    pc.token_conf.char_escapes.push_back( std::make_pair( "\\0", "\0" ) );
    pc.token_conf.char_encodings.push_back( "\\o" );
    pc.token_conf.char_encodings.push_back( "\\x" );
    pc.token_conf.char_encodings.push_back( "\\u" );
    pc.token_conf.string.push_back( std::make_pair( "\"", "\"" ) );
    pc.token_conf.integer_prefix.push_back( "0o" );
    pc.token_conf.integer_prefix.push_back( "0b" );
    pc.token_conf.integer_prefix.push_back( "0h" );
    pc.token_conf.float_delimiter.push_back( "." );
    pc.token_conf.operators.push_back( "," );
    pc.token_conf.operators.push_back( "->" );
    pc.token_conf.operators.push_back( "#" );

    pc.exclude_operators = true;
    pc.exclude_keywords = true;
    pc.spaces_bind_identifiers = false;
    pc.function_case = IdentifierCase::snake;
    pc.method_case = IdentifierCase::snake;
    pc.variable_case = IdentifierCase::snake;
    pc.module_case = IdentifierCase::snake;
    pc.struct_case = IdentifierCase::pascal;
    pc.trait_case = IdentifierCase::pascal;
    pc.unused_prefix.clear();

    pc.ascii_octal_prefix.push_back( "\\o" );
    pc.ascii_hex_prefix.push_back( "\\x" );
    pc.unicode_hex_prefix.push_back( "\\u" );
    pc.truncate_unicode_hex_prefix = true;
    pc.string_rules.push_back( StringRule{ "\"", "\"", "", "", "", true, false, true } );
    pc.value_extract.clear();
    pc.allow_multi_value_extract = false;
    pc.char_rules.push_back( CharacterRule{ "\'", "\'", "", true, true } );

    pc.bin_int_prefix.push_back( "0b" );
    pc.oct_int_prefix.push_back( "0o" );
    pc.hex_int_prefix.push_back( "0x" );
    pc.kilo_int_delimiter.clear();
    pc.allow_int_type_postfix = false;
    pc.kilo_float_delimiter.clear();
    pc.fraction_delimiter.push_back( "." );
    pc.expo_delimiter.push_back( "E" );
    pc.expo_positive.push_back( "+" );
    pc.expo_negative.push_back( "-" );
    pc.allow_float_type_postfix = false;

    pc.fn_definitions.clear();

    pc.scope_access_op.clear();
    // pc.scope_access_op.push_back( Operator{1, true,Syntax{ std::make_pair( "expr", "base" ), std::make_pair( "::", ""
    // ), std::make_pair( "expr", "name" ) } } );
    pc.member_access_op.clear();

    pc.subtypes.clear();
    pc.operators.clear();
    pc.reference_op.clear();
    pc.type_of_op.clear();
    pc.struct_to_tuple_op.clear();
    pc.type_op.clear();
    pc.range_op.clear();

    pc.type_literals.clear();
    pc.bool_literals.clear();
    pc.int_literals.clear();
    pc.float_literals.clear();
    pc.range_literals.clear();

    return pc;
}

void load_prelude( std::shared_ptr<String> prelude, JobsBuilder &jb, UnitCtx &ctx ) {
    jb.add_job<PreludeConfig>( [prelude]( Worker &w_ctx ) {
        auto ctx = w_ctx.unit_ctx();
        PreludeConfig prelude_conf;
        if ( *prelude == "prelude" ) {
            prelude_conf = get_prelude_prelude();
        } else {
            std::shared_ptr<String> filepath = get_std_dir();
            if ( *prelude == "push" ) {
                ( *filepath ) += "/prelude/push.push";
            } else if ( *prelude == "project" ) {
                ( *filepath ) += "/prelude/project.push";
            }
            ctx->prelude_conf = get_prelude_prelude();
            prelude_conf = *w_ctx.do_query( load_prelude_file, filepath )
                                ->jobs.front()
                                ->to<std::shared_ptr<PreludeConfig>>();
        }

        return prelude_conf;
    } );
}

// Extract mci rule into a PreludeConfig
bool parse_mci_rule( std::shared_ptr<PreludeConfig> &conf, std::shared_ptr<SourceInput> &input, Worker &w_ctx );

void load_prelude_file( std::shared_ptr<String> path, JobsBuilder &jb, UnitCtx &ctx ) {
    jb.add_job<std::shared_ptr<PreludeConfig>>( [path]( Worker &w_ctx ) {
        auto input = get_source_input( *path, w_ctx );
        input->configure( w_ctx.unit_ctx()->prelude_conf.token_conf );
        auto conf = std::make_shared<PreludeConfig>();
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


void create_prelude_error_msg( Worker &w_ctx, Token &token, std::shared_ptr<String> &filename ) {
    w_ctx.print_msg<MessageType::err_parse_mci_rule>(
        MessageInfo( filename, token.line, token.line, token.column, token.length, 0, FmtStr::Color::BoldRed ), {} );
}

void create_not_supported_error_msg( Worker &w_ctx, Token &token, std::shared_ptr<String> &filename,
                                     const String &feature_description ) {
    w_ctx.print_msg<MessageType::err_feature_curr_not_supported>(
        MessageInfo( filename, token.line, token.line, token.column, token.length, 0, FmtStr::Color::BoldRed ), {},
        feature_description );
}

// Translate strings like "semicolon" in ";"
String parse_string_literal( std::shared_ptr<SourceInput> &input, Worker &w_ctx, std::shared_ptr<String> &filename ) {
    auto token = input->get_token();
    if ( token.type == Token::Type::string_begin ) { // regular string
        String ret = "";
        auto start_token = token; // save begin for error reporting
        token = input->preview_next_token();
        while ( token.type != Token::Type::string_end && token.type != Token::Type::eof ) {
            token = input->get_token();
            if ( !ret.empty() )
                ret += token.leading_ws + token.content;
            else
                ret += token.content;
            token = input->preview_token();
        }
        if ( token.type == Token::Type::string_end )
            input->get_token(); // consume
        if ( token.type == Token::Type::eof || ret.empty() ) { // string_end not found
            create_prelude_error_msg( w_ctx, start_token, filename );
            return "";
        }
        return ret;
    } else if ( token.type == Token::Type::identifier ) { // named string
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
            return String( "\0", 1 );
        else if ( token.content == "tree_double_quotes" )
            return "\"\"\"";
        else if ( token.content == "operators" || token.content == "keywords" || token.content == "ascii_oct" ||
                  token.content == "ascii_hex" || token.content == "unicode_32_hex" ) // special identifiers
            return "\x2" + token.content;
        else {
            create_prelude_error_msg( w_ctx, token, filename );
            return "";
        }
    } else {
        create_prelude_error_msg( w_ctx, token, filename );
        return "";
    }
}

bool parse_mci_rule( std::shared_ptr<PreludeConfig> &conf, std::shared_ptr<SourceInput> &input, Worker &w_ctx ) {
    auto filename = input->get_filename();

    auto token = input->get_token();
    // function
    if ( token.type != Token::Type::identifier || token.content != "define_mci_rule" ) {
        create_prelude_error_msg( w_ctx, token, filename );
        return false;
    }
    token = input->get_token();
    // parenthesis
    if ( token.type != Token::Type::term_begin ) {
        create_prelude_error_msg( w_ctx, token, filename );
        return false;
    }

    token = input->get_token();
    // MCI
    if ( token.type != Token::Type::identifier ) {
        create_prelude_error_msg( w_ctx, token, filename );
        return false;
    }
    String mci = token.content;

    token = input->preview_token();
    // first comma
    if ( token.type != Token::Type::op || token.content != "," ) {
        create_prelude_error_msg( w_ctx, token, filename );
        return false;
    }

    // Iterate MCI parameters
    while ( token.type == Token::Type::op && token.content == "," ) {
        token = input->get_token(); // consume comma

// Using this here because the code occurs soo often and cannot be reasonably extracted into a function
#define PARSE_LITERAL( str )                                   \
    auto str = parse_string_literal( input, w_ctx, filename ); \
    if ( str.empty() )                                         \
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
                create_prelude_error_msg( w_ctx, token, filename );
                return false;
            }
        } else if ( mci == "COMMENT_RULES" ) {
            token = input->get_token();
            if ( token.content == "block" || token.content == "block_doc" ) {
                PARSE_LITERAL( str );
                PARSE_LITERAL( str2 );
                conf->token_conf.comment.push_back( std::make_pair( str, str2 ) );
            } else if ( token.content == "line" || token.content == "line_doc" ) {
                PARSE_LITERAL( str );
                conf->token_conf.comment.push_back( std::make_pair( str, "\n" ) );
                conf->token_conf.comment.push_back( std::make_pair( str, "\r" ) );
            } else if ( token.content == "nested_blocks_allowed" ) {
                conf->token_conf.nested_comments = true;
            } else if ( token.content == "nested_blocks_disallowed" ) {
                conf->token_conf.nested_comments = false;
            } else { // Unknown token
                create_prelude_error_msg( w_ctx, token, filename );
                return false;
            }
        } else if ( mci == "IDENTIFIER_RULES" ) {
            token = input->get_token();
            if ( token.content == "any_unicode" ) {
                conf->token_conf.allowed_chars = std::make_pair( 0, 0xffffffff );
            } else if ( token.content == "exclude" ) {
                do {
                    PARSE_LITERAL( str );
                    if ( str == "\x2operators" )
                        conf->exclude_operators = true;
                    else if ( str == "\x2keywords" )
                        conf->exclude_keywords = true;
                    else { // exclude specific words
                        create_not_supported_error_msg( w_ctx, token, filename,
                                                        "Exclude arbritray words from possible identifiers set." );
                        return false;
                    }
                } while ( input->preview_token().content == "or" && /*consume*/ input->get_token().column );
            } else if ( token.content == "no_spaces" ) {
                conf->spaces_bind_identifiers = false;
            } else if ( token.content == "spaces" ) {
                conf->spaces_bind_identifiers = true;
            } else if ( token.content == "case" ) {
                token = input->preview_token();
                if ( token.type != Token::Type::identifier ) { // sub parameters may not be empty
                    create_prelude_error_msg( w_ctx, token, filename );
                    return false;
                }
                while ( token.content != "," ) {
                    token = input->get_token();
                    if ( token.type != Token::Type::identifier ) {
                        create_prelude_error_msg( w_ctx, token, filename );
                        return false;
                    }
                    auto token2 = input->get_token();
                    if ( token2.type != Token::Type::identifier ) {
                        create_prelude_error_msg( w_ctx, token2, filename );
                        return false;
                    }
                    IdentifierCase i_case =
                        token2.content == "snake"
                            ? IdentifierCase::snake
                            : token2.content == "pascal"
                                  ? IdentifierCase::pascal
                                  : token2.content == "camel" ? IdentifierCase::camel : IdentifierCase::count;
                    if ( i_case == IdentifierCase::count ) {
                        create_prelude_error_msg( w_ctx, token2, filename );
                        return false;
                    }
                    if ( token.content == "functions" )
                        conf->function_case = i_case;
                    else if ( token.content == "method" )
                        conf->method_case = i_case;
                    else if ( token.content == "variable" )
                        conf->variable_case = i_case;
                    else if ( token.content == "module" )
                        conf->module_case = i_case;
                    else if ( token.content == "struct" )
                        conf->struct_case = i_case;
                    else if ( token.content == "trait" )
                        conf->trait_case = i_case;
                    token = input->preview_token();
                }
            } else if ( token.content == "unused" ) {
                if ( input->get_token().content != "begin" ) {
                    create_not_supported_error_msg( w_ctx, token, filename, "Unused variable not with prefix." );
                    return false;
                }
                PARSE_LITERAL( str );
                conf->unused_prefix.push_back( str );
            } else { // Unknown token
                create_prelude_error_msg( w_ctx, token, filename );
                return false;
            }
        } else if ( mci == "LITERAL_CHARACTER_ESCAPES" ) {
            PARSE_LITERAL( str );
            PARSE_LITERAL( str2 );
            if ( str.empty() ) {
                return false;
            } else if ( str == "\x2ascii_oct" ) {
                conf->ascii_octal_prefix.push_back( str2 );
                conf->token_conf.char_encodings.push_back( str2 );
            } else if ( str == "\x2ascii_hex" ) {
                conf->ascii_hex_prefix.push_back( str2 );
                conf->token_conf.char_encodings.push_back( str2 );
            } else if ( str == "\x2unicode_32_hex" ) {
                conf->unicode_hex_prefix.push_back( str2 );
                conf->token_conf.char_encodings.push_back( str2 );
            } else {
                conf->token_conf.char_escapes.push_back( std::make_pair( str, str2 ) );
            }
            if ( input->preview_token().content != "," ) {
                if ( str != "\x2unicode_32_hex" ) {
                    create_not_supported_error_msg(
                        w_ctx, input->get_token(), filename,
                        "Additional attributes for char encodings other than unicode_32_hex." );
                } else {
                    token = input->get_token();
                    if ( token.content == "truncate_leading" ) {
                        conf->truncate_unicode_hex_prefix = true;
                    } else {
                        create_prelude_error_msg( w_ctx, token, filename );
                        return false;
                    }
                }
            }
        } else if ( mci == "LITERAL_STRING_RULES" ) {
            token = input->get_token();
            if ( token.content == "basic_escaped" ) {
                PARSE_LITERAL( str );
                PARSE_LITERAL( str2 );
                conf->token_conf.string.push_back( std::make_pair( str, str2 ) );
                conf->string_rules.push_back( StringRule{ str, str2, "", "", "", true, false, true } );
            } else if ( token.content == "block_escaped" ) {
                PARSE_LITERAL( str );
                PARSE_LITERAL( str2 );
                conf->token_conf.string.push_back( std::make_pair( str, str2 ) );
                conf->string_rules.push_back( StringRule{ str, str2, "", "", "", true, true, true } );
            } else if ( token.content == "wide_escaped" ) {
                PARSE_LITERAL( str );
                PARSE_LITERAL( str2 );
                token = input->get_token();
                if ( token.content != "prefix" ) {
                    create_prelude_error_msg( w_ctx, token, filename );
                    return false;
                }
                PARSE_LITERAL( str3 );
                conf->token_conf.string.push_back( std::make_pair( str, str2 ) );
                conf->string_rules.push_back( StringRule{ str, str2, str3, "", "", true, false, false } );
            } else if ( token.content == "raw" ) {
                PARSE_LITERAL( str );
                PARSE_LITERAL( str2 );
                token = input->get_token();
                if ( token.content != "prefix" ) {
                    create_prelude_error_msg( w_ctx, token, filename );
                    return false;
                }
                PARSE_LITERAL( str3 );
                token = input->get_token();
                if ( token.content != "rep_delimiter" ) {
                    create_prelude_error_msg( w_ctx, token, filename );
                    return false;
                }
                PARSE_LITERAL( str4 );
                PARSE_LITERAL( str5 );
                conf->token_conf.string.push_back( std::make_pair( str, str2 ) );
                conf->string_rules.push_back( StringRule{ str, str2, str3, str4, str5, false, true, true } );
            } else if ( token.content == "wide_raw" ) {
                PARSE_LITERAL( str );
                PARSE_LITERAL( str2 );
                token = input->get_token();
                if ( token.content != "prefix" ) {
                    create_prelude_error_msg( w_ctx, token, filename );
                    return false;
                }
                PARSE_LITERAL( str3 );
                token = input->get_token();
                if ( token.content != "rep_delimiter" ) {
                    create_prelude_error_msg( w_ctx, token, filename );
                    return false;
                }
                PARSE_LITERAL( str4 );
                PARSE_LITERAL( str5 );
                conf->token_conf.string.push_back( std::make_pair( str, str2 ) );
                conf->string_rules.push_back( StringRule{ str, str2, str3, str4, str5, false, true, false } );
            } else if ( token.content == "value_extraction" ) {
                PARSE_LITERAL( str );
                PARSE_LITERAL( str2 );
                conf->value_extract.push_back( std::make_pair( str, str2 ) );
                token = input->preview_token();
                if ( token.content == "multi_allowed" ) {
                    input->get_token(); // consume
                    conf->allow_multi_value_extract = true;
                } else if ( token.content == "multi_disallowed" ) {
                    input->get_token(); // consume
                    conf->allow_multi_value_extract = false;
                }
            } else { // Unknown token
                create_prelude_error_msg( w_ctx, token, filename );
                return false;
            }
        } else if ( mci == "LITERAL_SINGLE_CHARACTER_RULES" ) {
            token = input->get_token();
            if ( token.content == "basic_escaped" ) {
                PARSE_LITERAL( str );
                PARSE_LITERAL( str2 );
                conf->token_conf.string.push_back( std::make_pair( str, str2 ) );
                conf->char_rules.push_back( CharacterRule{ str, str2, "", true, true } );
            } else if ( token.content == "byte_escaped" ) {
                PARSE_LITERAL( str );
                PARSE_LITERAL( str2 );
                token = input->get_token();
                if ( token.content != "prefix" ) {
                    create_prelude_error_msg( w_ctx, token, filename );
                    return false;
                }
                PARSE_LITERAL( str3 );
                conf->token_conf.string.push_back( std::make_pair( str, str2 ) );
                conf->char_rules.push_back( CharacterRule{ str, str2, str3, true, false } );
            } else { // Unknown token
                create_prelude_error_msg( w_ctx, token, filename );
                return false;
            }
        } else if ( mci == "LITERAL_INTEGER_RULES" ) {
            token = input->get_token();
            if ( token.content == "default_dec" ) {
                // currently this is always true (TODO)
            } else if ( token.content == "prefix_bin" ) {
                PARSE_LITERAL( str );
                token = input->get_token();
                if ( token.content != "optional" ) {
                    create_prelude_error_msg( w_ctx, token, filename );
                    return false;
                }
                conf->token_conf.integer_prefix.push_back( str );
                conf->bin_int_prefix.push_back( str );
            } else if ( token.content == "prefix_oct" ) {
                PARSE_LITERAL( str );
                token = input->get_token();
                if ( token.content != "optional" ) {
                    create_prelude_error_msg( w_ctx, token, filename );
                    return false;
                }
                conf->token_conf.integer_prefix.push_back( str );
                conf->oct_int_prefix.push_back( str );
            } else if ( token.content == "prefix_hex" ) {
                PARSE_LITERAL( str );
                token = input->get_token();
                if ( token.content != "optional" ) {
                    create_prelude_error_msg( w_ctx, token, filename );
                    return false;
                }
                conf->token_conf.integer_prefix.push_back( str );
                conf->hex_int_prefix.push_back( str );
            } else if ( token.content == "delimiter_kilo" ) {
                PARSE_LITERAL( str );
                token = input->get_token();
                if ( token.content != "optional" ) {
                    create_prelude_error_msg( w_ctx, token, filename );
                    return false;
                }
                conf->token_conf.integer_delimiter.push_back( str );
                conf->kilo_int_delimiter.push_back( str );
            } else if ( token.content == "allow_type_postfix" ) {
                conf->allow_int_type_postfix = true;
            } else if ( token.content == "disallow_type_postfix" ) {
                conf->allow_int_type_postfix = false;
            } else { // Unknown token
                create_prelude_error_msg( w_ctx, token, filename );
                return false;
            }
        } else if ( mci == "LITERAL_FLOATING_POINT_RULES" ) {
            token = input->get_token();
            if ( token.content == "default_dec" ) {
                // currently this is always true (TODO)
            } else if ( token.content == "delimiter_kilo" ) {
                PARSE_LITERAL( str );
                token = input->get_token();
                if ( token.content != "optional" ) {
                    create_prelude_error_msg( w_ctx, token, filename );
                    return false;
                }
                conf->token_conf.float_delimiter.push_back( str );
                conf->kilo_float_delimiter.push_back( str );
            } else if ( token.content == "fraction_delimiter" ) {
                PARSE_LITERAL( str );
                token = input->get_token();
                if ( token.content != "optional" ) {
                    create_prelude_error_msg( w_ctx, token, filename );
                    return false;
                }
                conf->token_conf.float_delimiter.push_back( str );
                conf->fraction_delimiter.push_back( str );
            } else if ( token.content == "exponential_delimiter" ) {
                do {
                    PARSE_LITERAL( str );
                    conf->token_conf.float_delimiter.push_back( str );
                    conf->expo_delimiter.push_back( str );
                } while ( input->preview_token().content == "or" && /*consume*/ input->get_token().column );
                token = input->get_token();
                if ( token.content != "optional" ) {
                    create_prelude_error_msg( w_ctx, token, filename );
                    return false;
                }
            } else if ( token.content == "exponential_positive" ) {
                do {
                    PARSE_LITERAL( str );
                    conf->token_conf.float_delimiter.push_back( str );
                    conf->expo_positive.push_back( str );
                } while ( input->preview_token().content == "or" && /*consume*/ input->get_token().column );
            } else if ( token.content == "exponential_negative" ) {
                do {
                    PARSE_LITERAL( str );
                    conf->token_conf.float_delimiter.push_back( str );
                    conf->expo_negative.push_back( str );
                } while ( input->preview_token().content == "or" && /*consume*/ input->get_token().column );
            } else if ( token.content == "allow_type_postfix" ) {
                conf->allow_float_type_postfix = true;
            } else if ( token.content == "disallow_type_postfix" ) {
                conf->allow_float_type_postfix = false;
            } else { // Unknown token
                create_prelude_error_msg( w_ctx, token, filename );
                return false;
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
        } else if ( mci == "FUNCTION_DEFINITION" ) {
            // TODO
        } else if ( mci == "DEFINE_TEMPLATE" ) { // TODO
        } else if ( mci == "SCOPE_ACCESS" ) {
            // TODO
        } else if ( mci == "MEMBER_ACCESS" ) {
            // TODO
        } else if ( mci == "ARRAY_SPECIFIER" ) { // TODO
        } else if ( mci == "NEW_OPERATOR" ) {
            // TODO
        } else if ( mci == "REFERENCE_TYPE" ) {
            // TODO
        } else if ( mci == "TYPE_OF" ) {
            // TODO
        } else if ( mci == "STRUCT_TO_TUPLE" ) {
            // TODO
        } else if ( mci == "OPERATION_TYPE" ) {
            // TODO
        } else if ( mci == "RANGE_DEFINITION_EXC" ) {
            // TODO
        } else if ( mci == "RANGE_DEFINITION_FROM_EXC" ) {
            // TODO
        } else if ( mci == "RANGE_DEFINITION_TO_EXC" ) {
            // TODO
        } else if ( mci == "RANGE_DEFINITION_INC" ) {
            // TODO
        } else if ( mci == "RANGE_DEFINITION_TO_INC" ) {
            // TODO
        } else if ( mci == "LITERAL_TYPE" ) {
            // TODO
        } else if ( mci == "LITERAL_BOOLEAN" ) {
            // TODO
        } else if ( mci == "LITERAL_RANGE" ) {
            // TODO
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
        create_prelude_error_msg( w_ctx, token, filename );
        return false;
    }
    token = input->get_token();
    // semicolon
    if ( token.type != Token::Type::stat_divider ) {
        create_prelude_error_msg( w_ctx, token, filename );
        return false;
    }

    return true;
}

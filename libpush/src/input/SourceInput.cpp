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
#include "libpushc/input/SourceInput.h"
#include "libpushc/Worker.h"
#include "libpushc/GlobalCtx.h"

TokenConfig TokenConfig::get_prelude_cfg() {
    TokenConfig cfg;
    cfg.stat_divider.push_back( ";" );
    cfg.block.push_back( std::make_pair( "{", "}" ) );
    cfg.term.push_back( std::make_pair( "(", ")" ) );
    cfg.comment.push_back( std::make_pair( "/*", "*/" ) );
    cfg.comment.push_back( std::make_pair( "//", "\n" ) );
    cfg.comment.push_back( std::make_pair( "//", "\r" ) );
    cfg.nested_comments = false;
    cfg.allowed_chars = std::make_pair<u32, u32>( 0, 0xffffffff );
    cfg.nested_strings = false;
    cfg.char_escapes.push_back( std::make_pair( "\\n", "\n" ) );
    cfg.char_escapes.push_back( std::make_pair( "\\t", "\t" ) );
    cfg.char_escapes.push_back( std::make_pair( "\\v", "\v" ) );
    cfg.char_escapes.push_back( std::make_pair( "\\r", "\r" ) );
    cfg.char_escapes.push_back( std::make_pair( "\\\\", "\\" ) );
    cfg.char_escapes.push_back( std::make_pair( "\\\'", "\'" ) );
    cfg.char_escapes.push_back( std::make_pair( "\\\"", "\"" ) );
    cfg.char_escapes.push_back( std::make_pair( "\\0", "\0" ) );
    cfg.char_encodings.push_back( "\\o" );
    cfg.char_encodings.push_back( "\\x" );
    cfg.char_encodings.push_back( "\\u" );
    cfg.string.push_back( std::make_pair( "\"", "\"" ) );
    cfg.integer_prefix.push_back( "0o" );
    cfg.integer_prefix.push_back( "0b" );
    cfg.integer_prefix.push_back( "0h" );
    cfg.float_delimiter.push_back( "." );
    cfg.operators.push_back( "," );
    cfg.operators.push_back( "->" );
    cfg.operators.push_back( "::" );
    return cfg;
}

std::pair<Token::Type, size_t> SourceInput::ending_token( const String &str, bool in_string, bool in_comment,
                                                          const Token::Type curr_tt, size_t curr_line,
                                                          size_t curr_column ) {
    if ( str.empty() ) {
        LOG_ERR( "Passed empty string to ending_token()." );
        return std::make_pair( Token::Type::count, 0 ); // invalid
    }
    u8 back = static_cast<u8>( str.back() );

    if ( back < cfg.allowed_chars.first || back > cfg.allowed_chars.second ) {
        w_ctx->print_msg<MessageType::err_lexer_char_not_allowed>(
            MessageInfo( filename, curr_line, curr_line, curr_column, 1, 0, FmtStr::Color::BoldRed ), {}, back );
    }

    // NOTE better approach to check elements: hashmaps for each operator size. Would be faster and not need all the
    // size checks and substr etc.
    // FIXME wrong comment ending rules may be applied:
    // "// foo */ bar" ends line comment
    // "/* foo \nbar*/" ends comment on newline
    if ( back == ' ' || back == '\n' || back == '\r' || back == '\t' ) {
        // first check if comments or strings are terminated by this token
        if ( in_comment ) {
            for ( auto &tc : cfg.comment ) { // comment delimiter
                if ( str.size() >= tc.second.size() && str.slice( str.size() - tc.second.size() ) == tc.second )
                    return std::make_pair( Token::Type::comment_end, tc.second.size() );
            }
        } else if ( in_string ) {
            for ( auto &tc : cfg.string ) { // comment delimiter
                if ( str.size() >= tc.second.size() && str.slice( str.size() - tc.second.size() ) == tc.second )
                    return std::make_pair( Token::Type::string_end, tc.second.size() );
            }
        }
        return std::make_pair( Token::Type::ws, 1 ); // whitespace
    } else {
        for ( auto &tc : cfg.stat_divider ) { // statement divider
            if ( str.size() >= tc.size() && str.slice( str.size() - tc.size() ) == tc )
                return std::make_pair( Token::Type::stat_divider, tc.size() );
        }
        for ( auto &tc : cfg.comment ) { // comment delimiter
            if ( in_comment && !cfg.nested_comments ) { // first check comment end
                if ( str.size() >= tc.second.size() && str.slice( str.size() - tc.second.size() ) == tc.second )
                    return std::make_pair( Token::Type::comment_end, tc.second.size() );
                if ( str.size() >= tc.first.size() && str.slice( str.size() - tc.first.size() ) == tc.first )
                    return std::make_pair( Token::Type::comment_begin, tc.first.size() );
            } else {
                if ( str.size() >= tc.first.size() && str.slice( str.size() - tc.first.size() ) == tc.first )
                    return std::make_pair( Token::Type::comment_begin, tc.first.size() );
                if ( str.size() >= tc.second.size() && str.slice( str.size() - tc.second.size() ) == tc.second )
                    return std::make_pair( Token::Type::comment_end, tc.second.size() );
            }
        }
        for ( auto &tc : cfg.block ) { // block delimiter
            if ( str.size() >= tc.first.size() && str.slice( str.size() - tc.first.size() ) == tc.first )
                return std::make_pair( Token::Type::block_begin, tc.first.size() );
            if ( str.size() >= tc.second.size() && str.slice( str.size() - tc.second.size() ) == tc.second )
                return std::make_pair( Token::Type::block_end, tc.second.size() );
        }
        for ( auto &tc : cfg.term ) { // term delimiter
            if ( str.size() >= tc.first.size() && str.slice( str.size() - tc.first.size() ) == tc.first )
                return std::make_pair( Token::Type::term_begin, tc.first.size() );
            if ( str.size() >= tc.second.size() && str.slice( str.size() - tc.second.size() ) == tc.second )
                return std::make_pair( Token::Type::term_end, tc.second.size() );
        }
        if ( curr_tt == Token::Type::encoded_char && ( back >= '0' && back <= '9' || back >= 'A' && back <= 'Z' ||
                                                       back >= 'a' && back <= 'z' ) ) { // encoded characters
            return std::make_pair( Token::Type::encoded_char, 1 );
        }
        if ( curr_tt != Token::Type::identifier &&
             ( back >= '0' && back <= '9' ||
               ( ( curr_tt == Token::Type::number || curr_tt == Token::Type::number_float ) &&
                 ( back >= 'A' && back <= 'F' || back >= 'a' && back <= 'f' ) ) ) ) { // numbers
            if ( curr_tt == Token::Type::number_float )
                return std::make_pair( Token::Type::number_float, 1 );
            else
                return std::make_pair( Token::Type::number, 1 );
        }
        for ( auto &tc : cfg.integer_prefix ) { // integer prefix
            if ( str.size() >= tc.size() && str.slice( str.size() - tc.size() ) == tc )
                return std::make_pair( Token::Type::number, tc.size() );
        }
        for ( auto &tc : cfg.float_prefix ) { // float prefix
            if ( str.size() >= tc.size() && str.slice( str.size() - tc.size() ) == tc )
                return std::make_pair( Token::Type::number_float, tc.size() );
        }
        if ( curr_tt == Token::Type::number ) {
            for ( auto &tc : cfg.integer_delimiter ) { // integer delimiter
                if ( str.size() >= tc.size() &&
                     str.slice( str.size() - tc.size() ) == tc ) // FIXME can only handle single character delimiter
                    return std::make_pair( Token::Type::number, tc.size() );
            }
        }
        if ( curr_tt == Token::Type::number_float || curr_tt == Token::Type::number ) {
            for ( auto &tc : cfg.float_delimiter ) { // float delimiter
                if ( str.size() >= tc.size() &&
                     str.slice( str.size() - tc.size() ) == tc ) // FIXME can only handle single character delimiter
                    return std::make_pair( Token::Type::number_float, tc.size() );
            }
        }
        for ( auto &tc : cfg.char_encodings ) { // encoded characters
            if ( str.size() >= tc.size() && str.slice( str.size() - tc.size() ) == tc )
                return std::make_pair( Token::Type::encoded_char, tc.size() );
        }
        for ( auto &tc : cfg.operators ) { // operators
            if ( str.size() >= tc.size() && str.slice( str.size() - tc.size() ) == tc )
                return std::make_pair( Token::Type::op, tc.size() );
        }
        for ( auto &tc : cfg.keywords ) { // keywords
            if ( str == tc )
                return std::make_pair( Token::Type::keyword, tc.size() );
        }
        for ( auto &tc : cfg.string ) { // string delimiter
            if ( in_string && !cfg.nested_strings ) { // first check string end
                if ( str.size() >= tc.second.size() && str.slice( str.size() - tc.second.size() ) == tc.second )
                    return std::make_pair( Token::Type::string_end, tc.second.size() );
                if ( str.size() >= tc.first.size() && str.slice( str.size() - tc.first.size() ) == tc.first )
                    return std::make_pair( Token::Type::string_begin, tc.first.size() );
            } else {
                if ( str.size() >= tc.first.size() && str.slice( str.size() - tc.first.size() ) == tc.first )
                    return std::make_pair( Token::Type::string_begin, tc.first.size() );
                if ( str.size() >= tc.second.size() && str.slice( str.size() - tc.second.size() ) == tc.second )
                    return std::make_pair( Token::Type::string_end, tc.second.size() );
            }
        }
        return std::make_pair( Token::Type::identifier, 1 ); // anything different
    }
}

void SourceInput::configure( const TokenConfig &cfg ) {
    revert_size = 1; // min 1, to review carriage return character
    this->cfg = cfg;

    for ( auto tc : cfg.stat_divider ) {
        if ( tc.size() > revert_size )
            revert_size = tc.size();
    }
    for ( auto tc : cfg.block ) {
        if ( tc.first.size() > revert_size )
            revert_size = tc.first.size();
        if ( tc.second.size() > revert_size )
            revert_size = tc.second.size();
    }
    for ( auto tc : cfg.term ) {
        if ( tc.first.size() > revert_size )
            revert_size = tc.first.size();
        if ( tc.second.size() > revert_size )
            revert_size = tc.second.size();
    }
    for ( auto tc : cfg.comment ) {
        if ( tc.first.size() > revert_size )
            revert_size = tc.first.size();
        if ( tc.second.size() > revert_size )
            revert_size = tc.second.size();
    }
    for ( auto tc : cfg.char_escapes ) {
        if ( tc.first.size() > revert_size )
            revert_size = tc.first.size();
        if ( tc.second.size() > revert_size )
            revert_size = tc.second.size();
    }
    for ( auto tc : cfg.char_encodings ) {
        if ( tc.size() > revert_size )
            revert_size = tc.size();
    }
    for ( auto tc : cfg.string ) {
        if ( tc.first.size() > revert_size )
            revert_size = tc.first.size();
        if ( tc.second.size() > revert_size )
            revert_size = tc.second.size();
    }
    for ( auto tc : cfg.integer_prefix ) {
        if ( tc.size() > revert_size )
            revert_size = tc.size();
    }
    for ( auto tc : cfg.integer_delimiter ) {
        if ( tc.size() > revert_size )
            revert_size = tc.size();
    }
    for ( auto tc : cfg.float_prefix ) {
        if ( tc.size() > revert_size )
            revert_size = tc.size();
    }
    for ( auto tc : cfg.float_delimiter ) {
        if ( tc.size() > revert_size )
            revert_size = tc.size();
    }
    for ( auto tc : cfg.operators ) {
        if ( tc.size() > revert_size )
            revert_size = tc.size();
    }
    for ( auto tc : cfg.keywords ) {
        if ( tc.size() > revert_size )
            revert_size = tc.size();
    }
}

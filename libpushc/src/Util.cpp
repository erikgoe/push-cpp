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
#include "libpushc/Util.h"

void consume_comment( sptr<SourceInput> &input, TokenConfig &conf ) {
    std::stack<String> comment_begin; // stack of token which start a comment
    do { // consume comment
        auto token = input->get_token();
        if ( token.type == Token::Type::comment_begin ) {
            auto *alo = comment_begin.empty() ? nullptr : &conf.allowed_level_overlay[comment_begin.top()];
            if ( !alo || std::find( alo->begin(), alo->end(), token.content ) != alo->end() ) {
                comment_begin.push( token.content );
            }
        } else if ( token.type == Token::Type::comment_end ) {
            bool found = false;
            // First check normal comments
            for ( auto pair : conf.level_map[TokenLevel::comment] ) {
                if ( pair.second.begin_token == comment_begin.top() && pair.second.end_token == token.content ) {
                    comment_begin.pop();
                    found = true;
                    break;
                }
            }
            if ( !found ) {
                // Check line comments
                for ( auto pair : conf.level_map[TokenLevel::comment_line] ) {
                    if ( pair.second.begin_token == comment_begin.top() && pair.second.end_token == token.content ) {
                        comment_begin.pop();
                        break;
                    }
                }
            }
        } else if ( token.type == Token::Type::eof )
            break;
    } while ( !comment_begin.empty() );
}

String parse_string( SourceInput &input, Worker &w_ctx ) {
    auto token = input.get_token();
    if ( token.type != Token::Type::string_begin ) {
        LOG_ERR( "String does not start with Token::Type::string_begin." );
        return "";
    }

    String ret = "";
    auto start_token = token; // save begin for error reporting
    token = input.preview_next_token();
    while ( token.type != Token::Type::string_end && token.type != Token::Type::eof ) {
        token = input.get_token();
        String content = token.content;
        if ( token.type == Token::Type::escaped_char ) {
            content = w_ctx.unit_ctx()->prelude_conf.token_conf.char_escapes[token.content];
        }
        if ( !ret.empty() )
            ret += token.leading_ws + content;
        else
            ret += content;
        token = input.preview_token();
    }
    if ( token.type == Token::Type::string_end )
        ret += input.get_token().leading_ws; // consume & add ws
    if ( token.type == Token::Type::eof ) { // string_end not found
        w_ctx.print_msg<MessageType::err_unexpected_eof_at_string_parsing>(
            MessageInfo( token.file, token.line, token.line, token.column, token.length, 0, FmtStr::Color::BoldRed ),
            {}, token.file );
        return "";
    }
    return ret;
}

Number parse_number( sptr<SourceInput> &input, Worker &w_ctx ) {
    Number num;

    auto token = input->get_token();
    if ( token.type == Token::Type::number ) {
        num = stoull( token.content );
    } else {
        w_ctx.print_msg<MessageType::err_parse_number>(
            MessageInfo( token.file, token.line, token.line, token.column, token.length, 0, FmtStr::Color::BoldRed ),
            {} );
        return num;
    }

    return num;
}

void append_hex_str( u8 val, String &str ) {
    str.reserve( str.size() + 2 );
    char tmp = '0' + ( ( val & 0xf0 ) >> 4 );
    if ( tmp > '9' )
        tmp += 'a' - ':';
    str.push_back( tmp );
    tmp = '0' + ( val & 0xf );
    if ( tmp > '9' )
        tmp += 'a' - ':';
    str.push_back( tmp );
}

bool is_operator_token( const String &token ) {
    if ( token.empty() ) {
        LOG_ERR( "Token string is empty. In `is_operator_token()`" );
        return true;
    }

    // NOTE: just a simple implementation
    return !( ( token.front() >= 'a' && token.front() <= 'z' ) || ( token.front() >= 'A' && token.front() <= 'Z' ) ||
              ( token.front() >= '0' && token.front() <= '9' ) || token.front() > (unsigned char) 128 );
}

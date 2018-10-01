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

#pragma once
#include "libpushc/stdafx.h"
#include "libpushc/Util.h"

void consume_comment( std::shared_ptr<SourceInput> &input, TokenConfig &conf ) {
    std::stack<String> comment_begin; // stack of token which start a comment
    do { // consume comment
        auto token = input->get_token();
        if ( token.type == Token::Type::comment_begin ) {
            if ( comment_begin.empty() || conf.nested_comments ) {
                comment_begin.push( token.content );
            }
        } else if ( token.type == Token::Type::comment_end ) {
            for ( auto pair : conf.comment ) {
                if ( pair.first == comment_begin.top() && pair.second == token.content ) {
                    comment_begin.pop();
                    break;
                }
            }
        } else if ( token.type == Token::Type::eof )
            break;
    } while ( !comment_begin.empty() );
}

String parse_string( std::shared_ptr<SourceInput> &input, Worker &w_ctx ) {
    auto token = input->get_token();
    if ( token.type != Token::Type::string_begin ) {
        LOG_ERR( "String does not start with Token::Type::string_begin." );
        return "";
    }

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
    if ( token.type == Token::Type::eof ) { // string_end not found
        w_ctx.print_msg<MessageType::err_unexpected_eof_at_string_parsing>(
            MessageInfo( token.file, token.line, token.line, token.column, token.length, 0, FmtStr::Color::BoldRed ),
            {}, token.file );
        return "";
    }
    return ret;
}

Number parse_number( std::shared_ptr<SourceInput> &input, Worker &w_ctx, std::shared_ptr<PreludeConfig> &conf ) {
    Number num;
    String value;

    auto token = input->get_token();
    if ( token.type == Token::Type::number ) {
        value = token.content;
        num.value.integer = stoll( value );
        num.type = Number::Type::integer;
        // TODO unsigned integer
    } else if ( token.type == Token::Type::number_float ) {
        value = token.content;
        num.value.floating_p = stod( value );
        num.type = Number::Type::floating_p;
    } else {
        w_ctx.print_msg<MessageType::err_parse_number>(
            MessageInfo( token.file, token.line, token.line, token.column, token.length, 0, FmtStr::Color::BoldRed ),
            {} );
        return num;
    }

    // TODO delimiter, prefix, postfix, exponential float
    return num;
}

bool is_operator_token( const String &token ) {
    if ( token.empty() ) {
        LOG_ERR( "Token string is empty. In `is_operator_token()`" );
        return true;
    }

    // NOTE: just a simple implementation
    return !( ( token.front() >= 'a' && token.front() <= 'z' ) || ( token.front() >= 'A' && token.front() <= 'Z' ) ||
              token.front() > 128 );
}
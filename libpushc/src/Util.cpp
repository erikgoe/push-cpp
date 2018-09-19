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

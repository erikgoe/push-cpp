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

#include "libpushc/tests/stdafx.h"
#include "catch/catch.hpp"
#include "libpushc/input/FileInput.h"

TEST_CASE( "Basic lexing", "[lexer]" ) {
    FileInput fin( CMAKE_PROJECT_ROOT "/Test/lexer.push", 5000, 4096 );
    auto cfg = TokenConfig::get_prelude_cfg();
    cfg.operators.push_back( "=" );
    cfg.operators.push_back( "+" );
    cfg.operators.push_back( "-" );
    cfg.keywords.push_back( "let" );
    fin.configure( cfg );

    std::list<String> token_list;
    while ( true ) {
        auto token = fin.get_token( false );
        if ( token.type == Token::Type::eof )
            break;
        token_list.push_back( token.content );
    }
    std::list<String> check_list{ "//",  "testing", "the",        "lexer", "(",       "SourceInput", ")", "main", "{",
                                  "let", "a",       "=",          "4",     ";",       "let",         "b", "=",    "3.2",
                                  ";",   "//",      "commenting", "c",     "=",       "a",           "+", "b",    "-",
                                  "2",   ";",       "/*",         "other", "comment", "*/",          "}" };

    CHECK( token_list == check_list );
}

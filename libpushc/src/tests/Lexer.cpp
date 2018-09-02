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
    cfg.operators.push_back( "+=-" );
    cfg.operators.push_back( "--" );
    cfg.operators.push_back( "=" );
    cfg.operators.push_back( "+" );
    cfg.operators.push_back( "-" );
    cfg.operators.push_back( "." );
    cfg.keywords.push_back( "let" );
    cfg.nested_comments = true;
    fin.configure( cfg );

    std::list<String> token_content_list;
    std::list<Token::Type> token_type_list;
    std::list<size_t> token_column_list;
    auto start = std::chrono::steady_clock::now();
    while ( true ) {
        auto token = fin.get_token( false );
        if ( token.type == Token::Type::eof )
            break;
        token_content_list.push_back( token.content );
        token_type_list.push_back( token.type );
        token_column_list.push_back( token.column );
    }
    auto duration = std::chrono::steady_clock::now() - start;
    LOG( "Lexer took " + to_string( std::chrono::duration_cast<std::chrono::microseconds>( duration ).count() ) +
         " microseconds." );

    std::list<String> content_check_list{ "//",
                                          "testing",
                                          "the",
                                          "lexer",
                                          "(",
                                          "SourceInput",
                                          ")",
                                          "\n",
                                          "main",
                                          "{",
                                          "letlet",
                                          "a",
                                          "=",
                                          "4",
                                          ";",
                                          "let",
                                          "b",
                                          "=",
                                          "3.2",
                                          ";",
                                          "//",
                                          "commenting",
                                          "🦄🦓and🦌",
                                          "\n",
                                          "c",
                                          "=",
                                          "a",
                                          "+",
                                          "b",
                                          "-",
                                          "2",
                                          ";",
                                          "/*",
                                          "other",
                                          "/*",
                                          "comment",
                                          "/*",
                                          "with",
                                          "*/",
                                          "*/",
                                          "nested",
                                          "*/",
                                          "c",
                                          "-",
                                          "+=-",
                                          "+=-",
                                          "--",
                                          "-",
                                          "objletlet",
                                          ".",
                                          "letletdo",
                                          "(",
                                          ")",
                                          ";",
                                          "}" };
    std::list<Token::Type> type_check_list{ Token::Type::comment_begin,
                                            Token::Type::identifier,
                                            Token::Type::identifier,
                                            Token::Type::identifier,
                                            Token::Type::term_begin,
                                            Token::Type::identifier,
                                            Token::Type::term_end,
                                            Token::Type::comment_end,
                                            Token::Type::identifier,
                                            Token::Type::block_begin,
                                            Token::Type::identifier,
                                            Token::Type::identifier,
                                            Token::Type::op,
                                            Token::Type::number,
                                            Token::Type::stat_divider,
                                            Token::Type::keyword,
                                            Token::Type::identifier,
                                            Token::Type::op,
                                            Token::Type::number_float,
                                            Token::Type::stat_divider,
                                            Token::Type::comment_begin,
                                            Token::Type::identifier,
                                            Token::Type::identifier,
                                            Token::Type::comment_end,
                                            Token::Type::identifier,
                                            Token::Type::op,
                                            Token::Type::identifier,
                                            Token::Type::op,
                                            Token::Type::identifier,
                                            Token::Type::op,
                                            Token::Type::number,
                                            Token::Type::stat_divider,
                                            Token::Type::comment_begin,
                                            Token::Type::identifier,
                                            Token::Type::comment_begin,
                                            Token::Type::identifier,
                                            Token::Type::comment_begin,
                                            Token::Type::identifier,
                                            Token::Type::comment_end,
                                            Token::Type::comment_end,
                                            Token::Type::identifier,
                                            Token::Type::comment_end,
                                            Token::Type::identifier,
                                            Token::Type::op,
                                            Token::Type::op,
                                            Token::Type::op,
                                            Token::Type::op,
                                            Token::Type::op,
                                            Token::Type::identifier,
                                            Token::Type::op,
                                            Token::Type::identifier,
                                            Token::Type::term_begin,
                                            Token::Type::term_end,
                                            Token::Type::stat_divider,
                                            Token::Type::block_end };

    std::list<size_t> column_check_list{
        1, // 1 line
        4,  12, 16, 22, 23, 34, 35,
        1, // 3 line
        6,
        5, // 4 line
        12, 13, 15, 16,
        5, // 5 line
        9,  11, 12, 15, 17, 20, 31, 37,
        5, // 6 line
        7,  9,  10, 11, 13, 15, 16, 18, 20, 26, 28, 36, 38, 42, 44, 47, 53,
        5, // 7 line
        7,  8,  11, 14, 16, 17, 26, 27, 35, 36, 37,
        1 // 8 line
    };
    CHECK( token_content_list == content_check_list );
    CHECK( token_type_list == type_check_list );
    CHECK( token_column_list == column_check_list );
}

#ifndef _DEBUG
TEST_CASE( "Stress test lexing", "[lexer]" ) {
    FileInput fin( CMAKE_PROJECT_ROOT "/Test/gibberish.txt", 50, 30 );
    auto cfg = TokenConfig::get_prelude_cfg();
    cfg.operators.push_back( "." );
    fin.configure( cfg );

    size_t token_count = 0, identifier_count = 0;
    auto start = std::chrono::steady_clock::now();
    while ( true ) {
        auto token = fin.get_token( false );
        if ( token.type == Token::Type::eof )
            break;
        token_count++;
        if ( token.type == Token::Type::identifier )
            identifier_count++;
    }
    auto duration = std::chrono::steady_clock::now() - start;
    LOG( "Lexer stress test took " +
         to_string( std::chrono::duration_cast<std::chrono::microseconds>( duration ).count() ) +
         " microseconds. With " + to_string( token_count ) + " tokens including " + to_string( identifier_count ) +
         " identifiers " );

    CHECK( token_count == 1001000 );
    CHECK( identifier_count == 1000000 );
}
#endif

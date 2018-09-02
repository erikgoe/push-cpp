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
#include "libpushc/input/FileInput.h"

namespace Catch {
template <>
struct StringMaker<Token> {
    static std::string convert( Token const &token ) {
        return "type: " + to_string( static_cast<int>( token.type ) ) + ", \"" + token.content + "\", file: " +
               ( token.file ? "\"" + *token.file + "\"" : "nullptr" ) + ", line: " + to_string( token.line ) +
               ", column: " + to_string( token.column ) + ", length: " + to_string( token.length ) + ", " +
               ( token.leading_ws ? "" : "!" ) + "leading_ws, ";
    }
};
} // namespace Catch

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

    std::list<Token> token_list;
    auto start = std::chrono::steady_clock::now();
    while ( true ) {
        auto token = fin.get_token( false );
        if ( token.type == Token::Type::eof )
            break;
        token_list.push_back( token );
    }
    auto duration = std::chrono::steady_clock::now() - start;
    LOG( "Lexer took " + to_string( std::chrono::duration_cast<std::chrono::microseconds>( duration ).count() ) +
         " microseconds." );

    auto test_file = std::make_shared<String>( CMAKE_PROJECT_ROOT "/Test/lexer.push" );
    std::list<Token> token_check_list{
        Token( Token::Type::comment_begin, "//", test_file, 1, 1, 2, false ),
        Token( Token::Type::identifier, "testing", test_file, 1, 4, 7, true ),
        Token( Token::Type::identifier, "the", test_file, 1, 12, 3, true ),
        Token( Token::Type::identifier, "lexer", test_file, 1, 16, 5, true ),
        Token( Token::Type::term_begin, "(", test_file, 1, 22, 1, true ),
        Token( Token::Type::identifier, "SourceInput", test_file, 1, 23, 11, false ),
        Token( Token::Type::term_end, ")", test_file, 1, 34, 1, false ),
        Token( Token::Type::comment_end, "\n", test_file, 1, 35, 1, false ),
        Token( Token::Type::identifier, "main", test_file, 3, 1, 4, true ),
        Token( Token::Type::block_begin, "{", test_file, 3, 6, 1, true ),
        Token( Token::Type::identifier, "letlet", test_file, 4, 5, 6, true ),
        Token( Token::Type::identifier, "a", test_file, 4, 12, 1, true ),
        Token( Token::Type::op, "=", test_file, 4, 13, 1, false ),
        Token( Token::Type::number, "4", test_file, 4, 15, 1, true ),
        Token( Token::Type::stat_divider, ";", test_file, 4, 16, 1, false ),
        Token( Token::Type::keyword, "let", test_file, 5, 5, 3, true ),
        Token( Token::Type::identifier, "b", test_file, 5, 9, 1, true ),
        Token( Token::Type::op, "=", test_file, 5, 11, 1, true ),
        Token( Token::Type::number_float, "3.2", test_file, 5, 12, 3, false ),
        Token( Token::Type::stat_divider, ";", test_file, 5, 15, 1, false ),
        Token( Token::Type::comment_begin, "//", test_file, 5, 17, 2, true ),
        Token( Token::Type::identifier, "commenting", test_file, 5, 20, 10, true ),
        Token( Token::Type::identifier, "🦄🦓and🦌", test_file, 5, 31, 6, true ),
        Token( Token::Type::comment_end, "\n", test_file, 5, 37, 1, false ),
        Token( Token::Type::identifier, "c", test_file, 6, 5, 1, true ),
        Token( Token::Type::op, "=", test_file, 6, 7, 1, true ),
        Token( Token::Type::identifier, "a", test_file, 6, 9, 1, true ),
        Token( Token::Type::op, "+", test_file, 6, 10, 1, false ),
        Token( Token::Type::identifier, "b", test_file, 6, 11, 1, false ),
        Token( Token::Type::op, "-", test_file, 6, 13, 1, true ),
        Token( Token::Type::number, "2", test_file, 6, 15, 1, true ),
        Token( Token::Type::stat_divider, ";", test_file, 6, 16, 1, false ),
        Token( Token::Type::comment_begin, "/*", test_file, 6, 18, 2, true ),
        Token( Token::Type::identifier, "other", test_file, 6, 20, 5, false ),
        Token( Token::Type::comment_begin, "/*", test_file, 6, 26, 2, true ),
        Token( Token::Type::identifier, "comment", test_file, 6, 28, 7, false ),
        Token( Token::Type::comment_begin, "/*", test_file, 6, 36, 2, true ),
        Token( Token::Type::identifier, "with", test_file, 6, 38, 4, false ),
        Token( Token::Type::comment_end, "*/", test_file, 6, 42, 2, false ),
        Token( Token::Type::comment_end, "*/", test_file, 6, 44, 2, false ),
        Token( Token::Type::identifier, "nested", test_file, 6, 47, 6, true ),
        Token( Token::Type::comment_end, "*/", test_file, 6, 53, 2, false ),
        Token( Token::Type::identifier, "c", test_file, 7, 5, 1, true ),
        Token( Token::Type::op, "-", test_file, 7, 7, 1, true ),
        Token( Token::Type::op, "+=-", test_file, 7, 8, 3, false ),
        Token( Token::Type::op, "+=-", test_file, 7, 11, 3, false ),
        Token( Token::Type::op, "--", test_file, 7, 14, 2, false ),
        Token( Token::Type::op, "-", test_file, 7, 16, 1, false ),
        Token( Token::Type::identifier, "objletlet", test_file, 7, 17, 9, false ),
        Token( Token::Type::op, ".", test_file, 7, 26, 1, false ),
        Token( Token::Type::identifier, "letletdo", test_file, 7, 27, 8, false ),
        Token( Token::Type::term_begin, "(", test_file, 7, 35, 1, false ),
        Token( Token::Type::term_end, ")", test_file, 7, 36, 1, false ),
        Token( Token::Type::stat_divider, ";", test_file, 7, 37, 1, false ),
        Token( Token::Type::block_end, "}", test_file, 8, 1, 1, true ),
    };

    REQUIRE( token_list.size() == token_check_list.size() );
    auto token_itr = token_list.begin();
    auto token_check_itr = token_check_list.begin();
    while ( token_itr != token_list.end() ) {
        CHECK( *token_itr == *token_check_itr );
        token_itr++;
        token_check_itr++;
    }
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

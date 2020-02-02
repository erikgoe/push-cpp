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

#include "libpush/tests/stdafx.h"
#include "libpush/input/FileInput.h"
#include "libpush/GlobalCtx.h"

namespace Catch {
template <>
struct StringMaker<Token> {
    static std::string convert( Token const &token ) {
        return "type: " + to_string( static_cast<int>( token.type ) ) + ", \"" + escape_ws( token.content ) +
               "\", file: " + ( token.file ? "\"" + *token.file + "\"" : "nullptr" ) +
               ", line: " + to_string( token.line ) + ", column: " + to_string( token.column ) +
               ", length: " + to_string( token.length ) + ", leading_ws: \"" + escape_ws( token.leading_ws ) +
               "\", token_level: " + to_string( static_cast<int>( token.tl ) );
    }
    static std::string escape_ws( const String &str ) {
        std::string result;
        for ( auto &s : str ) {
            if ( s == '\n' )
                result += "\\n";
            else if ( s == '\t' )
                result += "\\t";
            else if ( s == '\r' )
                result += "\\r";
            else
                result += s;
        }
        return result;
    }
};
} // namespace Catch

TEST_CASE( "Basic lexing", "[lexer]" ) {
    auto g_ctx = make_shared<GlobalCtx>();
    sptr<Worker> w_ctx = g_ctx->setup( 1, 0 );

    FileInput fin( make_shared<String>( CMAKE_PROJECT_ROOT "/Test/lexer.push" ), w_ctx );
    auto cfg = TokenConfig::get_prelude_cfg();
    cfg.operators.push_back( "+=-" );
    cfg.operators.push_back( "--" );
    cfg.operators.push_back( "=" );
    cfg.operators.push_back( "+" );
    cfg.operators.push_back( "-" );
    cfg.operators.push_back( "." );
    cfg.operators.push_back( "/" ); // just to test doc comments
    cfg.keywords.push_back( "let" );
    cfg.level_map[TokenLevel::comment_line]["lnd"] = { "///", "\n" };
    cfg.allowed_level_overlay[""].push_back( "lnd" );
    fin.configure( cfg );

    std::list<Token> token_list;
    auto start = std::chrono::steady_clock::now();
    while ( true ) {
        auto token = fin.get_token();
        if ( token.type == Token::Type::eof )
            break;
        token_list.push_back( token );
    }
    auto duration = std::chrono::steady_clock::now() - start;
    LOG( "Lexer took " + to_string( std::chrono::duration_cast<std::chrono::microseconds>( duration ).count() ) +
         " microseconds." );

    auto test_file = make_shared<String>( CMAKE_PROJECT_ROOT "/Test/lexer.push" );
    std::list<Token> token_check_list{
        Token( Token::Type::comment_begin, "//", test_file, 1, 1, 2, "", TokenLevel::normal ),
        Token( Token::Type::identifier, "testing", test_file, 1, 4, 7, " ", TokenLevel::comment_line ),
        Token( Token::Type::identifier, "the", test_file, 1, 12, 3, " ", TokenLevel::comment_line ),
        Token( Token::Type::identifier, "lexer", test_file, 1, 16, 5, " ", TokenLevel::comment_line ),
        Token( Token::Type::term_begin, "(", test_file, 1, 22, 1, " ", TokenLevel::comment_line ),
        Token( Token::Type::identifier, "SourceInput", test_file, 1, 23, 11, "", TokenLevel::comment_line ),
        Token( Token::Type::term_end, ")", test_file, 1, 34, 1, "", TokenLevel::comment_line ),
        Token( Token::Type::comment_end, "\n", test_file, 1, 35, 1, "", TokenLevel::comment_line ),
        Token( Token::Type::comment_begin, "///", test_file, 2, 2, 3, "\n ", TokenLevel::normal ),
        Token( Token::Type::identifier, "a", test_file, 2, 6, 1, " ", TokenLevel::comment_line ),
        Token( Token::Type::identifier, "doc", test_file, 2, 8, 3, " ", TokenLevel::comment_line ),
        Token( Token::Type::identifier, "comment", test_file, 2, 12, 7, " ", TokenLevel::comment_line ),
        Token( Token::Type::comment_end, "\n", test_file, 2, 20, 1, " ", TokenLevel::comment_line ),
        Token( Token::Type::identifier, "main", test_file, 4, 1, 4, "\n  \n", TokenLevel::normal ),
        Token( Token::Type::block_begin, "{", test_file, 4, 6, 1, " ", TokenLevel::normal ),
        Token( Token::Type::identifier, "letlet", test_file, 5, 5, 6, "\n\t", TokenLevel::normal ),
        Token( Token::Type::identifier, "a", test_file, 5, 12, 1, " ", TokenLevel::normal ),
        Token( Token::Type::op, "=", test_file, 5, 13, 1, "", TokenLevel::normal ),
        Token( Token::Type::number, "4", test_file, 5, 15, 1, " ", TokenLevel::normal ),
        Token( Token::Type::stat_divider, ";", test_file, 5, 16, 1, "", TokenLevel::normal ),
        Token( Token::Type::keyword, "let", test_file, 6, 5, 3, " \n    ", TokenLevel::normal ),
        Token( Token::Type::identifier, "b", test_file, 6, 9, 1, " ", TokenLevel::normal ),
        Token( Token::Type::op, "=", test_file, 6, 11, 1, " ", TokenLevel::normal ),
        Token( Token::Type::number, "3", test_file, 6, 12, 1, "", TokenLevel::normal ),
        Token( Token::Type::op, ".", test_file, 6, 13, 1, "", TokenLevel::normal ),
        Token( Token::Type::number, "2", test_file, 6, 14, 1, "", TokenLevel::normal ),
        Token( Token::Type::stat_divider, ";", test_file, 6, 15, 1, "", TokenLevel::normal ),
        Token( Token::Type::comment_begin, "//", test_file, 6, 17, 2, " ", TokenLevel::normal ),
        Token( Token::Type::identifier, "commenting", test_file, 6, 20, 10, " ", TokenLevel::comment_line ),
        Token( Token::Type::identifier, "🦄🦓and🦌", test_file, 6, 31, 6, " ", TokenLevel::comment_line ),
        Token( Token::Type::comment_end, "\n", test_file, 6, 37, 1, "", TokenLevel::comment_line ),
        Token( Token::Type::identifier, "c", test_file, 7, 5, 1, "\n    ", TokenLevel::normal ),
        Token( Token::Type::op, "=", test_file, 7, 7, 1, " ", TokenLevel::normal ),
        Token( Token::Type::identifier, "a", test_file, 7, 9, 1, " ", TokenLevel::normal ),
        Token( Token::Type::op, "+", test_file, 7, 10, 1, "", TokenLevel::normal ),
        Token( Token::Type::identifier, "b", test_file, 7, 11, 1, "", TokenLevel::normal ),
        Token( Token::Type::op, "-", test_file, 7, 13, 1, " ", TokenLevel::normal ),
        Token( Token::Type::number, "2", test_file, 7, 15, 1, " ", TokenLevel::normal ),
        Token( Token::Type::stat_divider, ";", test_file, 7, 16, 1, "", TokenLevel::normal ),
        Token( Token::Type::comment_begin, "/*", test_file, 7, 18, 2, " ", TokenLevel::normal ),
        Token( Token::Type::identifier, "other", test_file, 7, 20, 5, "", TokenLevel::comment ),
        Token( Token::Type::comment_begin, "/*", test_file, 7, 26, 2, " ", TokenLevel::comment ),
        Token( Token::Type::identifier, "comment", test_file, 7, 28, 7, "", TokenLevel::comment ),
        Token( Token::Type::comment_begin, "/*", test_file, 7, 36, 2, " ", TokenLevel::comment ),
        Token( Token::Type::identifier, "with", test_file, 7, 38, 4, "", TokenLevel::comment ),
        Token( Token::Type::comment_end, "*/", test_file, 7, 42, 2, "", TokenLevel::comment ),
        Token( Token::Type::comment_end, "*/", test_file, 7, 44, 2, "", TokenLevel::comment ),
        Token( Token::Type::identifier, "nested", test_file, 7, 47, 6, " ", TokenLevel::comment ),
        Token( Token::Type::comment_end, "*/", test_file, 7, 53, 2, "", TokenLevel::comment ),
        Token( Token::Type::identifier, "c", test_file, 8, 5, 1, "\n\t", TokenLevel::normal ),
        Token( Token::Type::op, "-", test_file, 8, 7, 1, " ", TokenLevel::normal ),
        Token( Token::Type::op, "+=-", test_file, 8, 8, 3, "", TokenLevel::normal ),
        Token( Token::Type::op, "+=-", test_file, 8, 11, 3, "", TokenLevel::normal ),
        Token( Token::Type::op, "--", test_file, 8, 14, 2, "", TokenLevel::normal ),
        Token( Token::Type::op, "-", test_file, 8, 16, 1, "", TokenLevel::normal ),
        Token( Token::Type::identifier, "objletlet", test_file, 8, 17, 9, "", TokenLevel::normal ),
        Token( Token::Type::op, ".", test_file, 8, 26, 1, "", TokenLevel::normal ),
        Token( Token::Type::identifier, "letletdo", test_file, 8, 27, 8, "", TokenLevel::normal ),
        Token( Token::Type::term_begin, "(", test_file, 8, 35, 1, "", TokenLevel::normal ),
        Token( Token::Type::term_end, ")", test_file, 8, 36, 1, "", TokenLevel::normal ),
        Token( Token::Type::stat_divider, ";", test_file, 8, 37, 1, "", TokenLevel::normal ),
        Token( Token::Type::block_end, "}", test_file, 9, 1, 1, "\n", TokenLevel::normal ),
    };

    CHECK( token_list.size() == token_check_list.size() );
    auto token_itr = token_list.begin();
    auto token_check_itr = token_check_list.begin();
    while ( token_itr != token_list.end() && token_check_itr != token_check_list.end() ) {
        CHECK( *token_itr == *token_check_itr );
        token_itr++;
        token_check_itr++;
    }
}

#ifdef NDEBUG
TEST_CASE( "Stress test lexing", "[lexer]" ) {
    auto g_ctx = make_shared<GlobalCtx>();
    sptr<Worker> w_ctx = g_ctx->setup( 1, 0 );

    FileInput fin( make_shared<String>( CMAKE_PROJECT_ROOT "/Test/gibberish.txt" ), w_ctx );
    auto cfg = TokenConfig::get_prelude_cfg();
    cfg.operators.push_back( "." );
    cfg.operators.erase( std::find( cfg.operators.begin(), cfg.operators.end(), "->" ) );
    fin.configure( cfg );

    size_t token_count = 0, identifier_count = 0;
    auto start = std::chrono::steady_clock::now();
    while ( true ) {
        auto token = fin.get_token();
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

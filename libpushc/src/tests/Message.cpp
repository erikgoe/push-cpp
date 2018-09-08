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
#include "libpushc/Message.h"
#include "libpushc/QueryMgr.h"


namespace Catch {
template <>
struct StringMaker<FmtStr> {
    static std::string convert( const FmtStr &str ) {
        String result;
        for ( auto &p : str.get_raw() ) {
            result += p.text;
        }
        return result;
    }
};
template <>
struct StringMaker<FmtStr::Piece> {
    static std::string convert( const FmtStr::Piece &p ) {
        String result;
        result += "\"" + p.text + "\", color" + to_string( static_cast<u32>( p.color ) );
        return result;
    }
};
} // namespace Catch



TEST_CASE( "Message head", "[message]" ) {
    CHECK( get_message_head<MessageType::err_lexer_char_not_allowed>() ==
           FmtStr::Piece( "error L" + to_string( static_cast<u32>( MessageType::err_lexer_char_not_allowed ) ),
                          FmtStr::Color::BoldRed ) +
               FmtStr::Piece( ": Character is not in allowed set of characters.\n", FmtStr::Color::BoldBlack ) );
}

TEST_CASE( "Message body", "[message]" ) {
    auto qm = std::make_shared<QueryMgr>();
    std::shared_ptr<Worker> w_ctx = qm->setup( 1 );
    auto file = std::make_shared<String>( CMAKE_PROJECT_ROOT "/Test/lexer.push" );

    {
        auto output = get_message<MessageType::err_lexer_char_not_allowed>(
            w_ctx, MessageInfo( file, 4, 4, 12, 4, 0, FmtStr::Color::BoldRed ), {} );
        FmtStr check_result;
        check_result += FmtStr::Piece( "error L101", FmtStr::Color::BoldRed );
        check_result += FmtStr::Piece( ": Character is not in allowed set of characters.\n", FmtStr::Color::BoldBlack );
        check_result += FmtStr::Piece( "  --> ", FmtStr::Color::Blue );
        check_result += FmtStr::Piece( CMAKE_PROJECT_ROOT "/Test/lexer.push", FmtStr::Color::Black );
        check_result += FmtStr::Piece( ";", FmtStr::Color::Black );
        check_result += FmtStr::Piece( "4:12..15", FmtStr::Color::BoldRed );
        check_result += FmtStr::Piece( "\n", FmtStr::Color::Black );
        check_result += FmtStr::Piece( "  |\n", FmtStr::Color::Blue );
        check_result += FmtStr::Piece( "4 |", FmtStr::Color::Blue );
        check_result += FmtStr::Piece( "    letlet ", FmtStr::Color::Black );
        check_result += FmtStr::Piece( "a= 4", FmtStr::Color::BoldRed );
        check_result += FmtStr::Piece( "; ", FmtStr::Color::Black );
        check_result += FmtStr::Piece( "\n", FmtStr::Color::Black );
        check_result += FmtStr::Piece( "  |", FmtStr::Color::Blue );
        check_result += FmtStr::Piece( "           ^~~~", FmtStr::Color::BoldRed );
        check_result += FmtStr::Piece( " not allowed character\n", FmtStr::Color::BoldRed );

        // Uncomment these lines to print the message to stdout
        // auto output_cp = output;
        // print_msg_to_stdout( output_cp );

        REQUIRE( output.size() == check_result.size() );
        while ( !output.empty() ) {
            CHECK( output.consume() == check_result.consume() );
        }
    }

    {
        auto output = get_message<MessageType::err_lexer_char_not_allowed>(
            w_ctx, MessageInfo( file, 4, 5, 12, 17, 0, FmtStr::Color::BoldRed ),
            { MessageInfo( file, 3, 4, 3, 18, 0, FmtStr::Color::BoldBlue ) } );
        FmtStr check_result;
        check_result += FmtStr::Piece( "error L101", FmtStr::Color::BoldRed );
        check_result += FmtStr::Piece( ": Character is not in allowed set of characters.\n", FmtStr::Color::BoldBlack );
        check_result += FmtStr::Piece( "  --> ", FmtStr::Color::Blue );
        check_result += FmtStr::Piece( CMAKE_PROJECT_ROOT "/Test/lexer.push", FmtStr::Color::Black );
        check_result += FmtStr::Piece( ";", FmtStr::Color::Black );
        check_result += FmtStr::Piece( "3..4:3+18", FmtStr::Color::BoldBlue );
        check_result += FmtStr::Piece( ";", FmtStr::Color::Black );
        check_result += FmtStr::Piece( "4..5:12+17", FmtStr::Color::BoldRed );
        check_result += FmtStr::Piece( "\n", FmtStr::Color::Black );
        check_result += FmtStr::Piece( "  |\n", FmtStr::Color::Blue );
        check_result += FmtStr::Piece( "3 |", FmtStr::Color::Blue );
        check_result += FmtStr::Piece( "ma", FmtStr::Color::Black );
        check_result += FmtStr::Piece( "in {", FmtStr::Color::BoldBlue );
        check_result += FmtStr::Piece( "\n", FmtStr::Color::Black );
        check_result += FmtStr::Piece( "4 |", FmtStr::Color::Blue );
        check_result += FmtStr::Piece( "    letlet ", FmtStr::Color::BoldBlue );
        check_result += FmtStr::Piece( "a= 4; ", FmtStr::Color::BoldRed );
        check_result += FmtStr::Piece( "\n", FmtStr::Color::Black );
        check_result += FmtStr::Piece( "5 |", FmtStr::Color::Blue );
        check_result += FmtStr::Piece( "    let b =", FmtStr::Color::BoldRed );
        check_result += FmtStr::Piece( "3.2; // commenting 🦄🦓and🦌", FmtStr::Color::Black );
        check_result += FmtStr::Piece( "\n", FmtStr::Color::Black );
        check_result += FmtStr::Piece( "  |", FmtStr::Color::Blue );
        check_result += FmtStr::Piece( "  ^---", FmtStr::Color::BoldBlue );
        check_result += FmtStr::Piece( "\n", FmtStr::Color::BoldBlue );
        check_result += FmtStr::Piece( "  |", FmtStr::Color::Blue );
        check_result += FmtStr::Piece( "--------------", FmtStr::Color::BoldBlue );
        check_result += FmtStr::Piece( " not allowed character\n", FmtStr::Color::BoldBlue );
        check_result += FmtStr::Piece( "  |", FmtStr::Color::Blue );
        check_result += FmtStr::Piece( "*", FmtStr::Color::BoldRed );
        check_result += FmtStr::Piece( "\n", FmtStr::Color::BoldRed );
        check_result += FmtStr::Piece( "  |", FmtStr::Color::Blue );
        check_result += FmtStr::Piece( "           ^~~~~~", FmtStr::Color::BoldRed );
        check_result += FmtStr::Piece( "\n", FmtStr::Color::BoldRed );
        check_result += FmtStr::Piece( "  |", FmtStr::Color::Blue );
        check_result += FmtStr::Piece( "~~~~~~~~~~~", FmtStr::Color::BoldRed );
        check_result += FmtStr::Piece( " not allowed character\n", FmtStr::Color::BoldRed );

        // Uncomment these lines to print the message to stdout
        // auto output_cp = output;
        // print_msg_to_stdout( output_cp );

        REQUIRE( output.size() == check_result.size() );
        while ( !output.empty() ) {
            CHECK( output.consume() == check_result.consume() );
        }
    }
}

TEST_CASE( "Message count", "[message]" ) {
    auto qm = std::make_shared<QueryMgr>();
    std::shared_ptr<Worker> w_ctx = qm->setup( 1 );

    qm->get_global_context()->set_setting<SizeSV>( SettingType::max_errors, 10 );
    qm->get_global_context()->update_global_settings();

    for ( size_t i = 0; i < 10; i++ )
        CHECK_NOTHROW( get_message<MessageType::err_lexer_char_not_allowed>( w_ctx, MessageInfo(), {} ) );
    CHECK_THROWS( get_message<MessageType::err_lexer_char_not_allowed>( w_ctx, MessageInfo(), {} ) );
}

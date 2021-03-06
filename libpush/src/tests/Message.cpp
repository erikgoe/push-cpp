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
#include "libpush/Message.h"
#include "libpush/GlobalCtx.h"

#include "libpush/Message.inl"
#include "libpush/Worker.inl"

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
    CHECK( get_message_head<MessageType::test_message>() ==
           FmtStr::Piece( "error X" + to_string( static_cast<u32>( MessageType::test_message ) ),
                          FmtStr::Color::BoldRed ) +
               FmtStr::Piece( ": Test error message.\n", FmtStr::Color::BoldBlack ) );
}

TEST_CASE( "Message body", "[message]" ) {
    auto g_ctx = make_shared<GlobalCtx>();
    sptr<Worker> w_ctx = g_ctx->setup( 1, 2 );
    auto file = make_shared<String>( CMAKE_PROJECT_ROOT "/Test/lexer.push" );

    { // Simple message
        auto output = get_message<MessageType::test_message>(
            w_ctx, MessageInfo( file, 5, 5, 12, 4, 0, FmtStr::Color::BoldRed ), {} );
        FmtStr check_result;
        check_result += FmtStr::Piece( "error X" + to_string( static_cast<u32>( MessageType::test_message ) ),
                                       FmtStr::Color::BoldRed );
        check_result += FmtStr::Piece( ": Test error message.\n", FmtStr::Color::BoldBlack );
        check_result += FmtStr::Piece( "  --> ", FmtStr::Color::Blue );
        check_result += FmtStr::Piece( CMAKE_PROJECT_ROOT "/Test/lexer.push", FmtStr::Color::Black );
        check_result += FmtStr::Piece( ";", FmtStr::Color::Black );
        check_result += FmtStr::Piece( "5:12..15", FmtStr::Color::BoldRed );
        check_result += FmtStr::Piece( "\n", FmtStr::Color::Black );
        check_result += FmtStr::Piece( "  |\n", FmtStr::Color::Blue );
        check_result += FmtStr::Piece( "5 |", FmtStr::Color::Blue );
        check_result += FmtStr::Piece( "    letlet ", FmtStr::Color::Black );
        check_result += FmtStr::Piece( "a= 4", FmtStr::Color::BoldRed );
        check_result += FmtStr::Piece( "; ", FmtStr::Color::Black );
        check_result += FmtStr::Piece( "\n", FmtStr::Color::Black );
        check_result += FmtStr::Piece( "  |", FmtStr::Color::Blue );
        check_result += FmtStr::Piece( "           ^~~~", FmtStr::Color::BoldRed );
        check_result += FmtStr::Piece( " message for this\n", FmtStr::Color::BoldRed );

        // Uncomment these lines to print the message to stdout
        // auto output_cp = output;
        // print_msg_to_stdout( output_cp );

        CHECK( output.size() == check_result.size() );
        while ( !output.empty() && !check_result.empty() ) {
            CHECK( output.consume() == check_result.consume() );
        }
    }

    { // Message with global notes
        auto output =
            get_message<MessageType::test_message>( w_ctx, MessageInfo( file, 5, 5, 12, 4, 0, FmtStr::Color::BoldRed ),
                                                    { MessageInfo( 1, FmtStr::Color::BoldBlue ) } );
        FmtStr check_result;
        check_result += FmtStr::Piece( "error X" + to_string( static_cast<u32>( MessageType::test_message ) ),
                                       FmtStr::Color::BoldRed );
        check_result += FmtStr::Piece( ": Test error message.\n", FmtStr::Color::BoldBlack );
        check_result += FmtStr::Piece( "  --> ", FmtStr::Color::Blue );
        check_result += FmtStr::Piece( CMAKE_PROJECT_ROOT "/Test/lexer.push", FmtStr::Color::Black );
        check_result += FmtStr::Piece( ";", FmtStr::Color::Black );
        check_result += FmtStr::Piece( "5:12..15", FmtStr::Color::BoldRed );
        check_result += FmtStr::Piece( "\n", FmtStr::Color::Black );
        check_result += FmtStr::Piece( "  |\n", FmtStr::Color::Blue );
        check_result += FmtStr::Piece( "5 |", FmtStr::Color::Blue );
        check_result += FmtStr::Piece( "    letlet ", FmtStr::Color::Black );
        check_result += FmtStr::Piece( "a= 4", FmtStr::Color::BoldRed );
        check_result += FmtStr::Piece( "; ", FmtStr::Color::Black );
        check_result += FmtStr::Piece( "\n", FmtStr::Color::Black );
        check_result += FmtStr::Piece( "  |", FmtStr::Color::Blue );
        check_result += FmtStr::Piece( "           ^~~~", FmtStr::Color::BoldRed );
        check_result += FmtStr::Piece( " message for this\n", FmtStr::Color::BoldRed );
        check_result += FmtStr::Piece( "  Notes:\n", FmtStr::Color::Blue );
        check_result += FmtStr::Piece( "   global information text\n", FmtStr::Color::BoldBlue );

        // Uncomment these lines to print the message to stdout
        // auto output_cp = output;
        // print_msg_to_stdout( output_cp );

        CHECK( output.size() == check_result.size() );
        while ( !output.empty() && !check_result.empty() ) {
            CHECK( output.consume() == check_result.consume() );
        }
    }

    { // Complex overlapping message
        auto output =
            get_message<MessageType::test_message>( w_ctx, MessageInfo( file, 5, 6, 12, 17, 0, FmtStr::Color::BoldRed ),
                                                    { MessageInfo( file, 4, 5, 3, 18, 0, FmtStr::Color::BoldBlue ) } );
        FmtStr check_result;
        check_result += FmtStr::Piece( "error X" + to_string( static_cast<u32>( MessageType::test_message ) ),
                                       FmtStr::Color::BoldRed );
        check_result += FmtStr::Piece( ": Test error message.\n", FmtStr::Color::BoldBlack );
        check_result += FmtStr::Piece( "  --> ", FmtStr::Color::Blue );
        check_result += FmtStr::Piece( CMAKE_PROJECT_ROOT "/Test/lexer.push", FmtStr::Color::Black );
        check_result += FmtStr::Piece( ";", FmtStr::Color::Black );
        check_result += FmtStr::Piece( "4..5:3+18", FmtStr::Color::BoldBlue );
        check_result += FmtStr::Piece( ";", FmtStr::Color::Black );
        check_result += FmtStr::Piece( "5..6:12+17", FmtStr::Color::BoldRed );
        check_result += FmtStr::Piece( "\n", FmtStr::Color::Black );
        check_result += FmtStr::Piece( "  |\n", FmtStr::Color::Blue );
        check_result += FmtStr::Piece( "4 |", FmtStr::Color::Blue );
        check_result += FmtStr::Piece( "ma", FmtStr::Color::Black );
        check_result += FmtStr::Piece( "in {", FmtStr::Color::BoldBlue );
        check_result += FmtStr::Piece( "\n", FmtStr::Color::Black );
        check_result += FmtStr::Piece( "5 |", FmtStr::Color::Blue );
        check_result += FmtStr::Piece( "    letlet ", FmtStr::Color::BoldBlue );
        check_result += FmtStr::Piece( "a= 4; ", FmtStr::Color::BoldRed );
        check_result += FmtStr::Piece( "\n", FmtStr::Color::Black );
        check_result += FmtStr::Piece( "6 |", FmtStr::Color::Blue );
        check_result += FmtStr::Piece( "    let b =", FmtStr::Color::BoldRed );
        check_result += FmtStr::Piece( "3.2; // commenting 🦄🦓and🦌", FmtStr::Color::Black );
        check_result += FmtStr::Piece( "\n", FmtStr::Color::Black );
        check_result += FmtStr::Piece( "  |", FmtStr::Color::Blue );
        check_result += FmtStr::Piece( "  ^---", FmtStr::Color::BoldBlue );
        check_result += FmtStr::Piece( "\n", FmtStr::Color::BoldBlue );
        check_result += FmtStr::Piece( "  |", FmtStr::Color::Blue );
        check_result += FmtStr::Piece( "--------------", FmtStr::Color::BoldBlue );
        check_result += FmtStr::Piece( " message for this\n", FmtStr::Color::BoldBlue );
        check_result += FmtStr::Piece( "  |", FmtStr::Color::Blue );
        check_result += FmtStr::Piece( "*", FmtStr::Color::BoldRed );
        check_result += FmtStr::Piece( "\n", FmtStr::Color::BoldRed );
        check_result += FmtStr::Piece( "  |", FmtStr::Color::Blue );
        check_result += FmtStr::Piece( "           ^~~~~~", FmtStr::Color::BoldRed );
        check_result += FmtStr::Piece( "\n", FmtStr::Color::BoldRed );
        check_result += FmtStr::Piece( "  |", FmtStr::Color::Blue );
        check_result += FmtStr::Piece( "~~~~~~~~~~~", FmtStr::Color::BoldRed );
        check_result += FmtStr::Piece( " message for this\n", FmtStr::Color::BoldRed );

        // Uncomment these lines to print the message to stdout
        // auto output_cp = output;
        // print_msg_to_stdout( output_cp );

        CHECK( output.size() == check_result.size() );
        while ( !output.empty() && !check_result.empty() ) {
            CHECK( output.consume() == check_result.consume() );
        }
    }
}

TEST_CASE( "Message count", "[message]" ) {
    auto g_ctx = make_shared<GlobalCtx>();
    sptr<Worker> w_ctx = g_ctx->setup( 1, 4 );

    g_ctx->set_pref<SizeSV>( PrefType::max_notifications, 10 );
    g_ctx->update_global_prefs();

    for ( size_t i = 0; i < 10; i++ )
        CHECK_NOTHROW( get_message<MessageType::test_message>( w_ctx, MessageInfo(), {} ) );
    CHECK_THROWS( get_message<MessageType::test_message>( w_ctx, MessageInfo(), {} ) );
}

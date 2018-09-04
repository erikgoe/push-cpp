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
} // namespace Catch



TEST_CASE( "Message head", "[message]" ) {
    CHECK( get_message_head<MessageType::error_lexer_char_not_allowed>() ==
           FmtStr::Piece( "error L0", FmtStr::Color::BoldRed ) +
               FmtStr::Piece( ": Character is not in allowed set of characters.\n", FmtStr::Color::BoldBlack ) );
}
TEST_CASE( "Message body", "[message]" ) {
    auto qm = std::make_shared<QueryMgr>();
    std::shared_ptr<Worker> w_ctx = qm->setup( 1 );
    std::shared_ptr<JobCollection<u32>> jc;
    auto file = std::make_shared<String>( CMAKE_PROJECT_ROOT "/Test/lexer.push" );

    CHECK( get_message<MessageType::error_lexer_char_not_allowed>(
               qm, MessageInfo( file, 4, 4, 12, 4, 0, FmtStr::Color::BoldRed ),
               std::array<MessageInfo, 1>{ { MessageInfo( file, 4, 4, 12, 4, 0, FmtStr::Color::BoldRed ) } } ) ==
           FmtStr::Piece( "error" ) + FmtStr::Piece( "error" ) );

}

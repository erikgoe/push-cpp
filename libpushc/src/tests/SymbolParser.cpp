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

#include <regex>
#include "libpushc/tests/stdafx.h"
#include "libpushc/AstParser.h"
#include "libpushc/Prelude.h"
#include "libpushc/Expression.h"
#include "libpushc/Util.h"
#include "libpushc/tests/StringInput.h"

static void test_parser( const String &data, sptr<PreludeConfig> config, JobsBuilder &jb, UnitCtx &parent_ctx ) {
    jb.add_job<void>( [data, config]( Worker &w_ctx ) {
        sptr<SourceInput> input =
            make_shared<StringInput>( make_shared<String>( "test" ), w_ctx.shared_from_this(), data );
        input->configure( config->token_conf );
        w_ctx.unit_ctx()->prelude_conf = *config;

        CrateCtx c_ctx;
        load_base_types( c_ctx, w_ctx.unit_ctx()->prelude_conf );
        load_syntax_rules( w_ctx, c_ctx );

        c_ctx.ast = parse_scope( input, w_ctx, c_ctx, Token::Type::eof, nullptr );
        c_ctx.ast->visit( c_ctx, w_ctx, VisitorPassType::BASIC_SEMANTIC_CHECK );
    } );
}

TEST_CASE( "Symbol parser", "[semantic_parser]" ) {
    auto g_ctx = make_shared<GlobalCtx>();
    auto w_ctx = g_ctx->setup( 1 );

    // Preload prelude config
    auto config = std::make_shared<PreludeConfig>();
    *config = w_ctx->do_query( load_prelude, make_shared<String>( "push" ) )->jobs.back()->to<PreludeConfig>();

    g_ctx->set_pref<StringSV>( PrefType::input_source, "debug" );

    // Pairs of code and the expected (single) message error (or none if MessageType::count)
    std::vector<std::pair<String, MessageType>> test_data = {
        { "", MessageType::count },
        { "", MessageType::count },
    };

    for ( auto &d : test_data ) {
        w_ctx->do_query( test_parser, d.first, config )->jobs.back();
        auto &log = w_ctx->global_ctx()->get_message_log();

        if ( d.second != MessageType::count ) {
            CAPTURE( d.first );
            CHECK( log.size() == 1 );
            if ( log.size() == 1 )
                CHECK( log.front().first == d.second );
        } else {
            CAPTURE( d.first );
            INFO( "Log not empty, but didn't expect any messages" );
            CHECK( log.empty() );
        }

        w_ctx->global_ctx()->clear_messages();
    }
}

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

// Defined in libpushc/AstParser.cpp
sptr<Expr> parse_scope( sptr<SourceInput> &input, Worker &w_ctx, AstCtx &a_ctx, Token::Type end_token,
                        Token *last_token );
void load_base_types( AstCtx &a_ctx, PreludeConfig &cfg );

// Provides token input from a string
class StringInput : public StreamInput {
public:
    StringInput( sptr<String> file, sptr<Worker> w_ctx, const String &data )
            : StreamInput( make_shared<std::basic_istringstream<char>>( data ), file, w_ctx ) {}

    sptr<SourceInput> open_new_file( sptr<String> file, sptr<Worker> w_ctx ) {
        return make_shared<StringInput>( file, w_ctx, "" );
    }

    static bool file_exists( const String &file ) { return false; }
};

void load_default_prelude( SourceInput &input, Worker &w_ctx ) {
    w_ctx.unit_ctx()->prelude_conf =
        w_ctx.do_query( load_prelude, make_shared<String>( "push" ) )->jobs.back()->to<PreludeConfig>();
    input.configure( w_ctx.unit_ctx()->prelude_conf.token_conf );
}


void test_parser( const String &data, JobsBuilder &jb, UnitCtx &parent_ctx ) {
    jb.add_job<sptr<Expr>>( [data]( Worker &w_ctx ) {
        sptr<SourceInput> input =
            make_shared<StringInput>( make_shared<String>( "test" ), w_ctx.shared_from_this(), data );
        load_default_prelude( *input, w_ctx );

        AstCtx a_ctx;
        a_ctx.next_symbol.id = 1;
        load_base_types( a_ctx, w_ctx.unit_ctx()->prelude_conf );
        load_syntax_rules( w_ctx, a_ctx );

        return parse_scope( input, w_ctx, a_ctx, Token::Type::eof, nullptr );
    } );
}

TEST_CASE( "Ast parser", "[ast_parser]" ) {
    auto g_ctx = make_shared<GlobalCtx>();
    auto w_ctx = g_ctx->setup( 1 );

    std::map<String, String> data = { { "a+b;", "GLOBAL {\n SC OP(SYM() + SYM());\n  }" },
                                      { "-a;", "GLOBAL {\n SC OP(- SYM());\n  }" } };

    std::regex symbol_regex( "SYM\\([0-9]*\\)" );
    for ( auto &d : data ) {
        String raw = w_ctx->do_query( test_parser, d.first )->jobs.back()->to<sptr<Expr>>()->get_debug_repr();
        String result = std::regex_replace( raw, symbol_regex, "SYM()" );
        CHECK( result == d.second );
    }
}

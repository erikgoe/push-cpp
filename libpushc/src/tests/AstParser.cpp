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

    std::vector<std::pair<String, String>> data = {
        { "a+b;", "GLOBAL { SC OP(SYM() + SYM()); }" },
        { "/// Basic function without anything special\n function {let val = 5;}",
          "GLOBAL { FUNC(0 SYM() BLOCK { SC BINDING(OP(SYM() = BLOB_LITERAL())); }) }" },
        { "let val = 5 * 3 + 2;",
          "GLOBAL { SC BINDING(OP(SYM() = OP(OP(BLOB_LITERAL() * BLOB_LITERAL()) + BLOB_LITERAL()))); }" },
        { "let v = val = 5 * 3 + 2 + 1",
          "GLOBAL { BINDING(OP(SYM() = OP(SYM() = OP(OP(OP(BLOB_LITERAL() * BLOB_LITERAL()) + BLOB_LITERAL()) + "
          "BLOB_LITERAL())))) }" },
        { "let v = val = 6 + 5 * 3 + 2 + 1",
          "GLOBAL { BINDING(OP(SYM() = OP(SYM() = OP(OP(OP(BLOB_LITERAL() + OP(BLOB_LITERAL() * BLOB_LITERAL())) + "
          "BLOB_LITERAL()) + BLOB_LITERAL())))) }" },
        { "let v = val = 6 + 5 * (3 + 2) + 1",
          "GLOBAL { BINDING(OP(SYM() = OP(SYM() = OP(OP(BLOB_LITERAL() + OP(BLOB_LITERAL() * TERM( OP(BLOB_LITERAL() + "
          "BLOB_LITERAL()) ))) + BLOB_LITERAL())))) }" },
        { "let val = 42 + 5 * (3 + 2) + true",
          "GLOBAL { BINDING(OP(SYM() = OP(OP(BLOB_LITERAL() + OP(BLOB_LITERAL() * TERM( OP(BLOB_LITERAL() + "
          "BLOB_LITERAL()) ))) + BLOB_LITERAL()))) }" },
        { "-5;", "GLOBAL { SC OP(- BLOB_LITERAL()); }" },
        { "let val = 42 + 5 * (3 + 2) + \" this is a string \";",
          "GLOBAL { SC BINDING(OP(SYM() = OP(OP(BLOB_LITERAL() + OP(BLOB_LITERAL() * TERM( OP(BLOB_LITERAL() + "
          "BLOB_LITERAL()) ))) + STR \"this is a string \"))); }" },
        //{ "x = 0  + '3';", "GLOBAL {  }" },
        { "5.4", "GLOBAL { OP(BLOB_LITERAL() . BLOB_LITERAL()) }" },
        { "(a, b, c, d, 5, (4%2));",
          "GLOBAL { SC TUPLE( SYM(), SYM(), SYM(), SYM(), BLOB_LITERAL(), TERM( OP(BLOB_LITERAL() % BLOB_LITERAL()) ), "
          "); }" },
        { "function (a, b) { \n//let val = 42 + 5 * (3 + 2) + \"this is a string\";\n (a, b, c, d, 5, (4%2)); } ",
          "GLOBAL { FUNC(0 TUPLE( SYM(), SYM(), ) SYM() BLOCK { SC TUPLE( SYM(), SYM(), SYM(), SYM(), BLOB_LITERAL(), "
          "TERM( OP(BLOB_LITERAL() % BLOB_LITERAL()) ), ); }) }" },
        { "function(c, d);", "GLOBAL { SC FUNC_HEAD(TUPLE( SYM(), SYM(), ) SYM()); }" },
        { "if true { let val = 4; }",
          "GLOBAL { IF(BLOB_LITERAL() THEN BLOCK { SC BINDING(OP(SYM() = BLOB_LITERAL())); } ) }" },
        { "if var { let val = 4; }",
          "GLOBAL { IF(SYM() THEN BLOCK { SC BINDING(OP(SYM() = BLOB_LITERAL())); } ) }" },
        { "do { function(c, d); } until true;",
          "GLOBAL { SC POST_LOOP(FALSE: BLOB_LITERAL() DO BLOCK { SC FUNC_HEAD(TUPLE( SYM(), SYM(), ) SYM()); } ); }" },
        { "function { let var[4] = [0,1,2,3]; var[2] } ",
          "GLOBAL { FUNC(0 SYM() BLOCK { SC BINDING(OP(ARR_ACC SYM()[BLOB_LITERAL()] = ARRAY[ COMMA( BLOB_LITERAL(), "
          "BLOB_LITERAL(), BLOB_LITERAL(), BLOB_LITERAL(), ) ])); ARR_ACC SYM()[BLOB_LITERAL()] }) }" },
        { "function { if true { let val = 4; } else { let val = 3; } if true  let val = 4; if false let val = 6; else "
          "let val = 5; let var = (if true 4; else 5;); if true let v = 3; else let v = 2; }",
          "GLOBAL { FUNC(0 SYM() BLOCK { IF(BLOB_LITERAL() THEN BLOCK { SC BINDING(OP(SYM() = BLOB_LITERAL())); } ELSE "
          "BLOCK { SC BINDING(OP(SYM() = BLOB_LITERAL())); } ) IF(BLOB_LITERAL() THEN SC BINDING(OP(SYM() = "
          "BLOB_LITERAL())); ) IF(BLOB_LITERAL() THEN SC BINDING(OP(SYM() = BLOB_LITERAL())); ELSE SC BINDING(OP(SYM() "
          "= BLOB_LITERAL())); ) SC BINDING(OP(SYM() = TERM( IF(BLOB_LITERAL() THEN SC BLOB_LITERAL(); ELSE SC "
          "BLOB_LITERAL(); ) ))); IF(BLOB_LITERAL() THEN SC BINDING(OP(SYM() = BLOB_LITERAL())); ELSE SC "
          "BINDING(OP(SYM() = BLOB_LITERAL())); ) }) }" },
        { "let a:struct = { name = \"john\", age=21 }; \n // Global struct\n struct A { val1:int, val2:float } ",
          "GLOBAL { SC BINDING(OP(TYPED(SYM():STRUCT <anonymous> <undefined>) = SET { OP(SYM() = STR \"john\"), "
          "OP(SYM() = BLOB_LITERAL()), })); STRUCT SYM() SET { TYPED(SYM():SYM()), TYPED(SYM():SYM()), } }" },
        { "trait Addable { } struct A { } impl A { } impl Addable for A { }",
          "GLOBAL { TRAIT SYM() BLOCK { } STRUCT SYM() BLOCK { } IMPL SYM() BLOCK { } IMPL SYM() FOR SYM() BLOCK { } "
          "}" },
        //{ "fn { (a,b,c,d); match a 1=>x, 2=>y, 3=>z; }", "GLOBAL {  }" },
        { "let a = { a, b, c };", "GLOBAL { SC BINDING(OP(SYM() = SET { SYM(), SYM(), SYM(), })); }" },
        { "fn (s) { }", "GLOBAL { FUNC(0 TERM( SYM() ) SYM() BLOCK { }) }" },
        { "s.b; A::B; ::C;", "GLOBAL { SC MEMBER(SYM().SYM()); SC SCOPE(SYM()::SYM()); SC SCOPE(<global>::SYM()); }" },
        { "A::B::C; ::A::B;",
          "GLOBAL { SC SCOPE(SCOPE(SYM()::SYM())::SYM()); SC SCOPE(<global>::SCOPE(SYM()::SYM())); }" },
        { "fn (s) { if true let a = 2; else let a = 3; if true { let a = 2 } else { let a = 3 } }",
          "GLOBAL { FUNC(0 TERM( SYM() ) SYM() BLOCK { IF(BLOB_LITERAL() THEN SC BINDING(OP(SYM() = BLOB_LITERAL())); "
          "ELSE SC BINDING(OP(SYM() = BLOB_LITERAL())); ) IF(BLOB_LITERAL() THEN BLOCK { BINDING(OP(SYM() = "
          "BLOB_LITERAL())) } ELSE BLOCK { BINDING(OP(SYM() = BLOB_LITERAL())) } ) }) }" },
        { "let a:&int = type_of s; let a:struct = 4;",
          "GLOBAL { SC BINDING(OP(TYPED(SYM():REF(SYM())) = TYPE_OF(SYM()))); SC BINDING(OP(TYPED(SYM():STRUCT "
          "<anonymous> <undefined>) = BLOB_LITERAL())); }" },
        { "mod modulename;", "GLOBAL { SC MODULE(SYM()); }" },
        { "mod crate::base::module;", "GLOBAL { SC MODULE(SCOPE(SCOPE(SYM()::SYM())::SYM())); }" },
        { "mod ::base::module;", "GLOBAL { SC MODULE(SCOPE(<global>::SCOPE(SYM()::SYM()))); }" },
        { "let result = 40 ${val != 0} / val;",
          "GLOBAL { SC BINDING(OP(SYM() = OP(BLOB_LITERAL() / SYM())$(STST BLOCK { OP(SYM() != BLOB_LITERAL()) }, "
          "))); }" },
        { "scope::fn { let val = a.b; A::S.func().member++; } ",
          "GLOBAL { FUNC(0 SCOPE(SYM()::SYM()) BLOCK { SC BINDING(OP(SYM() = MEMBER(SYM().SYM()))); SC "
          "OP(MEMBER(FUNC_HEAD(UNIT() MEMBER(SCOPE(SYM()::SYM()).SYM())).SYM()) ++); }) }" },
        { "fn<T:type> { let val:T = 3; fn<int>();}",
          "GLOBAL { FUNC(0 TEMPLATE SYM()<TYPED(SYM():SYM())> BLOCK { SC BINDING(OP(TYPED(SYM():SYM()) = "
          "BLOB_LITERAL())); SC FUNC_HEAD(UNIT() TEMPLATE SYM()<SYM()>); }) }" },
        { "fn { unsafe { C::printf(\"hello world\n\");} } ",
          "GLOBAL { FUNC(0 SYM() BLOCK { UNSAFE BLOCK { SC FUNC_HEAD(TERM( STR \"hello world\" ) SCOPE(SYM()::SYM())); "
          "} }) }" },
        { "decl fn1(arg:int); // implicitly public\n pub fn2(arg:int);",
          "GLOBAL { SC DECL(FUNC_HEAD(TERM( TYPED(SYM():SYM()) ) SYM())); SC PUBLIC(FUNC_HEAD(TERM( TYPED(SYM():SYM()) "
          ") SYM())); }" },
        { "scope::B::fn(a) { a.fn();}fn2 {}fn3 (a:int, b:int) -> int { [&]() { a++ } arr[2];}",
          "GLOBAL { FUNC(0 TERM( SYM() ) SCOPE(SCOPE(SYM()::SYM())::SYM()) BLOCK { SC FUNC_HEAD(UNIT() "
          "MEMBER(SYM().SYM())); }) FUNC(0 SYM() BLOCK { }) FUNC(0 TUPLE( TYPED(SYM():SYM()), TYPED(SYM():SYM()), ) "
          "SYM() -> SYM() BLOCK { FUNC(0 UNIT() ARRAY[ TOKEN 14 \"&\" ] BLOCK { OP(SYM() ++) }) SC ARR_ACC "
          "SYM()[BLOB_LITERAL()]; }) }" },
        //{ "#not_inline fn { println!(\"Hello World\"); } ", "GLOBAL {  }" },
        { "f(g<a>(c)); fn<A, B>() { a+fn(a+b, c); }",
          "GLOBAL { SC FUNC_HEAD(TERM( FUNC_HEAD(TERM( SYM() ) TEMPLATE SYM()<SYM()>) ) SYM()); FUNC(0 UNIT() TEMPLATE "
          "SYM()<COMMA( SYM(), SYM(), )> BLOCK { SC OP(SYM() + FUNC_HEAD(TUPLE( OP(SYM() + SYM()), SYM(), ) SYM())); "
          "}) }" },
        { "f(g<a>(c)); fn<A, B> { a+fn(a+b, c); }",
          "GLOBAL { SC FUNC_HEAD(TERM( FUNC_HEAD(TERM( SYM() ) TEMPLATE SYM()<SYM()>) ) SYM()); FUNC(0 TEMPLATE "
          "SYM()<COMMA( SYM(), SYM(), )> BLOCK { SC OP(SYM() + FUNC_HEAD(TUPLE( OP(SYM() + SYM()), SYM(), ) SYM())); "
          "}) }" },
        { "a..b; a..; ..b; a..=b; ..=b; ",
          "GLOBAL { SC RANGE EXCLUDE SYM()..SYM(); SC RANGE EXCLUDE_FROM SYM(); SC RANGE EXCLUDE_TO SYM(); SC RANGE "
          "INCLUDE SYM()..SYM(); SC RANGE INCLUDE_TO SYM(); }" },
        { "Vec1<Vec2<a> >;", "GLOBAL { SC TEMPLATE SYM()<TEMPLATE SYM()<SYM()>>; }" },
        { "Vec1<Vec2<a>>; a >> b;", "GLOBAL { SC TEMPLATE SYM()<TEMPLATE SYM()<SYM()>>; SC OP(SYM() >> SYM()); }" },
        { "Vec1<Vec2<a>>(); Vec<Vec<Vec<Vec<Vec<a>, b>>>>(); Vec<Vec<Vec<Vec<Vec<a>, b>>>>(); a >> b; "
          "Vec1<Vec2<a+b>>(); ",
          "GLOBAL { SC FUNC_HEAD(UNIT() TEMPLATE SYM()<TEMPLATE SYM()<SYM()>>); SC FUNC_HEAD(UNIT() TEMPLATE "
          "SYM()<TEMPLATE SYM()<TEMPLATE SYM()<TEMPLATE SYM()<COMMA( TEMPLATE SYM()<SYM()>, SYM(), )>>>>); SC "
          "FUNC_HEAD(UNIT() TEMPLATE SYM()<TEMPLATE SYM()<TEMPLATE SYM()<TEMPLATE SYM()<COMMA( TEMPLATE SYM()<SYM()>, "
          "SYM(), )>>>>); SC OP(SYM() >> SYM()); SC FUNC_HEAD(UNIT() TEMPLATE SYM()<TEMPLATE SYM()<OP(SYM() + "
          "SYM())>>); }" }
    };

    std::regex symbol_regex( "SYM\\([0-9]*\\)" ), blob_literal_regex( "BLOB_LITERAL\\([0-9a-f]*:[0-9]*\\)" );
    for ( auto &d : data ) {
        String str = w_ctx->do_query( test_parser, d.first )->jobs.back()->to<sptr<Expr>>()->get_debug_repr();
        str = std::regex_replace( str, symbol_regex, "SYM()" );
        str = std::regex_replace( str, blob_literal_regex, "BLOB_LITERAL()" );
        str.replace_all( "\n", "" );
        str.replace_all( "  ", " " );
        CHECK( str == d.second );
    }
}

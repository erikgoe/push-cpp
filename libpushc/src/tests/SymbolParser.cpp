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

static void test_parser( const String &data, sptr<PreludeConfig> config, sptr<std::vector<VisitorPassType>> passes,
                         JobsBuilder &jb, UnitCtx &parent_ctx ) {
    jb.add_job<sptr<CrateCtx>>( [data, config, passes]( Worker &w_ctx ) {
        sptr<SourceInput> input =
            make_shared<StringInput>( make_shared<String>( "test" ), w_ctx.shared_from_this(), data );
        input->configure( config->token_conf );
        w_ctx.unit_ctx()->prelude_conf = *config;

        auto c_ctx = make_shared<CrateCtx>();
        load_base_types( *c_ctx, w_ctx, w_ctx.unit_ctx()->prelude_conf );
        load_syntax_rules( w_ctx, *c_ctx );

        *c_ctx->ast = parse_scope( input, w_ctx, *c_ctx, Token::Type::eof, nullptr );
        AstNode dummy_root_parent{ ExprType::none };
        for ( auto &pass : *passes )
            c_ctx->ast->visit( *c_ctx, w_ctx, pass, dummy_root_parent, false );
        return c_ctx;
    } );
}

TEST_CASE( "Basic semantic check", "[semantic_parser]" ) {
    auto g_ctx = make_shared<GlobalCtx>();
    auto w_ctx = g_ctx->setup( 1 );

    // Preload prelude config
    auto config = make_shared<PreludeConfig>();
    *config = w_ctx->do_query( load_prelude, make_shared<String>( "push" ) )->jobs.back()->to<PreludeConfig>();

    g_ctx->set_pref<StringSV>( PrefType::input_source, "debug" );
    auto passes = make_shared<std::vector<VisitorPassType>>();
    passes->push_back( VisitorPassType::BASIC_SEMANTIC_CHECK );

    // Pairs of code and the expected (single) message error (or none if MessageType::count)
    std::vector<std::pair<String, MessageType>> test_data = {
        { "", MessageType::count },
        { "+;", MessageType::err_orphan_token },
        { "a;", MessageType::count },
        { "symbol", MessageType::err_unfinished_expr },
        { "a;;", MessageType::err_semicolon_without_meaning },
        { "{a,b}", MessageType::count },
        { "{a;b}", MessageType::count },
        { "{a;b;}", MessageType::count },
        { "{a b}", MessageType::err_unfinished_expr },
        { "[1,2];", MessageType::count },
        { "[a b];", MessageType::err_unfinished_expr },
        { "fn();", MessageType::count },
        { "fn(a);", MessageType::count },
        { "fn(1,2);", MessageType::count },
        { "fn(a,b);", MessageType::count },
        { "[]{}", MessageType::count },
        { "[]() {}", MessageType::count },
        //{ "1();", MessageType::err_expected_symbol }, TODO move these into type checks
        //{ "1(){}", MessageType::err_expected_symbol },
        { "fn {}", MessageType::count },
        { "fn() {}", MessageType::count },
        { "fn(1,b){}", MessageType::err_expected_symbol },
        { "fn()->T{}", MessageType::count },
        { "fn(a,b)->T{}", MessageType::count },
        { "let a = 4;", MessageType::count },
        { "use a = b;", MessageType::count },
        { "use 1=b;", MessageType::err_expected_symbol },
        { "use a=1;", MessageType::err_expected_symbol },
        { "match a {1=>b, 2=>c}", MessageType::count },
        { "match a 1=>b, 2=>c;", MessageType::count },
        { "match a if c d;", MessageType::err_expected_implication },
        { "match a a;", MessageType::err_expected_implication },
        { "match a {a,b}", MessageType::err_expected_implication },
        { "match a 1+b, 2+c;", MessageType::err_expected_implication },
        { "x[0];", MessageType::count },
        { "x[0,1];", MessageType::err_expected_only_one_parameter },
        { "struct A {}", MessageType::count },
        { "struct A { a,b }", MessageType::count },
        { "struct A { a:T1,b:T1 }", MessageType::count },
        { "struct A { pub a }", MessageType::count },
        { "struct A { pub a:T }", MessageType::count },
        { "struct A { pub a:T1, b:T1 }", MessageType::count },
        { "struct A if c d;", MessageType::err_expected_symbol },
        { "struct A { 1 }", MessageType::err_expected_symbol },
        { "struct A { 1,2 }", MessageType::err_expected_symbol },
        { "struct A { 1:T }", MessageType::err_expected_symbol },
        { "struct A { a:1 }", MessageType::err_expected_symbol },
        { "struct A { fn(){} }", MessageType::err_method_not_allowed },
        { "struct A { fn() }", MessageType::err_method_not_allowed },
        { "struct A { pub 1 }", MessageType::err_expected_symbol },
        { "struct A { pub 1:T }", MessageType::err_expected_symbol },
        { "struct A { pub 1, 2 }", MessageType::err_expected_symbol },
        { "struct A { pub fn(){} }", MessageType::err_method_not_allowed },
        { "trait A {}", MessageType::count },
        { "trait A { f() }", MessageType::count },
        { "trait A { f(),g() }", MessageType::count },
        { "trait A { f() {}, g() {} }", MessageType::count },
        { "trait A { pub f() {}, g() {} }", MessageType::count },
        { "trait A if a b;", MessageType::err_expected_function_head },
        { "trait A { a }", MessageType::err_expected_function_head },
        { "trait A { a{}, b }", MessageType::err_expected_function_head },
        { "impl A {}", MessageType::count },
        { "impl A for B {}", MessageType::count },
        { "impl A { f {} }", MessageType::count },
        { "impl A { f{}, g() {} }", MessageType::count },
        { "impl A { pub f() {}, g() {} }", MessageType::count },
        { "impl A if a b;", MessageType::err_expected_function_definition },
        { "impl A { f() }", MessageType::err_expected_function_definition },
        { "impl A { a }", MessageType::err_expected_function_definition },
        { "decl fn();", MessageType::count },
        { "pub fn();", MessageType::count },
        { "pub fn() {};", MessageType::count },
        { "pub a;", MessageType::count },
        { "pub a:T;", MessageType::count },
        { "pub a:1;", MessageType::err_expected_symbol },
        { "pub 1:T;", MessageType::err_expected_symbol },
        { "pub 1;", MessageType::err_expected_symbol },
        { "#annotation(param) fn(){}", MessageType::count },
        { "macro!();", MessageType::count },
        { "macro!{};", MessageType::count },
        { "fn<T>();", MessageType::count },
        { "fn<1+2>();", MessageType::count },
    };

    for ( auto &d : test_data ) {
        auto c_ctx = w_ctx->do_query( test_parser, d.first, config, passes )->jobs.back()->to<sptr<CrateCtx>>();
        auto &log = w_ctx->global_ctx()->get_message_log();

        CAPTURE( c_ctx->ast->get_debug_repr() );
        CAPTURE( d.first );
        if ( d.second != MessageType::count ) {
            CHECK( log.size() == 1 );
            if ( log.size() == 1 )
                CHECK( log.front().first == d.second );
        } else {
            INFO( "Log not empty, but didn't expect any messages" );
            if ( !log.empty() ) {
                INFO( "Error number: " + to_string( static_cast<size_t>( log.front().first ) ) );
                CHECK( log.empty() );
            }
        }

        w_ctx->global_ctx()->clear_messages();
    }
}

TEST_CASE( "First transformation", "[semantic_parser]" ) {
    auto g_ctx = make_shared<GlobalCtx>();
    auto w_ctx = g_ctx->setup( 1 );

    // Preload prelude config
    auto config = make_shared<PreludeConfig>();
    *config = w_ctx->do_query( load_prelude, make_shared<String>( "push" ) )->jobs.back()->to<PreludeConfig>();

    g_ctx->set_pref<StringSV>( PrefType::input_source, "debug" );
    auto passes = make_shared<std::vector<VisitorPassType>>();
    passes->push_back( VisitorPassType::FIRST_TRANSFORMATION );

    // Pairs of code and the expected transformed AST
    std::vector<std::pair<String, String>> test_data = {
        { "{}", "GLOBAL { GLOBAL { } }" },
        { "{{}}{}", "GLOBAL { GLOBAL { GLOBAL { } } GLOBAL { } }" },
        { "fn();", "GLOBAL { FUNC_DECL(UNIT() SYM()) }" },
        { "pub fn();", "GLOBAL { FUNC_DECL(UNIT() SYM()) }" },
        { "struct A { pub a, b }", "GLOBAL { STRUCT SYM() GLOBAL { SYM() SYM() } }" },
        { "struct B { pub a, pub b }", "GLOBAL { STRUCT SYM() GLOBAL { SYM() SYM() } }" },
        { "trait C { pub fn() }", "GLOBAL { TRAIT SYM() GLOBAL { FUNC_DECL(UNIT() SYM()) } }" },
        { "struct A { {}, a }", "GLOBAL { STRUCT SYM() GLOBAL { GLOBAL { } SYM() } }" },
        { "fn() { {} a }", "GLOBAL { FUNC(UNIT() SYM() IMP { IMP { UNIT() } SYM() }) }" },
        { "fn() a;", "GLOBAL { FUNC(UNIT() SYM() IMP { SYM() }) }" },
        { "x = A{}", "GLOBAL { OP(SYM() = STRUCT_INIT(SYM() SET { })) }" },
        { "x = A{x}", "GLOBAL { OP(SYM() = STRUCT_INIT(SYM() SET { SYM(), })) }" },
        { "x = A{x,y}", "GLOBAL { OP(SYM() = STRUCT_INIT(SYM() SET { SYM(), SYM(), })) }" },
        { "A{}", "GLOBAL { FUNC(SYM() IMP { UNIT() }) }" },
        { "x = A(){}", "GLOBAL { OP(SYM() = FUNC(UNIT() SYM() IMP { UNIT() })) }" },
        { "x = A(){x}", "GLOBAL { OP(SYM() = FUNC(UNIT() SYM() IMP { SYM() })) }" },
        { "x = A{x;y}", "GLOBAL { OP(SYM() = FUNC(SYM() IMP { SYM() SYM() })) }" },
        { "struct A a;", "GLOBAL { STRUCT SYM() GLOBAL { SYM() } }" },
        { "struct A { a }", "GLOBAL { STRUCT SYM() GLOBAL { SYM() } }" },
        { "struct A a, b;", "GLOBAL { STRUCT SYM() GLOBAL { SYM() SYM() } }" },
        { "struct A { a, b }", "GLOBAL { STRUCT SYM() GLOBAL { SYM() SYM() } }" },
        { "match x 1=>a;", "GLOBAL { MATCH(SYM() WITH SET { OP(BLOB_LITERAL() => SYM()), }) }" },
        { "match x { 1=>a }", "GLOBAL { MATCH(SYM() WITH SET { OP(BLOB_LITERAL() => SYM()), }) }" },
        { "match x 1=>a, 2=>b;",
          "GLOBAL { MATCH(SYM() WITH SET { OP(BLOB_LITERAL() => SYM()), OP(BLOB_LITERAL() => SYM()), }) }" },
        { "match x { 1=>a, 2=>b }",
          "GLOBAL { MATCH(SYM() WITH SET { OP(BLOB_LITERAL() => SYM()), OP(BLOB_LITERAL() => SYM()), }) }" },
        { "if a b; else c;", "GLOBAL { IF(SYM() THEN IMP { SYM() } ELSE IMP { SYM() } ) }" },
        { "if a { b; } else { c; }", "GLOBAL { IF(SYM() THEN IMP { SYM() UNIT() } ELSE IMP { SYM() UNIT() } ) }" },
        { "a[1];", "GLOBAL { ARR_ACC SYM()[BLOB_LITERAL()] }" },
        { "fn<A, B>() {}", "GLOBAL { FUNC(UNIT() TEMPLATE SYM()<SYM(), SYM(), > IMP { UNIT() }) }" },
        { "fn() { fn(); }", "GLOBAL { FUNC(UNIT() SYM() IMP { FN_CALL(UNIT() SYM()) UNIT() }) }" },
        { "#annotation() fn() a;", "GLOBAL { FUNC(UNIT() SYM() IMP { SYM() })#(ANNOTATE(SYM() UNIT()), ) }" },
        { "trait C { #annotation() fn() }",
          "GLOBAL { TRAIT SYM() GLOBAL { FUNC_DECL(UNIT() SYM())#(ANNOTATE(SYM() UNIT()), ) } }" },
        { "use a = b; struct A {}", "GLOBAL { STRUCT SYM() GLOBAL { } }" },
    };

    std::regex symbol_regex( "SYM\\([0-9]* [a-zA-Z0-9_]*\\)" ), blob_literal_regex( "BLOB_LITERAL\\([0-9a-f]*\\)" );
    for ( auto &d : test_data ) {
        auto c_ctx = w_ctx->do_query( test_parser, d.first, config, passes )->jobs.back()->to<sptr<CrateCtx>>();
        String str = c_ctx->ast->get_debug_repr();
        str = std::regex_replace( str, symbol_regex, "SYM()" );
        str = std::regex_replace( str, blob_literal_regex, "BLOB_LITERAL()" );
        str.replace_all( "\n", "" );
        str.replace_all( "  ", " " );
        CHECK( str == d.second );
    }
}

TEST_CASE( "Symbol discovery", "[semantic_parser]" ) {
    auto g_ctx = make_shared<GlobalCtx>();
    auto w_ctx = g_ctx->setup( 1 );

    // Preload prelude config
    auto config = make_shared<PreludeConfig>();
    *config = w_ctx->do_query( load_prelude, make_shared<String>( "push" ) )->jobs.back()->to<PreludeConfig>();

    g_ctx->set_pref<StringSV>( PrefType::input_source, "debug" );
    auto passes = make_shared<std::vector<VisitorPassType>>();
    passes->push_back( VisitorPassType::FIRST_TRANSFORMATION );
    passes->push_back( VisitorPassType::SYMBOL_DISCOVERY );

    // Pairs of code and the expected transformed AST
    String test_file =
        "#prelude(push)"
        "use op = std::op;"
        "struct A {"
        "    foo:u32,"
        "    bar:u32,"
        "}"
        "struct A {"
        "    foobar:f32"
        "}"
        "trait op::Add {"
        "    add(self, other)"
        "}"
        "impl op::Add for A {"
        "    add(self, other) {"
        "        foo + other.foo"
        "    }"
        "}"
        "mod submodule {"
        "    struct B {"
        "        a,b"
        "    }"
        "}"
        "base::B::A::b::a::function (a, b) -> u32 {"
        "    {"
        "        let a:A = {1,2};"
        "    }"
        "    "
        "    fn {"
        "        a+b"
        "    }"
        "    fn()"
        "}"
        "sub::new_fn() {"
        "}"
        "other_fn() {"
        "}"
        "another_sub::fn() {"
        "}";

    // Parse
    auto c_ctx = w_ctx->do_query( test_parser, test_file, config, passes )->jobs.back()->to<sptr<CrateCtx>>();

    // Check
    auto graph_start = std::find_if( c_ctx->symbol_graph.begin(), c_ctx->symbol_graph.end(),
                                     []( const SymbolGraphNode &node ) { return node.identifier.name == "A"; } );
    u32 graph_start_idx = graph_start - c_ctx->symbol_graph.begin();
    auto type_start =
        std::find_if( c_ctx->type_table.begin(), c_ctx->type_table.end(),
                      [&graph_start_idx]( const TypeTableEntry &entry ) { return entry.symbol == graph_start_idx; } );
    u32 type_start_idx = type_start - c_ctx->type_table.begin();
    REQUIRE( c_ctx->symbol_graph.end() - graph_start == 25 ); // expected graph element count
    REQUIRE( c_ctx->type_table.end() - type_start == 10 ); // expected type table element count
    u32 op_scope = std::find_if( c_ctx->symbol_graph.begin(), c_ctx->symbol_graph.end(),
                                 []( const SymbolGraphNode &node ) { return node.identifier.name == "op"; } ) -
                   c_ctx->symbol_graph.begin();

    // Create expected data
    u32 type_ctr = graph_start_idx;
    std::vector<SymbolGraphNode> expected_graph_nodes = {
        SymbolGraphNode{ 1, {}, {}, SymbolIdentifier{ "A" }, {}, false, type_start_idx, c_ctx->struct_type },
        SymbolGraphNode{
            op_scope, {}, {}, SymbolIdentifier{ "Add" }, {}, false, type_start_idx + 1, c_ctx->trait_type },
        SymbolGraphNode{
            graph_start_idx + 1, {}, {}, SymbolIdentifier{ "add" }, {}, false, type_start_idx + 2, c_ctx->fn_type },
        SymbolGraphNode{
            graph_start_idx, {}, {}, SymbolIdentifier{ "add" }, {}, false, type_start_idx + 3, c_ctx->fn_type },
        SymbolGraphNode{ graph_start_idx + 3, {}, {}, SymbolIdentifier{ "" }, {}, false, 0, 0 },
        SymbolGraphNode{ 1, {}, {}, SymbolIdentifier{ "submodule" }, {}, false, 0, c_ctx->mod_type },
        SymbolGraphNode{
            graph_start_idx + 5, {}, {}, SymbolIdentifier{ "B" }, {}, false, type_start_idx + 4, c_ctx->struct_type },
        SymbolGraphNode{ 1, {}, {}, SymbolIdentifier{ "base" }, {}, false, 0, c_ctx->mod_type },
        SymbolGraphNode{ graph_start_idx + 7, {}, {}, SymbolIdentifier{ "B" }, {}, false, 0, c_ctx->mod_type },
        SymbolGraphNode{ graph_start_idx + 8, {}, {}, SymbolIdentifier{ "A" }, {}, false, 0, c_ctx->mod_type },
        SymbolGraphNode{ graph_start_idx + 9, {}, {}, SymbolIdentifier{ "b" }, {}, false, 0, c_ctx->mod_type },
        SymbolGraphNode{ graph_start_idx + 10, {}, {}, SymbolIdentifier{ "a" }, {}, false, 0, c_ctx->mod_type },
        SymbolGraphNode{ graph_start_idx + 11,
                         {},
                         {},
                         SymbolIdentifier{ "function" },
                         {},
                         false,
                         type_start_idx + 5,
                         c_ctx->fn_type },
        SymbolGraphNode{ graph_start_idx + 12, {}, {}, SymbolIdentifier{ "" }, {}, false, 0, 0 },
        SymbolGraphNode{ graph_start_idx + 13, {}, {}, SymbolIdentifier{ "" }, {}, false, 0, 0 },
        SymbolGraphNode{
            graph_start_idx + 13, {}, {}, SymbolIdentifier{ "fn" }, {}, false, type_start_idx + 6, c_ctx->fn_type },
        SymbolGraphNode{ graph_start_idx + 15, {}, {}, SymbolIdentifier{ "" }, {}, false, 0, 0 },
        SymbolGraphNode{ 1, {}, {}, SymbolIdentifier{ "sub" }, {}, false, 0, c_ctx->mod_type },
        SymbolGraphNode{
            graph_start_idx + 17, {}, {}, SymbolIdentifier{ "new_fn" }, {}, false, type_start_idx + 7, c_ctx->fn_type },
        SymbolGraphNode{ graph_start_idx + 18, {}, {}, SymbolIdentifier{ "" }, {}, false, 0, 0 },
        SymbolGraphNode{ 1, {}, {}, SymbolIdentifier{ "other_fn" }, {}, false, type_start_idx + 8, c_ctx->fn_type },
        SymbolGraphNode{ graph_start_idx + 20, {}, {}, SymbolIdentifier{ "" }, {}, false, 0, 0 },
        SymbolGraphNode{ 1, {}, {}, SymbolIdentifier{ "another_sub" }, {}, false, 0, c_ctx->mod_type },
        SymbolGraphNode{
            graph_start_idx + 22, {}, {}, SymbolIdentifier{ "fn" }, {}, false, type_start_idx + 9, c_ctx->fn_type },
        SymbolGraphNode{ graph_start_idx + 23, {}, {}, SymbolIdentifier{ "" }, {}, false, 0, 0 },
    };

    for ( size_t i = graph_start_idx; i < c_ctx->symbol_graph.size(); i++ ) {
        SymbolGraphNode &generated = c_ctx->symbol_graph[i];
        SymbolGraphNode &expected = expected_graph_nodes[i - graph_start_idx];
        INFO( "At test index " + to_string( i - graph_start_idx ) );
        CHECK( generated.parent == expected.parent );
        CHECK( generated.identifier.name == expected.identifier.name );
        CHECK( generated.value == expected.value );
        CHECK( generated.type == expected.type );
    }
}

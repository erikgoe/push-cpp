// Copyright 2019 Erik Götzfried
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

#include "libpushc/stdafx.h"
#include "libpushc/AstParser.h"
#include "libpushc/Prelude.h"

using TT = Token::Type;

// Consume any amount of comments if any
void consume_comment( SourceInput &input ) {
    while ( input.preview_token().type == TT::comment_begin ) {
        input.get_token(); // consume consume comment begin
        // Consume any token until the comment ends
        while ( input.get_token().type != TT::comment_end )
            ;
    }
}

// Check for an expected token and returns it. All preceding comments are ignored
template <MessageType MesT>
Token expect_token_or_comment( TT type, SourceInput &input, Worker &worker ) {
    consume_comment( input );
    Token t = input.get_token();
    if ( t.type != type ) {
        worker.print_msg<MesT>( MessageInfo( t, 0, FmtStr::Color::Red ), std::vector<MessageInfo>{},
                                Token::get_name( type ) );
    }
    return t;
}

// Parse a following string. Including the string begin and end tokens
String extract_string( SourceInput &input, Worker &worker ) {
    expect_token_or_comment<MessageType::err_expected_string>( TT::string_begin, input, worker );

    String content;
    Token t;
    while ( ( t = input.get_token() ).type != TT::string_end ) {
        if ( t.type == TT::eof ) {
            worker.print_msg<MessageType::err_unexpected_eof>( MessageInfo( t, 0, FmtStr::Color::Red ) );
            break;
        }
        content += t.leading_ws + t.content;
    }
    content += t.leading_ws;

    return content;
}

// Checks if a prelude is defined and loads the proper prelude.
// Should be called at the beginning of a file
void select_prelude( SourceInput &input, Worker &worker ) {
    // Load prelude-prelude first
    log( "" );
    worker.unit_ctx()->prelude_conf =
        worker.do_query( load_prelude, make_shared<String>( "prelude" ) )->jobs.front()->to<PreludeConfig>();
    input.configure( worker.unit_ctx()->prelude_conf.token_conf );

    // Consume any leading comment
    consume_comment( input );

    // Parse prelude command
    Token t = input.preview_token();
    if ( t.type == TT::op && t.content == "#" ) {
        t = input.preview_next_token();
        if ( t.type != TT::identifier || t.content != "prelude" ) {
            worker.print_msg<MessageType::err_malformed_prelude_command>( MessageInfo( t, 0, FmtStr::Color::Red ),
                                                                          std::vector<MessageInfo>{},
                                                                          Token::get_name( TT::identifier ) );
        }
        input.get_token(); // consume
        input.get_token(); // consume

        // Found a prelude
        expect_token_or_comment<MessageType::err_malformed_prelude_command>( TT::term_begin, input, worker );

        t = input.preview_token();
        if ( t.type == TT::identifier ) {
            input.get_token(); // consume
            worker.do_query( load_prelude, make_shared<String>( t.content ) );
        } else if ( t.type == TT::string_begin ) {
            String path = extract_string( input, worker );
            worker.do_query( load_prelude_file, make_shared<String>( path ) );
        } else {
            // invalid prelude identifier
            worker.print_msg<MessageType::err_unexpected_eof>( MessageInfo( t, 0, FmtStr::Color::Red ) );
            // load default prelude as fallback
            worker.do_query( load_prelude, make_shared<String>( "push" ) );
        }

        expect_token_or_comment<MessageType::err_malformed_prelude_command>( TT::term_end, input, worker );
    } else {
        // Load default prelude
        worker.do_query( load_prelude, make_shared<String>( "push" ) );
    }
}

void get_ast( JobsBuilder &jb, UnitCtx &parent_ctx ) {
    jb.add_job<void>( []( Worker &w_ctx ) { w_ctx.do_query( parse_ast ); } );
}

void parse_ast( JobsBuilder &jb, UnitCtx &parent_ctx ) {
    jb.add_job<void>( []( Worker &w_ctx ) {
        auto input = get_source_input( *w_ctx.unit_ctx()->root_file, w_ctx );
        if ( !input )
            return;

        select_prelude( *input, w_ctx );

        
    } );
}

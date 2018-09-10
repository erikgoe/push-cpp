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
#include "libpushc/QueryMgr.h"
#include "libpushc/Message.h"

void get_token_list( const String file, JobsBuilder &jb, QueryMgr &qm ) {
    jb.add_job<std::list<String>>( [file]( Worker &w_ctx ) {
        w_ctx.set_curr_job_volatile();
        std::list<String> token;
        token.push_back( file.substr( 0, file.find( "." ) ) );
        token.push_back( file.substr( file.find( "." ), 1 ) );
        token.push_back( file.substr( file.find( "." ) + 1 ) );
        return token;
    } );
}
void get_binary_from_source( const std::list<String> files, JobsBuilder &jb, QueryMgr &qm ) {
    for ( auto &file : files ) {
        jb.add_job<std::list<String>>( [file]( Worker &w_ctx ) {
            std::list<String> b;
            auto jc = w_ctx.query( get_token_list, file )->execute( w_ctx );
            // do sth with the data
            auto job = jc->jobs.front()->to<std::list<String>>();
            for ( auto &line : job )
                b.push_back( line + "_token" );

            return b;
        } );
    }
}
u32 compile_binary( const std::list<String> files, JobsBuilder &jb, QueryMgr &qm ) {
    jb.add_job<String>( [files]( Worker &w_ctx ) {
        auto jc = w_ctx.query( get_binary_from_source, files );
        Sleep( 10. );
        jc->execute( w_ctx );
        String stream;
        for ( auto &job : jc->jobs ) {
            for ( auto &t : job->to<std::list<String>>() ) {
                stream += t + " ";
            }
        }
        return stream;
    } );
    return 0xD42;
}

TEST_CASE( "Infrastructure", "[basic_workflow]" ) {
    auto qm = std::make_shared<QueryMgr>();

    // LOG( "Start pass" );
    std::shared_ptr<Worker> w_ctx;
    std::shared_ptr<JobCollection<u32>> jc;
    String check_result;
    SECTION( "simple files" ) {
        SECTION( "single threaded" ) { w_ctx = qm->setup( 20 ); }
        SECTION( "multithreaded" ) { w_ctx = qm->setup( 4, 20 ); }
        jc = w_ctx->query( compile_binary, std::list<String>{ "somefile.push", "another.push", "last.push" } );
        check_result =
            "somefile_token ._token push_token another_token ._token push_token last_token ._token push_token ";
    }
    SECTION( "multi files" ) {
        // SECTION( "single threaded" ) { w_ctx = qm->setup( 1, 1024 ); }
        SECTION( "multithreaded" ) { w_ctx = qm->setup( 16, 1024 ); }
        auto filelist = std::list<String>();
        for ( char c = '@'; c <= 'Z'; c++ ) {
            for ( char c2 = '@'; c2 <= 'Z'; c2++ ) {
                // for ( char c3 = '@'; c3 <= 'Z'; c3++ ) { // use this additional loops for stress testing
                // for ( char c4 = '@'; c4 <= 'Z'; c4++ ) {
                String str;
                str += c;
                str += c2;
                // str += c3;
                // str += c4;
                filelist.push_back( str + ".push" );
                check_result += str + "_token ._token push_token ";
                //}
                //}
            }
        }
        jc = w_ctx->query( compile_binary, filelist );
    }
    // Sleep( 1000. );
    jc->execute( *w_ctx );
    String result = jc->jobs.front()->to<String>();
    CHECK( result == check_result );
    CHECK( jc->get() == 0xD42 );
    qm->wait_finished();
}

TEST_CASE( "Query caching", "[basic_workflow]" ) {
    auto qm = std::make_shared<QueryMgr>();
    std::shared_ptr<Worker> w_ctx = qm->setup( 1, 8 );

    w_ctx->query( get_binary_from_source, std::list<String>{"a.b"} )->execute( *w_ctx )->wait();
    w_ctx->query( get_binary_from_source, std::list<String>{"a.b"} )->execute( *w_ctx )->wait();
    qm->reset();
    w_ctx->query( get_binary_from_source, std::list<String>{"a.b"} )->execute( *w_ctx )->wait();
    // this should print 1x "Using cached..." and 2x "Update cached..."
}

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
#define CATCH_CONFIG_MAIN // Catch provides main()
#include "catch/catch.hpp"

#include "libpushc/QueryMgr.h"


void get_token_list( const String &file, JobsBuilder &jb, QueryMgr &qm ) {
    jb.add_job<std::list<String>>( [&]() {
        std::list<String> token;
        token.push_back( file.substr( 0, file.find( "." ) ) );
        token.push_back( file.substr( file.find( "." ), 1 ) );
        token.push_back( file.substr( file.find( "." ) + 1 ) );
        return token;
    } );
}
void get_binary_from_source( const std::list<String> &files, JobsBuilder &jb, QueryMgr &qm ) {
    for ( auto &file : files ) {
        jb.add_job<std::list<String>>( [&]() {
            std::list<String> b;
            auto jc = qm.query( &get_token_list, file )->execute();
            // do sth with the data
            auto job = jc.jobs.front()->as<std::list<String>>();
            for ( auto &line : job.get() )
                b.push_back( line + "_token" );
            return b;
        } );
    }
}
void compile_binary( const std::list<String> &files, JobsBuilder &jb, QueryMgr &qm ) {
    jb.add_job<String>( [&]() {
        auto jc = qm.query( get_binary_from_source, files )->execute();
        String stream;
        for ( auto &job : jc.jobs ) {
            for ( auto &t : job->as<std::list<String>>().get() ) {
                stream += t + " ";
            }
        }
        return stream;
    } );
}

TEST_CASE( "Infrastructure", "[basic_workflow]" ) {
    auto qm = std::make_shared<QueryMgr>();
    qm->setup( 1 );
    auto jc = qm->query( compile_binary, std::list<String>{ "somefile.push", "another.push", "last.push" } )->execute();
    String result = jc.jobs.front()->as<String>().get();
    CHECK( result == "somefile_token ._token push_token another_token ._token push_token last_token ._token push_token " );
    qm->wait_finished();
}

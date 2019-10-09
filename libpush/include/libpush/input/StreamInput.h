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

#pragma once
#include "libpush/Base.h"
#include "libpush/input/SourceInput.h"

// Provides token input from any stream
class StreamInput : public SourceInput {
    sptr<std::basic_istream<u8>> stream;
    bool checked_bom = false; // already checked for BOM

    std::stack<std::pair<String, TokenLevel>> level_stack; // Level name -> level class
    size_t curr_line = 1;
    size_t curr_column = 1;

    String putback_buffer; // contains last chars which where not used
    std::queue<Token> back_buffer; // contains token which have only been previewed

    // Load the next token from the stream. Returns false if eof reached or failed
    bool load_next_token( String &buffer, size_t count = 1 );

    // implementation of the *_token() methods. \param whitespace is used internally
    Token get_token_impl( String whitespace = "" );

public:
    // Create a fileinput from a stream. \param file must be the name of the file from which the stream was open
    StreamInput( sptr<std::basic_istream<u8>> stream, sptr<String> file, sptr<Worker> w_ctx );
    virtual ~StreamInput() {}

    Token get_token();

    Token preview_token();

    Token preview_next_token();

    std::list<String> get_lines( size_t line_begin, size_t line_end, Worker &w_ctx );
};
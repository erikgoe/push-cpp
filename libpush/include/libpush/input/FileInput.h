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

#pragma once
#include "libpush/Base.h"
#include "libpush/input/SourceInput.h"

// Provides token input from a file
// NOTE: would be better with one abstract BufferInput for FileInput, StringInput, etc. and just provide the
// fill_buffer() method
// NOTE: better use a struct of ptr, in_string, etc. One for regular and one for prev_*
// NOTE BUG: in_string and in_comment should be stacks with the beginning type of comment or struct (otherwise a comment
// like "int a;// comment */ not a comment" may be written).
// NOTE BUG: The ">>" in "S<A<B>>" will be recognized as one operator
class FileInput : public SourceInput {
    std::ifstream fstream;
    char *buff, *buff_end, *fill, *ptr, *prev_ptr; // buffer ptrs
    size_t max_read; // max amount of data to read in one buffer fill
    bool eof; // stream reached end of file
    bool checked_bom; // already checked for BOM

    Token::Type curr_tt = Token::Type::count, prev_curr_tt = curr_tt;

    u32 in_string, in_comment, prev_in_string, prev_in_comment;
    size_t curr_line, curr_column, prev_curr_line, prev_curr_column;

    // may fail ( e. g. if ptr - prev_ptr == 1 )
    bool fill_buffer();

    // implementation of the *_token() methods
    Token FileInput::get_token_impl( char *&ptr, u32 &in_string, u32 &in_comment, size_t &curr_line,
                                     size_t &curr_column, Token::Type &curr_tt );

public:
    FileInput( const String &file, size_t buffer_size, size_t max_read, std::shared_ptr<Worker> w_ctx );
    ~FileInput();

    std::shared_ptr<SourceInput> open_new_file( const String &file, std::shared_ptr<Worker> w_ctx );
    bool file_exists( const String &file );

    Token get_token();

    Token preview_token();

    Token preview_next_token();
    
    std::list<String> get_lines( size_t line_begin, size_t line_end, Worker &w_ctx );
};

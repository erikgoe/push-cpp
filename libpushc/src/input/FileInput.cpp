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

#include "libpushc/stdafx.h"
#include "libpushc/input/FileInput.h"

FileInput::FileInput( const String &file, size_t buffer_size, size_t max_read ) {
    filename = std::make_shared<String>( file );
    fstream.open( file, std::ios_base::binary );

    if ( buffer_size == 0 )
        LOG_ERR( "buffer_size cannot be zero!" );
    buff = new char[buffer_size];
    prev_ptr = ptr = ( buff_end = buff + buffer_size ) - 1;
    fill = buff;
    this->max_read = max_read;
    if ( max_read == 0 )
        LOG_ERR( "max_read cannot be zero!" );

    eof = fstream.bad() || fstream.eof();

    in_string = prev_in_string = false;
    in_comment = prev_in_comment = false;
    curr_line = prev_curr_line = 0;
    curr_column = prev_curr_column = 0;
}
FileInput::~FileInput() {
    delete[] buff;
}

bool FileInput::fill_buffer() {
    if ( fstream.bad() || fstream.eof() ) {
        eof = true;
        return false;
    }
    if ( !eof ) {
        if ( ptr - fill == 1 ) {
            LOG_ERR( "Cannot fill buffer!" );
            return false; // fill cannot move thus the buffer cannot change. May occur when prev_ptr - ptr == 1.
        }
        size_t read_bytes = 0;
        char *rev_max = ptr - revert_size;
        if ( rev_max < buff )
            rev_max += buff_end - buff;
        if ( fill < buff_end ) { // fill buffer to the end
            fstream.read( fill, std::min( static_cast<size_t>( std::min( buff_end, rev_max ) - fill ), max_read ) );
            read_bytes = static_cast<size_t>( fstream.gcount() );
            fill += read_bytes;
            if ( fill == buff_end )
                fill = buff;
        }
        if ( fill < rev_max ) { // fill buffer from the begin
            fstream.read( fill, std::max( static_cast<size_t>( rev_max - fill ), max_read - read_bytes ) );
            fill += fstream.gcount();
        }
        return true;
    } else
        return false;
}

std::shared_ptr<SourceInput> FileInput::open_new_file( const String &file ) {
    return std::make_shared<FileInput>( file, buff_end - buff, max_read );
}

bool FileInput::file_exists( const String &file ) {
    return fs::exists( file );
}

Token FileInput::get_token_impl( bool original, char *&ptr, u32 &in_string, u32 &in_comment, size_t &curr_line,
                                 size_t &curr_column, Token::Type &curr_tt ) {
    Token t;
    t.file = filename;
    String curr;
    String curr_ws;

    while ( true ) {
        ptr++;
        if ( ptr == buff_end )
            ptr = buff;

        if ( ptr == fill ) { // buffer empty
            if ( !fill_buffer() ) { // reached end of file or error
                if ( curr.empty() ) { // return end of file
                    Token t;
                    t.type = Token::Type::eof;
                    t.content = "";
                    t.line = curr_line;
                    t.column = curr_column;
                    t.length = 0;
                    t.leading_ws = false;
                    return t;
                } else {
                    ptr--; // move one back for next run
                    if ( ptr < buff )
                        ptr += buff_end - buff;
                }
            }
        }

        curr += *ptr;
        auto e_pair = ending_token( curr, in_string, in_comment, curr_tt );
        if ( eof )
            e_pair = std::make_pair( Token::Type::eof, 1 );
        if ( e_pair.first != curr_tt && !( curr_tt == Token::Type::number &&
                 e_pair.first == Token::Type::number_float ) ||
             eof ) {
            if ( curr_tt == Token::Type::ws && !eof ) { // store leading whitespace
                curr_ws = curr.substr( 0, curr.size() - 1 );
                curr = curr.substr( curr.size() - 1 );
            } else if ( e_pair.second != curr.size() ) {
                // revert last token
                ptr -= e_pair.second;
                if ( ptr < buff )
                    ptr += buff_end - buff;
                curr = curr.substr( 0, curr.size() - e_pair.second );

                t.type = e_pair.first;
                t.content = ( original ? curr_ws + curr : curr );
                t.line = curr_line;
                t.column = curr_column;
                t.length = curr.length(); // FIXME does not count right for UTF-8 files
                t.leading_ws = !curr_ws.empty();

                // comment rules
                if ( t.type == Token::Type::comment_begin ) {
                    in_comment = ( cfg.nested_comments ? in_comment + 1 : 1 );
                } else if ( t.type == Token::Type::comment_end ) {
                    in_comment = ( cfg.nested_comments ? in_comment - 1 : 0 );
                }
                // string rules
                else if ( t.type == Token::Type::string_begin ) {
                    in_string = ( cfg.nested_strings ? in_string + 1 : 1 );
                } else if ( t.type == Token::Type::string_end ) {
                    in_string = ( cfg.nested_strings ? in_string - 1 : 0 );
                }

                curr_column += curr.size(); // FIXME does not count right for UTF-8 files
                return t;
            }
        }

        // token not finished
        curr_tt = e_pair.first; // set the type if ending_token() used the whole string
        if ( *ptr == '\n' && *( ptr != buff ? ptr - 1 : buff_end - 1 ) != '\r' ||
             *ptr == '\r' ) { // advance one line. Possible because revert_size is at least 1
            curr_line++;
            curr_column = 0;
        }
    }
}

Token FileInput::get_token( bool original ) {
    // reset for next preview
    prev_ptr = ptr;
    prev_in_string = in_string;
    prev_in_comment = in_comment;
    prev_curr_line = curr_line;
    prev_curr_column = curr_column;
    prev_curr_tt = curr_tt;

    return get_token_impl( original, ptr, in_string, in_comment, curr_line, curr_column, curr_tt );
}

Token FileInput::preview_token( bool original ) {
    prev_ptr = ptr;
    prev_in_string = in_string;
    prev_in_comment = in_comment;
    prev_curr_line = curr_line;
    prev_curr_column = curr_column;
    prev_curr_tt = curr_tt;

    return get_token_impl( original, prev_ptr, prev_in_string, prev_in_comment, prev_curr_line, prev_curr_column,
                           prev_curr_tt );
}

Token FileInput::preview_next_token( bool original ) {
    return get_token_impl( original, prev_ptr, prev_in_string, prev_in_comment, prev_curr_line, prev_curr_column,
                           prev_curr_tt );
}

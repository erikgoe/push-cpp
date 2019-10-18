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

#include "libpush/stdafx.h"
#include "libpush/input/StreamInput.h"
#include "libpush/Worker.h"
#include "libpush/GlobalCtx.h"

#include "libpush/Worker.inl"
#include "libpush/Message.inl"

StreamInput::StreamInput( sptr<std::basic_ifstream<char>> stream, sptr<String> file, sptr<Worker> w_ctx )
        : SourceInput( w_ctx, file ) {
    this->stream = stream;
    level_stack.push( std::make_pair( "", TokenLevel::normal ) );
}

bool StreamInput::load_next_token( String &buffer, size_t count ) {
    // First load buffered data
    size_t from_buffer = std::min( count, putback_buffer.size() );
    if ( from_buffer > 0 ) {
        buffer += putback_buffer.substr( 0, from_buffer );
        putback_buffer = putback_buffer.substr( from_buffer );
        count -= from_buffer;
    }

    // Load from stream
    if ( count > 0 ) {
        if ( stream->bad() || stream->eof() ) {
            return false;
        }

        // Fill the buffer with the remaining bytes and resize to fit
        buffer.resize( buffer.size() + count );
        stream->read( &buffer[buffer.size() - count], count );
        buffer.resize( buffer.size() - count + stream->gcount() );
        if ( stream->bad() || stream->eof() )
            return false;
    }
    return true;
}

// Counts how many newlines a string contains
size_t count_newlines( String &str ) {
    size_t ctr = 0;
    for ( size_t i = 0; i < str.size(); i++ ) {
        if ( ( str[i] == '\n' && ( i <= 0 || str[i - 1] != '\r' ) ) || str[i] == '\r' )
            ctr++;
    }
    return ctr;
}

Token StreamInput::get_token_impl( String whitespace ) {
    Token t;
    t.file = filename;
    String curr;
    bool is_special_ws = false; // like a comment end after "//"

    // Check for UTF-8 BOM first
    if ( !checked_bom ) {
        if ( !load_next_token( curr, 3 ) || curr[0] != (char) 0xEF || curr[1] != (char) 0xBB ||
             curr[2] != (char) 0xBF ) { // revert chars
            putback_buffer += curr;
        }
        curr.clear();
        checked_bom = true;
    }

    // ----------
    // Part A: Test for not-sticky tokens
    // ----------

    // Load next char
    load_next_token( curr, max_op_size );

    // Error handling
    if ( curr.empty() ) {
        // could not load chars
        t.type = Token::Type::eof;
        t.content = "";
        t.line = curr_line;
        t.column = curr_column;
        t.length = 0;
        t.leading_ws = whitespace;
        return t;
    }

    // Find token
    size_t slice_length = curr.size();
    // Decrease if needed until a matching token (or none at all) was found
    while ( ( t.type = find_non_sticky_token( curr.slice( 0, slice_length ), level_stack.top().second ) ) ==
            Token::Type::count ) {
        slice_length--;
        if ( slice_length == 0 )
            break;
    }
    if ( slice_length > 0 ) {
        // found token
        putback_buffer.insert( 0, curr.substr( slice_length ) );
        curr.resize( slice_length );

        // Check if is special whitespace token
        auto tmp_ending_type = find_last_sticky_token( curr.slice( 0 ), level_stack.top().second ).first;
        if ( tmp_ending_type == Token::Type::ws && t.type != Token::Type::ws ) {
            if ( !next_ws_is_not_special ) {
                is_special_ws = true;
                putback_buffer.insert( 0, curr );
                next_ws_is_not_special = true;
            } else { // ignore that it is special, because it has already be registered as it
                t.type = Token::Type::ws;
                next_ws_is_not_special = false;
            }
        }
    } else {
        // ----------
        // Part B: Test for sticky tokens
        // ----------

        // Unload loaded data
        putback_buffer.insert( 0, curr );
        curr.clear();

        std::pair<Token::Type, size_t> ending;
        bool eof_reached = false;
        do {
            // Load new char
            if ( !load_next_token( curr ) ) {
                // Stream ended with no new token
                // so the current token has to be finished
                eof_reached = true;
                break;
            }

            // Find last (longest) token of the string
            ending = find_last_sticky_token( curr.slice( 0 ), level_stack.top().second );
        } while ( ending.second == curr.size() );

        // Found the end of the token
        // curr.size() >= 2 because single chars will always match a token (if not eof)
        if ( !eof_reached ) {
            putback_buffer.insert( putback_buffer.begin(), curr.back() );
            curr.resize( curr.size() - 1 );
        }

        t.type = find_last_sticky_token( curr.slice( 0 ), level_stack.top().second ).first;
    }

    // Finish token
    t.content = curr;
    t.line = curr_line;
    t.column = curr_column;
    t.length = curr.length_cp();
    t.leading_ws = whitespace;
    t.tl = level_stack.top().second;

    // Count lines and columns
    if( !is_special_ws ) {
        curr_line += count_newlines( curr );
        size_t last_newline_idx = curr.find_last_of( '\n' );
        if ( last_newline_idx == String::npos )
            last_newline_idx = curr.find_last_of( '\r' );
        if( last_newline_idx == String::npos ) {
            curr_column += curr.length_grapheme();
        } else { // found a newline
            curr_column = curr.slice( last_newline_idx ).length_grapheme() + 1;
        }
    }

    // Update token level
    bool changed_level = false;
    auto *pairs = &cfg.normal;
    if ( level_stack.top().second == TokenLevel::comment ) {
        pairs = &cfg.comment;
    } else if ( level_stack.top().second == TokenLevel::string ) {
        pairs = &cfg.string;
    }
    for ( auto &c : *pairs ) {
        if ( c.second.first == level_stack.top().first && c.second.second == curr ) {
            // Found a pair which would match and end the token level
            level_stack.pop();
            changed_level = true;
            break;
        }
    }
    std::vector<String> &alo = cfg.allowed_level_overlay[level_stack.top().first];
    if ( !changed_level ) {
        // Might be a new normal level
        for ( auto &c : cfg.normal ) {
            if ( c.second.first == curr && std::find( alo.begin(), alo.end(), c.first ) != alo.end() ) {
                // Found new level
                level_stack.push( std::make_pair( c.second.first, TokenLevel::normal ) );
                changed_level = true;
                break;
            }
        }
    }
    if ( !changed_level ) {
        // Might be a new comment level
        for ( auto &c : cfg.comment ) {
            if ( c.second.first == curr && std::find( alo.begin(), alo.end(), c.first ) != alo.end() ) {
                // Found new level
                level_stack.push( std::make_pair( c.second.first, TokenLevel::comment ) );
                changed_level = true;
                break;
            }
        }
    }
    if ( !changed_level ) {
        // Might be a new string level
        for ( auto &c : cfg.string ) {
            if ( c.second.first == curr && std::find( alo.begin(), alo.end(), c.first ) != alo.end() ) {
                // Found new level
                level_stack.push( std::make_pair( c.second.first, TokenLevel::string ) );
                changed_level = true;
                break;
            }
        }
    }

    // Return the token
    if ( t.type == Token::Type::ws ) {
        // the token is just whitespace, so return the next token as the actual token (with updated whitespace)
        // Whitespace may be split into multiple tokens e. g. with comment ending newline
        return get_token_impl( whitespace + curr.replace_all( "\r\n", "\n" ).replace_all( "\r", "\n" ) );
    } else {
        return t;
    }
}

Token StreamInput::get_token() {
    if ( back_buffer.empty() ) {
        return get_token_impl();
    } else {
        auto tmp = back_buffer.front();
        back_buffer.pop();
        return tmp;
    }
}

Token StreamInput::preview_token() {
    if ( back_buffer.empty() )
        back_buffer.push( get_token_impl() );
    return back_buffer.front();
}

Token StreamInput::preview_next_token() {
    back_buffer.push( get_token_impl() );
    return back_buffer.back();
}

std::list<String> StreamInput::get_lines( size_t line_begin, size_t line_end, Worker &w_ctx ) {
    size_t line_count = 1;
    std::list<String> lines;
    String curr_line;
    char c = 0, last_c;

    while ( true ) {
        last_c = c;
        stream->get( c );
        if ( stream->bad() || stream->eof() ) {
            w_ctx.print_msg<MessageType::err_unexpected_eof_at_line_query>( MessageInfo(), {}, filename, line_count,
                                                                            line_begin, line_end );
            break;
        }

        if ( line_count >= line_begin && c != '\r' && c != '\n' )
            curr_line += c;

        if ( ( c == '\n' && last_c != '\r' ) || c == '\r' ) {
            // found a newline
            line_count++;
            if ( line_count > line_begin )
                lines.push_back( curr_line );
            curr_line.clear();

            if ( line_count > line_end )
                break;
        }
    }
    return lines;
}

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
#include "libpushc/Base.h"

struct Token {
    enum Type {
        stat_divider, // statement divider ";"
        block_begin, // begin of a block "{"
        block_end, // end of a block "}"

        comment_begin,
        comment_end,

        number, // integer type
        number_float, // any floating type
        encoded_char, // encoded char like "\x26"
        string_begin, // begin of a string "\""
        string_begin, // end of a string "\""
        op, // operator (multiple operators are bound together)
        keyword, // like operator but a single identifier
        identifier, // regular identifier that does not match any other category

        eof,

        count // not a token
    } type;

    String content;
    std::shared_ptr<String> file; // The actual access goes through the SourceInput
    size_t line = 0;
    size_t column = 0;
    size_t length = 0;
    bool leading_ws = false; // whether a whitspace char is in front of this token
};

// Very basic set of rules to define how strings are divided into token lists
struct TokenConfig {
    std::vector<String> stat_divider;
    std::vector<std::pair<String, String>> block; // begin -> end pair
    std::vector<std::pair<String, String>> comment; // begin -> end pair
    bool nested_comments;
    std::pair<u32, u32> allowed_chars; // start char -> end char pair
    bool nested_strings;
    std::vector<std::pair<String, String>> char_escapes; // from -> to pairing
    std::vector<String> char_encodings; // encoding prefixes
    std::vector<std::pair<String, String>> string; // character or string begin and end pair
    std::vector<String> integer_prefix; // allowed prefix tokens for a integer
    std::vector<String> integer_delimiter; // allowed tokens inside a integer
    std::vector<String> float_prefix; // allowed prefix tokens for a float
    std::vector<String> float_delimiter; // allowed tokens inside a float

    // Returns a predefined configuration for prelude files
    static TokenConfig get_prelude_cfg() {
        TokenConfig cfg;
        cfg.stat_divider.push_back( ";" );
        cfg.block.push_back( std::make_pair( "{", "}" ) );
        cfg.comment.push_back( std::make_pair( "/*", "*/" ) );
        cfg.comment.push_back( std::make_pair( "//", "\n" ) );
        cfg.nested_comments = true;
        cfg.allowed_chars = std::make_pair<u32, u32>( 0, 0xffffffff );
        cfg.nested_strings = false;
        cfg.char_escapes.push_back( std::make_pair( "\\n", "\n" ) );
        cfg.char_escapes.push_back( std::make_pair( "\\t", "\t" ) );
        cfg.char_escapes.push_back( std::make_pair( "\\v", "\v" ) );
        cfg.char_escapes.push_back( std::make_pair( "\\r", "\r" ) );
        cfg.char_escapes.push_back( std::make_pair( "\\\\", "\\" ) );
        cfg.char_escapes.push_back( std::make_pair( "\\\'", "\'" ) );
        cfg.char_escapes.push_back( std::make_pair( "\\\"", "\"" ) );
        cfg.char_escapes.push_back( std::make_pair( "\\0", "\0" ) );
        cfg.char_encodings.push_back( "\\o" );
        cfg.char_encodings.push_back( "\\x" );
        cfg.char_encodings.push_back( "\\u" );
        cfg.comment.push_back( std::make_pair( "\"", "\"" ) );
        cfg.integer_prefix.push_back( "0o" );
        cfg.integer_prefix.push_back( "0b" );
        cfg.integer_prefix.push_back( "0h" );
        cfg.float_delimiter.push_back( "." );
        return cfg;
    }
};

// Base class to get a token list
class SourceInput {
protected:
    TokenConfig cfg;

public:
    virtual ~SourceInput() {}

    // set the TokenConfig configuration
    void configure( const TokenConfig &cfg ) { this->cfg = cfg; }

    // Opens a new source input for the given file
    virtual std::shared_ptr<SourceInput> open_new_file( const String &file ) = 0;

    // Get the next token from the stream
    virtual Token get_token() = 0;

    // Get the next token, but don't move the stream head forward
    virtual Token preview_next_token() = 0;
};

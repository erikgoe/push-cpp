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
#include "libpushc/util/String.h"

struct Token {
    enum class Type {
        stat_divider, // statement divider ";"
        block_begin, // begin of a block "{"
        block_end, // end of a block "}"
        term_begin, // begin of a term "("
        term_end, // end of a term ")"

        comment_begin,
        comment_end,

        number, // integer type
        number_float, // any floating type
        encoded_char, // encoded char like "\x26"
        string_begin, // begin of a string "\""
        string_end, // end of a string "\""
        op, // operator (multiple operators are bound together)
        keyword, // like operator but a single identifier
        identifier, // regular identifier that does not match any other category

        eof,

        ws, // is not returned by the *_token() functions

        count // not a token
    } type;

    String content;
    std::shared_ptr<String> file; // The actual access goes through the SourceInput
    size_t line = 0;
    size_t column = 0;
    size_t length = 0;
    bool leading_ws = false; // whether a whitspace char is in front of this token (also with \param original set)
};

// Very basic set of rules to define how strings are divided into token lists
struct TokenConfig {
    std::vector<String> stat_divider;
    std::vector<std::pair<String, String>> block; // begin -> end pair
    std::vector<std::pair<String, String>> term; // begin -> end pair
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
    std::vector<String> operators; // all available operators.
                                   // Should be sorted with longest & most likely operators first
    std::vector<String> keywords; // all available operators

    // Returns a predefined configuration for prelude files
    static TokenConfig TokenConfig::get_prelude_cfg();
};

// Base class to get a token list
class SourceInput {
protected:
    TokenConfig cfg;
    size_t revert_size; // max size of a operator, etc.

    // returns the type and (bounded) size of the last characters of a string
    std::pair<Token::Type, size_t> ending_token( const String &str, bool in_string, bool in_comment,
                                                 const Token::Type curr_tt );

public:
    virtual ~SourceInput() {}

    // set the TokenConfig configuration
    virtual void configure( const TokenConfig &cfg );

    // Opens a new source input for the given file
    virtual std::shared_ptr<SourceInput> open_new_file( const String &file ) = 0;
    // Check whether a file exists in the source system.
    virtual bool file_exists( const String &file ) = 0;

    // Get the next token from the stream. \param original: if true, don't trim leading whitespace, etc.
    virtual Token get_token( bool original = false ) = 0;

    // Get the next token, but don't move the stream head forward. \param original: if true, don't trim leading
    // whitespace, etc.
    virtual Token preview_token( bool original = false ) = 0;

    // like preview_token but gives the next after a earlier preview
    virtual Token preview_next_token( bool original = false ) = 0;
};

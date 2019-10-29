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
#include "libpush/util/String.h"

// Contains information about the position in the file
struct PosInfo {
    sptr<String> file; // The actual access goes through the SourceInput
    size_t line = 0;
    size_t column = 0;
    size_t length = 0;
};

// Where in the code a token is
enum class TokenLevel {
    normal, // in no special area
    comment, // in any comment
    comment_line, // in a comment which ends at the next newline
    string, // in any string or character

    count
};

// Groups types of chars together. Sorted after priority descending
enum class CharRangeType {
    identifier, // forced identifier
    integer,
    ws,
    op,
    opt_identifier, // allowed in identifiers

    count
};

// Result of the lexing process
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
        encoded_char, // encoded char like "\x26"
        escaped_char, // escaped char like "\n"
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
    sptr<String> file; // The actual access goes through the SourceInput
    size_t line = 0;
    size_t column = 0;
    size_t length = 0;
    String leading_ws; // contains the whitspace in front of this token
    TokenLevel tl;

    Token() {}
    Token( Type type, const String &content, sptr<String> file, size_t line, size_t column, size_t length,
           const String &leading_ws, TokenLevel tl ) {
        this->type = type;
        this->content = content;
        this->file = file;
        this->line = line;
        this->column = column;
        this->length = length;
        this->leading_ws = leading_ws;
        this->tl = tl;
    }

    bool operator==( const Token &other ) const {
        return type == other.type && content == other.content &&
               ( file == other.file || ( file && other.file && *file == *other.file ) ) && line == other.line &&
               column == other.column && length == other.length && leading_ws == other.leading_ws && tl == other.tl;
    }

    // Returns a human readable representation of a token type
    static const String get_name( Type type ) {
        return type == Type::stat_divider
                   ? "end of expression"
                   : type == Type::block_begin
                         ? "begin of block"
                         : type == Type::block_end
                               ? "end of block"
                               : type == Type::term_begin || type == Type::term_end
                                     ? "parenthesis"
                                     : type == Type::comment_begin
                                           ? "begin of comment"
                                           : type == Type::comment_end
                                                 ? "end of comment"
                                                 : type == Type::number
                                                       ? "number literal"
                                                       : type == Type::encoded_char
                                                             ? "encoded character literal"
                                                             : type == Type::string_begin
                                                                   ? "begin of string"
                                                                   : type == Type::string_end
                                                                         ? "end of string"
                                                                         : type == Type::op
                                                                               ? "operator"
                                                                               : type == Type::keyword
                                                                                     ? "keyword"
                                                                                     : type == Type::identifier
                                                                                           ? "identifier"
                                                                                           : type == Type::eof
                                                                                                 ? "end of file"
                                                                                                 : type == Type::ws
                                                                                                       ? "whitespace"
                                                                                                       : "token";
    }
};

// Very basic set of rules to define how strings are divided into token lists
struct TokenConfig {
    std::vector<String> stat_divider;
    std::vector<std::pair<String, String>> block; // begin => end pair
    std::vector<std::pair<String, String>> term; // begin => end pair

    // Helper struct for level dependent begin-& end-pair
    struct LevelToken {
        String begin_token, end_token;
    };
    // Level dependent mappings (name of type => begin-end pair)
    std::map<TokenLevel, std::map<String, LevelToken>> level_map; // begin => end pair
    std::map<String, std::vector<String>> allowed_level_overlay; // outer begin => inner type. E. g. nested comments
    
    std::map<String, String> char_escapes; // map escaped chars to their representing value
    //std::map<String, String> char_encodings; // encode chars like '\x42' TODO delete

    std::map<CharRangeType, std::vector<std::pair<u32, u32>>> char_ranges; // ranges of characters (e. g. integers)

    std::vector<String> operators; // all available operators.
                                   // Should be sorted with longest & most likely operators first
    std::vector<String> keywords; // all available operators

    // Returns a predefined configuration for prelude files
    static TokenConfig get_prelude_cfg();
};

// Base class to get a token list
class SourceInput {
protected:
    TokenConfig cfg;
    std::map<TokenLevel, std::unordered_map<String, Token::Type>>
        not_sticky_map; // maps not sticky tokens (for each token level)
    std::map<CharRangeType, std::unordered_set<u32>> ranges_sets; // maps to elements in char ranges
    sptr<Worker> w_ctx;
    sptr<String> filename;
    size_t max_op_size; // max size of a operator (any not-sticky token)

    // Adds all chars of a string to a specific char range
    void insert_in_range( const String &str, CharRangeType range );

    // Checks which token matches the whole string. Returns Token::Type::count if none was found
    Token::Type find_non_sticky_token( const StringSlice &str, TokenLevel tl );

    // Checks which token matches the longest part at the enf of the string and its size
    std::pair<Token::Type, size_t> find_last_sticky_token( const StringSlice &str, TokenLevel tl );

public:
    SourceInput( sptr<Worker> w_ctx, sptr<String> file ) {
        this->w_ctx = w_ctx;
        this->filename = file;
    }
    virtual ~SourceInput() {}

    // set the TokenConfig configuration
    virtual void configure( const TokenConfig &cfg );

    // Opens a new source input for the given file
    virtual sptr<SourceInput> open_new_file( sptr<String> file, sptr<Worker> w_ctx ) = 0;

    // Returns the used filepath
    virtual sptr<String> get_filename() { return filename; }

    // Get the next token from the stream.
    virtual Token get_token() = 0;

    // Get the next token, but don't move the stream head forward.
    virtual Token preview_token() = 0;

    // like preview_token but gives the next after a earlier preview
    virtual Token preview_next_token() = 0;

    // Returns a list of source lines from the range line_begin..=line_end
    virtual std::list<String> get_lines( size_t line_begin, size_t line_end, Worker &w_ctx ) = 0;
};

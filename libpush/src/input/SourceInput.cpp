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
#include "libpush/input/SourceInput.h"
#include "libpush/Worker.h"
#include "libpush/GlobalCtx.h"

#include "libpush/Worker.inl"
#include "libpush/Message.inl"

TokenConfig TokenConfig::get_prelude_cfg() {
    TokenConfig cfg;
    cfg.stat_divider.push_back( ";" );
    cfg.block.push_back( std::make_pair( "{", "}" ) );
    cfg.term.push_back( std::make_pair( "(", ")" ) );
    cfg.level_map[TokenLevel::comment]["b"] = { "/*", "*/" };
    cfg.level_map[TokenLevel::comment_line]["ln"] = { "//", "\n" };
    cfg.level_map[TokenLevel::comment_line]["lr"] = { "//", "\r" };
    cfg.level_map[TokenLevel::string]["s"] = { "\"", "\"" };
    cfg.char_escapes["\\n"] = "\n";
    cfg.char_escapes["\\t"] = "\t";
    cfg.char_escapes["\\v"] = "\v";
    cfg.char_escapes["\\r"] = "\r";
    cfg.char_escapes["\\\\"] = "\\";
    cfg.char_escapes["\\\'"] = "\'";
    cfg.char_escapes["\\\""] = "\"";
    cfg.char_escapes["\\0"] = "\0";
    cfg.allowed_level_overlay[""].push_back( "s" );
    cfg.allowed_level_overlay[""].push_back( "b" );
    cfg.allowed_level_overlay[""].push_back( "ln" );
    cfg.allowed_level_overlay[""].push_back( "lr" );
    cfg.allowed_level_overlay["/*"].push_back( "b" );
    cfg.char_ranges[CharRangeType::opt_identifier].push_back( std::make_pair( '0', '9' ) );
    cfg.char_ranges[CharRangeType::integer].push_back( std::make_pair( '0', '9' ) );
    cfg.char_ranges[CharRangeType::ws].push_back( std::make_pair( ' ', ' ' ) );
    cfg.char_ranges[CharRangeType::ws].push_back( std::make_pair( '\n', '\n' ) );
    cfg.char_ranges[CharRangeType::ws].push_back( std::make_pair( '\r', '\r' ) );
    cfg.char_ranges[CharRangeType::ws].push_back( std::make_pair( '\t', '\t' ) );
    cfg.operators.push_back( "," );
    cfg.operators.push_back( "->" );
    cfg.operators.push_back( "#" );
    return cfg;
}

Token::Type SourceInput::find_non_sticky_token( const StringSlice &str, TokenLevel tl ) {
    auto tt = not_sticky_map[tl].find( str );
    if ( tt != not_sticky_map[tl].end() ) {
        return tt->second;
    } else {
        auto tt = cfg.char_escapes.find( str );
        return tt == cfg.char_escapes.end() ? Token::Type::count : Token::Type::escaped_char;
    }
}

std::pair<Token::Type, size_t> SourceInput::find_last_sticky_token( const StringSlice &str, TokenLevel tl ) {
    if ( str.empty() )
        return std::make_pair( Token::Type::count, 0 );

    CharRangeType expected;
    size_t offset = 0;
    for ( ; offset < str.size(); offset++ ) {
        // Find the type of the first char
        expected = CharRangeType::identifier;
        for ( auto &r : ranges_sets ) {
            if ( r.second.find( str[offset] ) != r.second.end() ) {
                expected = r.first;
                break;
            }
        }

        if ( expected == CharRangeType::op ) {
            // Operators are not allowed as sticky tokens
            if ( offset == str.size() - 1 )
                break; // allow a single char as sticky token
            continue;
        }

        // Check if all following chars match the expected type
        bool matches = true;
        for ( size_t i = offset + 1; i < str.size(); i++ ) {
            // A special case are identifiers which may also contain opt_identifier(s)
            if ( ranges_sets[expected].find( str[i] ) == ranges_sets[expected].end() &&
                 ( expected != CharRangeType::identifier || ranges_sets[CharRangeType::opt_identifier].find( str[i] ) ==
                                                                ranges_sets[CharRangeType::opt_identifier].end() ) ) {
                if ( expected == CharRangeType::identifier ) {
                    // An identifier may also be a char which is in no other range (because it's the default type)
                    for ( auto &r : ranges_sets ) {
                        if ( r.first != CharRangeType::identifier && r.first != CharRangeType::opt_identifier &&
                             r.second.find( str[i] ) != r.second.end() ) {
                            // Found in another range => type not matching
                            matches = false;
                            break;
                        }
                    }
                } else {
                    // Different type a the expected
                    matches = false;
                    break;
                }
            }
            if ( expected == CharRangeType::ws ) {
                // An identifier may also be a char which is in no other range (because it's the default type)
                for ( auto &t : not_sticky_map[tl] ) {
                    if ( t.second != Token::Type::ws && str.size() >= i + t.first.size() &&
                         str.slice( i, t.first.size() ) == t.first ) {
                        // found a token which is not a whitespace
                        matches = false;
                        break;
                    }
                }
            }
        }

        if ( matches )
            break;
    }

    // Check if matching is a keyword
    Token::Type tt;
    if ( expected == CharRangeType::identifier ) {
        if ( std::find( cfg.keywords.begin(), cfg.keywords.end(), String( str.slice( offset ) ) ) !=
             cfg.keywords.end() ) {
            tt = Token::Type::keyword;
        } else {
            tt = Token::Type::identifier;
        }
    } else if ( expected == CharRangeType::op ) {
        tt = Token::Type::op;
    } else if ( expected == CharRangeType::integer ) {
        tt = Token::Type::number;
    } else if ( expected == CharRangeType::ws ) {
        tt = Token::Type::ws;
    } else {
        tt = Token::Type::count;
    }
    return std::make_pair( tt, str.size() - offset );
}

void SourceInput::insert_in_range( const String &str, CharRangeType range ) {
    for ( auto &s : str ) {
        ranges_sets[range].insert( s );
    }
}

void SourceInput::configure( const TokenConfig &cfg ) {
    max_op_size = 1; // min 1, to review carriage return character
    this->cfg = cfg;

    // Helper lambda
    auto add_sticky_token_to_all = [this]( const String &token, Token::Type tt ) {
        not_sticky_map[TokenLevel::normal][token] = tt;
        not_sticky_map[TokenLevel::comment][token] = tt;
        not_sticky_map[TokenLevel::comment_line][token] = tt;
        not_sticky_map[TokenLevel::string][token] = tt;
    };

    // Initial ranges
    for ( auto &cr : cfg.char_ranges ) {
        for ( auto &subrange : cr.second ) {
            for ( u32 i = subrange.first; i <= subrange.second; i++ ) {
                ranges_sets[cr.first].insert( i );
            }
        }
    }

    // Operators (not-sticky tokens)
    for ( auto &tc : cfg.stat_divider ) {
        if ( tc.size() > max_op_size )
            max_op_size = tc.size();
        add_sticky_token_to_all( tc, Token::Type::stat_divider );
        insert_in_range( tc, CharRangeType::op );
    }
    for ( auto &tc : cfg.list_divider ) {
        if ( tc.size() > max_op_size )
            max_op_size = tc.size();
        add_sticky_token_to_all( tc, Token::Type::list_divider );
        insert_in_range( tc, CharRangeType::op );
    }
    for ( auto &tc : cfg.block ) {
        if ( tc.first.size() > max_op_size )
            max_op_size = tc.first.size();
        if ( tc.second.size() > max_op_size )
            max_op_size = tc.second.size();
        add_sticky_token_to_all( tc.first, Token::Type::block_begin );
        add_sticky_token_to_all( tc.second, Token::Type::block_end );
        insert_in_range( tc.first, CharRangeType::op );
        insert_in_range( tc.second, CharRangeType::op );
    }
    for ( auto &tc : cfg.term ) {
        if ( tc.first.size() > max_op_size )
            max_op_size = tc.first.size();
        if ( tc.second.size() > max_op_size )
            max_op_size = tc.second.size();
        add_sticky_token_to_all( tc.first, Token::Type::term_begin );
        add_sticky_token_to_all( tc.second, Token::Type::term_end );
        insert_in_range( tc.first, CharRangeType::op );
        insert_in_range( tc.second, CharRangeType::op );
    }
    for ( auto &tc : cfg.array ) {
        if ( tc.first.size() > max_op_size )
            max_op_size = tc.first.size();
        if ( tc.second.size() > max_op_size )
            max_op_size = tc.second.size();
        add_sticky_token_to_all( tc.first, Token::Type::array_begin );
        add_sticky_token_to_all( tc.second, Token::Type::array_end );
        insert_in_range( tc.first, CharRangeType::op );
        insert_in_range( tc.second, CharRangeType::op );
    }
    for ( auto &lm : cfg.level_map ) {
        for ( auto &tc : lm.second ) {
            if ( tc.second.begin_token.size() > max_op_size )
                max_op_size = tc.second.begin_token.size();
            if ( tc.second.end_token.size() > max_op_size )
                max_op_size = tc.second.end_token.size();

            Token::Type begin_tt =
                lm.first == TokenLevel::normal
                    ? Token::Type::op
                    : lm.first == TokenLevel::comment
                          ? Token::Type::comment_begin
                          : lm.first == TokenLevel::comment_line
                                ? Token::Type::comment_begin
                                : lm.first == TokenLevel::string ? Token::Type::string_begin : Token::Type::count;
            Token::Type end_tt =
                lm.first == TokenLevel::normal
                    ? Token::Type::op
                    : lm.first == TokenLevel::comment
                          ? Token::Type::comment_end
                          : lm.first == TokenLevel::comment_line
                                ? Token::Type::comment_end
                                : lm.first == TokenLevel::string ? Token::Type::string_end : Token::Type::count;

            add_sticky_token_to_all( tc.second.begin_token, begin_tt );
            not_sticky_map[lm.first][tc.second.end_token] = end_tt;

            insert_in_range( tc.second.begin_token, CharRangeType::op );
            insert_in_range( tc.second.end_token, CharRangeType::op );
        }
    }
    for ( auto &tc : cfg.operators ) {
        if ( tc.size() > max_op_size )
            max_op_size = tc.size();
        add_sticky_token_to_all( tc, Token::Type::op );
        insert_in_range( tc, CharRangeType::op );
    }
}

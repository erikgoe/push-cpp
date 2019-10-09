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
    cfg.comment["b"] = std::make_pair( "/*", "*/" );
    cfg.comment["ln"] = std::make_pair( "//", "\n" );
    cfg.comment["lr"] = std::make_pair( "//", "\r" );
    cfg.allowed_chars = std::make_pair<u32, u32>( 0, 0xffffffff );
    cfg.char_escapes.push_back( std::make_pair( "\\n", "\n" ) );
    cfg.char_escapes.push_back( std::make_pair( "\\t", "\t" ) );
    cfg.char_escapes.push_back( std::make_pair( "\\v", "\v" ) );
    cfg.char_escapes.push_back( std::make_pair( "\\r", "\r" ) );
    cfg.char_escapes.push_back( std::make_pair( "\\\\", "\\" ) );
    cfg.char_escapes.push_back( std::make_pair( "\\\'", "\'" ) );
    cfg.char_escapes.push_back( std::make_pair( "\\\"", "\"" ) );
    cfg.char_escapes.push_back( std::make_pair( "\\0", "\0" ) );
    cfg.string["s"] = std::make_pair( "\"", "\"" );
    cfg.allowed_level_overlay["n"].push_back( "s" );
    cfg.allowed_level_overlay["n"].push_back( "c" );
    cfg.allowed_level_overlay["b"].push_back( "b" );
    cfg.char_ranges[CharRangeType::opt_identifier].push_back( std::make_pair( '0', '9' ) );
    cfg.char_ranges[CharRangeType::integer].push_back( std::make_pair( '0', '9' ) );
    cfg.operators.push_back( "," );
    cfg.operators.push_back( "->" );
    cfg.operators.push_back( "#" );
    cfg.operators.push_back( "::" );
    return cfg;
}

Token::Type SourceInput::find_non_sticky_token( const StringSlice &str ) {
    auto tt = not_sticky_map.find( str );
    return tt == not_sticky_map.end() ? Token::Type::count : tt->second;
}

std::pair<Token::Type, size_t> SourceInput::find_last_sticky_token( const StringSlice &str ) {
    if ( str.empty() )
        return std::make_pair( Token::Type::count, 0 );

    CharRangeType expected;
    size_t offset = 0;
    for ( ; offset < str.size(); offset++ ) {
        // Find the type of the first char
        for ( auto &r : ranges_sets ) {
            if ( r.second.find( str[offset] ) != r.second.end() ) {
                expected = r.first;
                break;
            }
        }

        // Check if all following chars match the expected type
        bool matches = true;
        for ( size_t i = offset + 1; i < str.size(); i++ ) {
            // A special case are identifiers which may also contain opt_identifier(s)
            if ( ranges_sets[expected].find( str[i] ) == ranges_sets[expected].end() &&
                 ( expected != CharRangeType::identifier || ranges_sets[CharRangeType::opt_identifier].find( str[i] ) ==
                                                                ranges_sets[CharRangeType::opt_identifier].end() ) ) {
                // Different type
                matches = false;
                break;
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
    }
    return std::make_pair( tt, str.size() - offset );
}

void SourceInput::insert_in_range( const String &str, CharRangeType range ) {
    for ( auto &s : str )
        ranges_sets[range].insert( s );
}

void SourceInput::configure( const TokenConfig &cfg ) {
    max_op_size = 1; // min 1, to review carriage return character
    this->cfg = cfg;

    for ( auto &tc : cfg.stat_divider ) {
        if ( tc.size() > max_op_size )
            max_op_size = tc.size();
        not_sticky_map[tc] = Token::Type::stat_divider;
        insert_in_range( tc, CharRangeType::op );
    }
    for ( auto &tc : cfg.block ) {
        if ( tc.first.size() > max_op_size )
            max_op_size = tc.first.size();
        if ( tc.second.size() > max_op_size )
            max_op_size = tc.second.size();
        not_sticky_map[tc.first] = Token::Type::block_begin;
        not_sticky_map[tc.second] = Token::Type::block_end;
        insert_in_range( tc.first, CharRangeType::op );
        insert_in_range( tc.second, CharRangeType::op );
    }
    for ( auto &tc : cfg.term ) {
        if ( tc.first.size() > max_op_size )
            max_op_size = tc.first.size();
        if ( tc.second.size() > max_op_size )
            max_op_size = tc.second.size();
        not_sticky_map[tc.first] = Token::Type::term_begin;
        not_sticky_map[tc.second] = Token::Type::term_end;
        insert_in_range( tc.first, CharRangeType::op );
        insert_in_range( tc.second, CharRangeType::op );
    }
    for ( auto &tc : cfg.comment ) {
        if ( tc.second.first.size() > max_op_size )
            max_op_size = tc.second.first.size();
        if ( tc.second.second.size() > max_op_size )
            max_op_size = tc.second.second.size();
        not_sticky_map[tc.second.first] = Token::Type::comment_begin;
        not_sticky_map[tc.second.second] = Token::Type::comment_end;
        insert_in_range( tc.second.first, CharRangeType::op );
        insert_in_range( tc.second.second, CharRangeType::op );
    }
    for ( auto &tc : cfg.string ) {
        if ( tc.second.first.size() > max_op_size )
            max_op_size = tc.second.first.size();
        if ( tc.second.second.size() > max_op_size )
            max_op_size = tc.second.second.size();
        not_sticky_map[tc.second.first] = Token::Type::string_begin;
        not_sticky_map[tc.second.second] = Token::Type::string_end;
        insert_in_range( tc.second.first, CharRangeType::op );
        insert_in_range( tc.second.second, CharRangeType::op );
    }
    for ( auto &tc : cfg.operators ) {
        if ( tc.size() > max_op_size )
            max_op_size = tc.size();
        not_sticky_map[tc] = Token::Type::op;
        insert_in_range( tc, CharRangeType::op );
    }

    // Other ranges
    for ( auto &cr : cfg.char_ranges ) {
        for ( auto &subrange : cr.second ) {
            for ( u32 i = subrange.first; i <= subrange.second; i++ ) {
                ranges_sets[cr.first].insert( i );
            }
        }
    }

    // Predefined ranges
    ranges_sets[CharRangeType::ws].insert( { ' ', '\n', '\r', '\t' } );
}

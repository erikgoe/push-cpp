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
#include "libpushc/util/FmtStr.h"
#include "libpushc/input/SourceInput.h"

// Defines all types of messages
enum class MessageType;
enum class MessageClass {
    Notification,
    Warning,
    Error,
    FatalError,

    count
};
// Contains information about the sourcecode of a message
struct MessageInfo {
    std::shared_ptr<String> file;
    u32 line_begin = 0, line_end = 0;
    u32 column = 0, length = 0;
    u32 message_id = 0;
    FmtStr::Color color = FmtStr::Color::Blue;

    MessageInfo( std::shared_ptr<String> file, u32 line_begin, u32 line_end, u32 column, u32 length, u32 message_id,
                 FmtStr::Color color = FmtStr::Color::Blue ) {
        this->file = file;
        this->line_begin = line_begin;
        this->line_end = line_end;
        this->column = column;
        this->length = length;
        this->message_id = message_id;
        this->color = color;
    }
    bool operator<( const MessageInfo &other ) const {
        return ( file == other.file ? line_begin < other.line_begin : file < other.file );
    }
};

// Weird workaround for template specialization

template <MessageType MesT, typename... Args>
constexpr const FmtStr get_message_head( Args... args ) {
    return get_message_head_impl<MesT, Args...>::impl( args... );
}

template <MessageType MesT, typename... Args>
struct get_message_head_impl {
    constexpr static const FmtStr impl( Args... args ) {
        return "fatal error I" + to_string( 0xffff ) + ": no error definition.";
    }
};

template <MessageType MesT, typename... Args>
constexpr const std::vector<String> get_message_notes( Args... args ) {
    return get_message_notes_impl<MesT, Args...>::impl( args... );
}

template <MessageType MesT, typename... Args>
struct get_message_notes_impl {
    constexpr static const std::vector<String> impl( Args... args ) { return ""; }
};

// TODO add source module id
#define MESSAGE_DEFINITION( id, classid, source_symbol, msg, notes_list )                                        \
    template <typename... Args>                                                                                  \
    struct get_message_head_impl<id, Args...> {                                                                  \
        constexpr static const FmtStr impl( Args... args ) {                                                     \
            auto at = std::make_tuple( args... );                                                                \
            FmtStr::Color message_class_clr =                                                                    \
                ( classid == MessageClass::Notification                                                          \
                      ? FmtStr::Color::BoldBlue                                                                  \
                      : classid == MessageClass::Warning ? FmtStr::Color::BoldYellow : FmtStr::Color::BoldRed ); \
            String message_class_str =                                                                           \
                ( classid == MessageClass::Notification                                                          \
                      ? "notification"                                                                           \
                      : classid == MessageClass::Warning                                                         \
                            ? "warning"                                                                          \
                            : classid == MessageClass::Error                                                     \
                                  ? "error"                                                                      \
                                  : classid == MessageClass::FatalError ? "fatal error" : "unknown" );           \
            return FmtStr::Piece( message_class_str + " " + source_symbol + to_string( static_cast<u32>( id ) ), \
                                  message_class_clr ) +                                                          \
                   FmtStr::Piece( ": " + String( msg ) + "\n", FmtStr::Color::BoldBlack );                       \
        }                                                                                                        \
    };                                                                                                           \
    template <typename... Args>                                                                                  \
    struct get_message_notes_impl<id, Args...> {                                                                 \
        constexpr static const std::vector<String> impl( Args... args ) {                                        \
            auto at = std::make_tuple( args... );                                                                \
            return std::vector<String> notes_list;                                                               \
        }                                                                                                        \
    }

#define GET_ARG( index ) std::get<index>( at )

// Contains the actual definitions
#include "libpushc/Message.inl"

#undef GET_ARG
#undef MESSAGE_PARSE

// Replaces tabs with spaces
void ws_format_line( String &line ) {
    String tab_replace;
    for ( size_t i = 0; i < String::TAB_WIDTH; i++ ) // TODO replace with setting
        tab_replace += ' ';

    for ( size_t i = 0; i < line.size(); i++ ) {
        if ( line[i] == '\t' ) {
            line.replace( i, 1, tab_replace );
        }
    }
}


u8 get_color_hierarchy_value( const FmtStr::Color &color ) {
    static const std::map<FmtStr::Color, u8> color_h{
        { FmtStr::Color::Black, 0 },      { FmtStr::Color::Red, 8 },       { FmtStr::Color::Green, 3 },
        { FmtStr::Color::Blue, 2 },       { FmtStr::Color::Yellow, 5 },    { FmtStr::Color::BoldBlack, 1 },
        { FmtStr::Color::BoldRed, 9 },    { FmtStr::Color::BoldGreen, 6 }, { FmtStr::Color::BoldBlue, 4 },
        { FmtStr::Color::BoldYellow, 7 },
    };
    return color_h.at( color );
}

// Extract highlighted lines and combine them. \param hl_lines is sorted after the highlight precedence
void highlight_lines( std::list<std::tuple<size_t, size_t, size_t, FmtStr::Color>> &hl_lines, const MessageInfo &note,
                      std::shared_ptr<QueryMgr> qm ) {
    // Collect all highlightings
    std::list<std::tuple<size_t, size_t, size_t, FmtStr::Color>> tmp_hl_lines;
    String line; // TODO get line
    tmp_hl_lines.push_back(
        { note.line_begin, note.column, std::min<size_t>( note.length, line.length() - note.column ), note.color } );
    size_t remaining_chars = note.length - std::min<size_t>( note.length, line.length() - note.column );
    for ( size_t i = note.line_begin + 1; i <= note.line_end; i++ ) { // all following lines
        // TODO get line
        tmp_hl_lines.push_back( { i, 0, std::min<size_t>( remaining_chars, line.length() ), note.color } );
        remaining_chars -= std::min<size_t>( remaining_chars, line.length() );
    }

    // Combine the highlightings
    // Select the "strongest" color if they overlap
    auto insert_itr = std::find_if( hl_lines.begin(), hl_lines.end(), [&]( auto &e ) -> bool {
        return get_color_hierarchy_value( std::get<3>( e ) ) < get_color_hierarchy_value( note.color );
    } );
    hl_lines.insert( insert_itr, tmp_hl_lines.begin(), tmp_hl_lines.end() );
}

void draw_file( FmtStr &result, const String &file, const std::list<MessageInfo> &notes,
                const std::vector<String> &note_messages, size_t line_offset, std::shared_ptr<QueryMgr> qm ) {
    auto note_color = FmtStr::Color::Blue;
    auto regular_color = FmtStr::Color::Black;

    // Draw header
    result += FmtStr::Piece( "  --> ", note_color );
    String tmp = file;
    for ( auto &n : notes ) {
        tmp += ";" + to_string( n.line_begin ) + ( n.line_begin != n.line_end ? ".." + to_string( n.line_end ) : "" ) +
               ":" + to_string( n.column ) + ( n.column > 1 ? ".." + to_string( n.column + n.length ) : "" );
    }
    result += FmtStr::Piece( tmp + "\n", regular_color );

    // Draw source code body
    size_t last_lower_bound = 0, last_upper_bound = 0; // used to calculate line bounds
    for ( auto &n_itr = notes.begin(); n_itr != notes.end(); n_itr++ ) {
        // Contains all highlightings in the code. Structure: line, column, length, color
        std::list<std::tuple<size_t, size_t, size_t, FmtStr::Color>> hl_lines;

        // Calculate line bounds
        if ( n_itr->line_begin > last_upper_bound ) { // new bounded body
            // Leading line
            if ( n_itr == notes.begin() )
                result += FmtStr::Piece( String( line_offset, ' ' ) + " |", note_color );
            else
                result += FmtStr::Piece( "...", note_color );

            last_lower_bound = n_itr->line_begin;
            last_upper_bound = n_itr->line_end;
            highlight_lines( hl_lines, *n_itr, qm );
            auto tmp_itr = n_itr;
            while ( true ) {
                tmp_itr++;
                if ( tmp_itr != notes.end() && tmp_itr->line_begin <= last_upper_bound ) {
                    last_upper_bound = tmp_itr->line_end;
                    highlight_lines( hl_lines, *tmp_itr, qm );
                } else
                    break;
            }
        }

        // Print bounded text
        std::vector<size_t> line_lengths( last_upper_bound - last_lower_bound + 1 );
        for ( size_t i = last_lower_bound; i <= last_upper_bound; i++ ) {
            String num = to_string( i );
            result += FmtStr::Piece( String( line_offset - num.length(), ' ' ) + num + " |", note_color );

            String line; // TODO get sourcecode line
            ws_format_line( line );
            line_lengths[i - last_lower_bound] = line.length();

            // Divide into colored substrings
            String curr_piece = "";
            FmtStr::Color curr_color = regular_color;
            for ( size_t i = 0; i < line.length(); i++ ) {
                FmtStr::Color tmp_color = regular_color;
                for ( auto hll : hl_lines ) {
                    if ( i == std::get<0>( hll ) && i >= std::get<1>( hll ) &&
                         i < std::get<1>( hll ) + std::get<2>( hll ) ) {
                        tmp_color = std::get<3>( hll );
                    }
                }
                if ( curr_color == tmp_color ) {
                    curr_piece += line[i];
                } else if ( !curr_piece.empty() ) { // new substring
                    result += FmtStr::Piece( curr_piece, curr_color );
                    curr_piece.clear();
                    curr_piece += line[i];
                    curr_color = tmp_color;
                }
            }
            if ( !curr_piece.empty() ) // last substring
                result += FmtStr::Piece( curr_piece, curr_color );

            result += FmtStr::Piece( line + "\n", regular_color );
        }

        // Print message
        size_t remaining_chars = n_itr->length - 1;
        char underline_char =
            ( get_color_hierarchy_value( n_itr->color ) >= get_color_hierarchy_value( FmtStr::Color::Yellow ) ? '~'
                                                                                                              : '-' );
        for ( size_t i = last_lower_bound; i <= last_upper_bound; i++ ) {
            result += FmtStr::Piece( String( line_offset, ' ' ) + " |", note_color );
            if ( i < n_itr->line_begin ) { // line space
                result += FmtStr::Piece( "*\n", note_color );
            } else if ( i == n_itr->line_begin ) { // print first underlines
                if ( line_lengths[i - last_lower_bound] > n_itr->column ) { // something bad happened
                    result += FmtStr::Piece(
                        String( n_itr->column - 1, ' ' ) + '^' +
                            String( std::min<size_t>( remaining_chars,
                                                      line_lengths[i - last_lower_bound] - n_itr->column - 1 ),
                                    underline_char ) +
                            '\n',
                        n_itr->color );
                    remaining_chars -= std::min<size_t>( remaining_chars, line_lengths[i - last_lower_bound] );
                } else {
                    result += FmtStr::Piece( "\n", n_itr->color );
                    // TODO internel compiler error
                }
            } else if ( i <= n_itr->line_end ) { // print further underlines
                result += FmtStr::Piece(
                    String( std::min<size_t>( remaining_chars, line_lengths[i - last_lower_bound] - n_itr->column - 1 ),
                            underline_char ) +
                        '\n',
                    n_itr->color );
                remaining_chars -= std::min<size_t>( remaining_chars, line_lengths[i - last_lower_bound] );
            }
            if ( i == n_itr->line_end && note_messages.size() > n_itr->message_id ) { // print message
                result += FmtStr::Piece( " " + note_messages[n_itr->message_id], n_itr->color );
            }
        }
    }
}

template <MessageType MesT, size_t NotesSize, typename... Args>
constexpr const FmtStr get_message( std::shared_ptr<QueryMgr> qm, const MessageInfo &message,
                                    const std::array<MessageInfo, NotesSize> &notes, Args... head_args ) {
    FmtStr result = get_message_head<MesT>( head_args... );
    auto notes_list = get_message_notes<MesT>( head_args... );

    // Calculate some required formatting information
    size_t last_line = 0;
    std::map<String, std::list<MessageInfo>> notes_map;
    for ( auto &n : notes ) {
        last_line = std::max( last_line, n.line_end );
        notes_map[*n.file].push_back( n );
    }
    size_t line_offset = to_string( last_line ).size();

    // Print main message
    notes_map[*message.file].sort();
    notes_map[*message.file].push_front( message );
    draw_file( result, *message.file, notes_map[*message.file], notes_list, line_offset, qm );
    notes_map.erase( *message.file );
    // Print notes
    for ( auto &n : notes_map ) {
        n.second.sort();
        draw_file( result, n.first, n.second, notes_list, line_offset, qm );
    }
    return result;
}

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
#include "libpushc/Message.h"
#include "libpushc/GlobalCtx.h"
#include "libpushc/basic_queries/FileQueries.h"
#include "libpushc/UnitCtx.h"

// Replaces tabs with spaces
void ws_format_line( String &line ) {
    String tab_replace;
    for ( size_t i = 0; i < String::TAB_WIDTH; i++ )
        tab_replace += ' ';

    for ( size_t i = 0; i < line.size(); i++ ) {
        if ( line[i] == '\t' ) {
            line.replace( i, 1, tab_replace );
        }
    }
}


// Returns the precedence of a color. Higher value means it will overwrite other colors
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
                      std::vector<String> &lines, u32 lower_bound ) {
    // Collect all highlightings
    std::list<std::tuple<size_t, size_t, size_t, FmtStr::Color>> tmp_hl_lines;
    String line = ( lines.size() > note.line_begin - lower_bound ? lines[note.line_begin - lower_bound] : "" );
    tmp_hl_lines.push_back( { note.line_begin, note.column,
                              std::min<size_t>( note.length, line.length_grapheme() - note.column + 1 ), note.color } );
    size_t remaining_chars = note.length - std::min<size_t>( note.length, line.length_grapheme() - note.column + 1 );
    for ( size_t i = note.line_begin + 1; i <= note.line_end; i++ ) { // all following lines
        String line = ( lines.size() > i - lower_bound ? lines[i - lower_bound] : "" );
        tmp_hl_lines.push_back( { i, 1, std::min<size_t>( remaining_chars, line.length_grapheme() ), note.color } );
        remaining_chars -= std::min<size_t>( remaining_chars, line.length_grapheme() );
    }

    // Combine the highlightings
    // Select the "strongest" color if they overlap
    auto insert_itr = std::find_if( hl_lines.begin(), hl_lines.end(), [&]( auto &e ) -> bool {
        return get_color_hierarchy_value( std::get<3>( e ) ) > get_color_hierarchy_value( note.color );
    } );
    hl_lines.insert( insert_itr, tmp_hl_lines.begin(), tmp_hl_lines.end() );
}

void draw_file( FmtStr &result, const String &file, const std::list<MessageInfo> &notes,
                const std::vector<String> &note_messages, size_t line_offset, std::shared_ptr<Worker> w_ctx ) {
    auto note_color = FmtStr::Color::Blue;
    auto regular_color = FmtStr::Color::Black;

    // Preload lines
    std::vector<String> source_lines;
    size_t source_line_bound = 0;
    {
        source_line_bound = notes.front().line_begin;
        size_t upper_bound = notes.front().line_end;
        for ( auto n : notes ) {
            if ( n.line_begin < source_line_bound )
                source_line_bound = n.line_begin;
            if ( n.line_end > upper_bound )
                upper_bound = n.line_end;
        }

        auto list = w_ctx->get_query_mgr()
                        ->query( get_source_lines, nullptr, file, source_line_bound, upper_bound )
                        ->execute( *w_ctx )
                        ->jobs.front()
                        ->to<std::list<String>>();
        source_lines.reserve( list.size() );
        for ( auto &s : list ) {
            ws_format_line( s );
            source_lines.push_back( s );
        }
    }

    // Draw header
    result += FmtStr::Piece( "  --> ", note_color );
    result += FmtStr::Piece( file, regular_color );
    for ( auto &n : notes ) {
        result += FmtStr::Piece( ";", regular_color );
        result += FmtStr::Piece(
            to_string( n.line_begin ) + ( n.line_begin != n.line_end ? ".." + to_string( n.line_end ) : "" ) + ":" +
                to_string( n.column ) +
                ( n.length > 1 && n.line_begin == n.line_end ? ".." + to_string( n.column + n.length - 1 )
                                                             : n.length > 1 ? "+" + to_string( n.length ) : "" ),
            n.color );
    }
    result += FmtStr::Piece( "\n", regular_color );

    // Draw source code body
    size_t last_lower_bound = 0, last_upper_bound = 0; // used to calculate line bounds
    std::vector<size_t> line_lengths;
    for ( auto &n_itr = notes.begin(); n_itr != notes.end(); n_itr++ ) {
        // Contains all highlightings in the code. Structure: line, column, length, color
        std::list<std::tuple<size_t, size_t, size_t, FmtStr::Color>> hl_lines;

        if ( n_itr->line_begin > last_upper_bound ) { // new bounded body
            // Calculate line bounds

            // Leading line
            if ( n_itr == notes.begin() )
                result += FmtStr::Piece( String( line_offset, ' ' ) + " |\n", note_color );
            else
                result += FmtStr::Piece( "...\n", note_color );

            last_lower_bound = n_itr->line_begin;
            last_upper_bound = n_itr->line_end;
            highlight_lines( hl_lines, *n_itr, source_lines, source_line_bound );
            auto tmp_itr = n_itr;
            while ( true ) {
                tmp_itr++;
                if ( tmp_itr != notes.end() && tmp_itr->line_begin <= last_upper_bound ) { // increase bounds
                    last_upper_bound = tmp_itr->line_end;
                    highlight_lines( hl_lines, *tmp_itr, source_lines, source_line_bound );
                } else
                    break;
            }

            // Print bounded text
            line_lengths.clear();
            line_lengths.resize( last_upper_bound - last_lower_bound + 1 );
            for ( size_t i = last_lower_bound; i <= last_upper_bound; i++ ) {
                String num = to_string( i );
                result += FmtStr::Piece( String( line_offset - num.length(), ' ' ) + num + " |", note_color );

                String &line =
                    ( source_lines.size() > i - source_line_bound ? source_lines[i - source_line_bound] : "" );
                line_lengths[i - last_lower_bound] = line.length_grapheme();

                // Divide into colored substrings
                String curr_piece = "";
                FmtStr::Color curr_color = regular_color;
                for ( size_t j = 0; j < line.length(); j++ ) {
                    FmtStr::Color tmp_color = regular_color;
                    for ( auto hll : hl_lines ) {
                        if ( i == std::get<0>( hll ) && j + 1 >= std::get<1>( hll ) &&
                             j + 1 < std::get<1>( hll ) + std::get<2>( hll ) ) {
                            tmp_color = std::get<3>( hll );
                        }
                    }
                    if ( curr_color == tmp_color ) {
                        curr_piece += line[j];
                        while ( line.size() > j + 1 && ( line[j + 1] & 0xC0 ) == 0x80 ) {
                            j++;
                            curr_piece += line[j];
                        }
                    } else {
                        if ( !curr_piece.empty() ) { // new substring
                            result += FmtStr::Piece( curr_piece, curr_color );
                            curr_piece.clear();
                        }
                        curr_piece += line[j];
                        while ( line.size() > j + 1 && ( line[j + 1] & 0xC0 ) == 0x80 ) {
                            j++;
                            curr_piece += line[j];
                        }
                        curr_color = tmp_color;
                    }
                }
                if ( !curr_piece.empty() ) // last substring
                    result += FmtStr::Piece( curr_piece, curr_color );

                result += FmtStr::Piece( "\n", regular_color );
            }
        }

        // Print message
        size_t remaining_chars = n_itr->length - 1;
        char underline_char =
            ( get_color_hierarchy_value( n_itr->color ) >= get_color_hierarchy_value( FmtStr::Color::Yellow ) ? '~'
                                                                                                              : '-' );
        for ( size_t i = last_lower_bound; i <= last_upper_bound && i <= n_itr->line_end; i++ ) {
            result += FmtStr::Piece( String( line_offset, ' ' ) + " |", note_color );
            if ( i < n_itr->line_begin ) { // line space
                result += FmtStr::Piece( "*", n_itr->color );
            } else if ( i == n_itr->line_begin ) { // print first underlines
                if ( line_lengths[i - last_lower_bound] >= n_itr->column ) {
                    // TODO enable messages with arbitrary caret position and size (~~^~~~ or ^^^^~~ or ~^^~~)
                    result += FmtStr::Piece(
                        String( n_itr->column - 1, ' ' ) + '^' +
                            String(
                                std::min<size_t>( remaining_chars, line_lengths[i - last_lower_bound] - n_itr->column ),
                                underline_char ),
                        n_itr->color );
                    remaining_chars -=
                        std::min<size_t>( remaining_chars, line_lengths[i - last_lower_bound] - n_itr->column );
                } else { // something bad happened
                    LOG_ERR( "Line " + to_string( i ) + " is not long enough to reach column " +
                             to_string( n_itr->column ) + "." );
                }
            } else if ( i <= n_itr->line_end ) { // print further underlines
                result += FmtStr::Piece(
                    String( std::min<size_t>( remaining_chars, line_lengths[i - last_lower_bound] ), underline_char ),
                    n_itr->color );
                remaining_chars -= std::min<size_t>( remaining_chars, line_lengths[i - last_lower_bound] );
            }
            if ( i == n_itr->line_end && note_messages.size() > n_itr->message_idx ) { // print message
                result += FmtStr::Piece( " " + note_messages[n_itr->message_idx] + "\n", n_itr->color );
            } else {
                result += FmtStr::Piece( "\n", n_itr->color );
            }
        }
    }
}

void print_msg_to_stdout( FmtStr &str ) {
// Configure console first
#ifdef _WIN32
    HANDLE console_h = GetStdHandle( STD_OUTPUT_HANDLE );
    auto buff_info = CONSOLE_SCREEN_BUFFER_INFO();
    GetConsoleScreenBufferInfo( console_h, &buff_info );
    SetConsoleOutputCP( CP_UTF8 );
    int previous_color = buff_info.wAttributes;
#else
    int previous_color = 8;
#endif
    setvbuf( stdout, nullptr, _IOFBF, 1000 );

    int att_color = previous_color;
    String linux_prefix = "";

    while ( !str.empty() ) {
        auto piece = str.consume();

        if ( piece.color == FmtStr::Color::Black ) {
            att_color = previous_color;
            linux_prefix = "\033[0;37m";
        } else if ( piece.color == FmtStr::Color::Red ) {
            att_color = 4;
            linux_prefix = "\033[0;31m";
        } else if ( piece.color == FmtStr::Color::Green ) {
            att_color = 2;
            linux_prefix = "\033[0;32m";
        } else if ( piece.color == FmtStr::Color::Blue ) {
            att_color = 1;
            linux_prefix = "\033[0;34m";
        } else if ( piece.color == FmtStr::Color::Yellow ) {
            att_color = 6;
            linux_prefix = "\033[0;33m";
        } else if ( piece.color == FmtStr::Color::BoldBlack ) {
            att_color = 15;
            linux_prefix = "\033[1;37m";
        } else if ( piece.color == FmtStr::Color::BoldRed ) {
            att_color = 12;
            linux_prefix = "\033[1;31m";
        } else if ( piece.color == FmtStr::Color::BoldGreen ) {
            att_color = 10;
            linux_prefix = "\033[1;32m";
        } else if ( piece.color == FmtStr::Color::BoldBlue ) {
            att_color = 9;
            linux_prefix = "\033[1;34m";
        } else if ( piece.color == FmtStr::Color::BoldYellow ) {
            att_color = 14;
            linux_prefix = "\033[1;33m";
        }

#ifdef _WIN32
        SetConsoleTextAttribute( console_h, att_color );
#else
        std::cout << linux_prefix;
#endif

        std::cout << piece.text << std::flush;
    }

    // Reset console prefs
#ifdef _WIN32
    SetConsoleTextAttribute( console_h, previous_color );
#else
    std::cout << "\033[0m";
#endif
    setvbuf( stdout, nullptr, _IONBF, 0 );
}

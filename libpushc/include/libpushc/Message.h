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
// Existing classes of messages. If changed, edit the get_message_head_impl() macro
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

// Weird workaround for global function template specialization

// Returns the head of a message including the error message
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

// Returns a list of additional notes which can be applied to a message
template <MessageType MesT, typename... Args>
constexpr const std::vector<String> get_message_notes( Args... args ) {
    return get_message_notes_impl<MesT, Args...>::impl( args... );
}

template <MessageType MesT, typename... Args>
struct get_message_notes_impl {
    constexpr static const std::vector<String> impl( Args... args ) { return ""; }
};

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

// Returns the value of a given argument
#define GET_ARG( index ) std::get<index>( at )

// Contains the actual definitions
#include "libpushc/Message.inl"

#undef GET_ARG
#undef MESSAGE_PARSE

// Internally used by get_message() to print the messages for one file
void draw_file( FmtStr &result, const String &file, const std::list<MessageInfo> &notes,
                const std::vector<String> &note_messages, size_t line_offset, std::shared_ptr<Worker> w_ctx );

// Returns a formatted message which can be shown to the user
template <MessageType MesT, size_t NotesSize, typename... Args>
constexpr const FmtStr get_message( std::shared_ptr<Worker> w_ctx, const MessageInfo &message,
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
    notes_map[*message.file].push_front( message );
    notes_map[*message.file].sort();
    draw_file( result, *message.file, notes_map[*message.file], notes_list, line_offset, w_ctx );
    notes_map.erase( *message.file );

    // Print notes
    for ( auto &n : notes_map ) {
        n.second.sort();
        draw_file( result, n.first, n.second, notes_list, line_offset, w_ctx );
    }
    return result;
}

// Prints
void print_msg_to_stdout( FmtStr &str );

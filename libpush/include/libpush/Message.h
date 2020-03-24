// Copyright 2020 Erik Götzfried
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
#include "libpush/util/FmtStr.h"
#include "libpush/input/SourceInput.h"

class Expr;

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
    sptr<String> file;
    u32 line_begin = 0, line_end = 0;
    u32 column = 0, length = 0;
    u32 message_idx = 0;
    FmtStr::Color color = FmtStr::Color::Blue;

    MessageInfo() {}
    MessageInfo( sptr<String> file, u32 line_begin, u32 line_end, u32 column, u32 length, u32 message_idx,
                 FmtStr::Color color = FmtStr::Color::Blue ) {
        this->file = file;
        this->line_begin = line_begin;
        this->line_end = line_end;
        this->column = column;
        this->length = length;
        this->message_idx = message_idx;
        this->color = color;
    }
    MessageInfo( u32 message_idx, FmtStr::Color color = FmtStr::Color::Blue ) {
        this->message_idx = message_idx;
        this->color = color;
    }
    MessageInfo( const Token &t, u32 message_idx = 0, FmtStr::Color color = FmtStr::Color::Blue )
            : MessageInfo( t.file, t.line, t.line, t.column, t.length, message_idx, color ) {}
    // This constructor in defined in libpushc/src/Expression.cpp
    MessageInfo( const sptr<Expr> &expr, u32 message_idx = 0, FmtStr::Color color = FmtStr::Color::Blue );
    MessageInfo( const PosInfo &po, u32 message_idx = 0, FmtStr::Color color = FmtStr::Color::Blue )
            : MessageInfo( po.file, po.line, po.line, po.column, po.length, message_idx, color ) {}

    bool operator<( const MessageInfo &other ) const {
        return ( file == other.file ? line_begin < other.line_begin : file < other.file );
    }
};

// This is thrown if the compilation is aborted
class AbortCompilationError : public std::exception {
public:
    AbortCompilationError() : std::exception() {}
};

// Weird workaround for global function template specialization

template <MessageType MesT, typename... Args>
struct get_message_head_impl {
    static FmtStr impl( Args... args ) {
        /*static_assert( false,
                       "No message head information was found for this MessageType. Implement it in \"Message.inl\""
           );*/
        return {};
    }
};

// Returns the head of a message including the message text. Will not increment any message count. Use get_message()
// instead.
template <MessageType MesT, typename... Args>
FmtStr get_message_head( Args... args ) {
    return get_message_head_impl<MesT, Args...>::impl( args... );
}

template <MessageType MesT, typename... Args>
struct get_message_notes_impl {
    static std::vector<String> impl( Args... args ) {
        /*static_assert( false,
                       "No message note information was found for this MessageType. Implement it in \"Message.inl\""
           );*/
        return {};
    }
};

// Returns a list of additional notes which can be applied to a message. Will not increment any message count. Use
// get_message() instead.
template <MessageType MesT, typename... Args>
std::vector<String> get_message_notes( Args... args ) {
    return get_message_notes_impl<MesT, Args...>::impl( args... );
}

#define MESSAGE_DEFINITION( id, classid, source_symbol, msg, ... )                                               \
    template <typename... Args>                                                                                  \
    struct get_message_head_impl<id, Args...> {                                                                  \
        static FmtStr impl( Args... args ) {                                                                     \
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
        static std::vector<String> impl( Args... args ) {                                                        \
            auto at = std::make_tuple( args... );                                                                \
            return std::vector<String>{ __VA_ARGS__ };                                                           \
        }                                                                                                        \
    }

// Returns the value of a given argument
#define GET_ARG( index ) std::get<index>( at )

// Contains the actual definitions
#include "libpush/Messages.inl"

#undef GET_ARG
#undef MESSAGE_PARSE

// Internally used by get_message() to print the messages for one file
void draw_file( FmtStr &result, const String &file, const std::list<MessageInfo> &notes,
                const std::vector<String> &note_messages, size_t line_offset, sptr<Worker> w_ctx );

// Returns a formatted message which can be shown to the user
template <MessageType MesT, typename... Args>
FmtStr get_message( sptr<Worker> w_ctx, const MessageInfo &message, const std::vector<MessageInfo> &notes,
                    Args... head_args );

// Prints
void print_msg_to_stdout( FmtStr str );

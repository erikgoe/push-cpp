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

template <MessageType MesT, typename... Args>
FmtStr get_message( sptr<Worker> w_ctx, const MessageInfo &message,
                              const std::vector<MessageInfo> &notes, Args... head_args ) {
    FmtStr result = get_message_head<MesT>( head_args... );
    auto notes_list = get_message_notes<MesT>( head_args... );

    auto g_ctx = w_ctx->global_ctx();

    if ( !g_ctx->jobs_allowed() )
        throw AbortCompilationError();

    // Calculate some required formatting information
    u32 last_line = 0;
    std::map<String, std::list<MessageInfo>> notes_map;
    std::list<MessageInfo> global_messages;
    for ( auto &n : notes ) {
        last_line = std::max( last_line, n.line_end );
        if ( n.file )
            notes_map[*n.file].push_back( n );
        else
            global_messages.push_back( n );
    }
    size_t line_offset = to_string( std::max( last_line, std::max( message.line_begin, message.line_begin ) ) ).size();

    // Print main message
    if ( message.file ) { // message has no main file
        notes_map[*message.file].push_front( message );
        notes_map[*message.file].sort();
        draw_file( result, *message.file, notes_map[*message.file], notes_list, line_offset, w_ctx );
        notes_map.erase( *message.file );
    }

    // Print notes
    for ( auto &n : notes_map ) {
        n.second.sort();
        draw_file( result, n.first, n.second, notes_list, line_offset, w_ctx );
    }

    // Global messages
    if ( !global_messages.empty() ) {
        result += FmtStr::Piece( "  Notes:\n", FmtStr::Color::Blue ); // using notes color
        for ( auto &m : global_messages ) {
            result += FmtStr::Piece( "   " + notes_list[m.message_idx] + "\n", m.color );
        }
    }

    if ( MesT < MessageType::error ) { // fatal error
        g_ctx->abort_compilation();
    } else if ( MesT < MessageType::warning ) { // error
        if ( g_ctx->error_count++ >= g_ctx->max_allowed_errors ) {
            w_ctx->print_msg<MessageType::ferr_abort_too_many_errors>( MessageInfo(), {}, g_ctx->error_count.load() );
        }
    } else if ( MesT < MessageType::notification ) { // warning
        if ( g_ctx->warning_count++ >= g_ctx->max_allowed_warnings ) {
            w_ctx->print_msg<MessageType::ferr_abort_too_many_warnings>( MessageInfo(), {},
                                                                         g_ctx->warning_count.load() );
        }
    } else if ( g_ctx->notification_count++ >= g_ctx->max_allowed_notifications ) { // notification
        w_ctx->print_msg<MessageType::ferr_abort_too_many_notifications>( MessageInfo(), {},
                                                                          g_ctx->notification_count.load() );
    }
    return result;
}

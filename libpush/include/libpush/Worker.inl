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
#include "libpush/GlobalCtx.inl"

template <typename FuncT, typename... Args>
auto Worker::query( FuncT fn, const Args &... args ) -> decltype( auto ) {
    return g_ctx->query( fn, shared_from_this(), args... );
}
template <typename FuncT, typename... Args>
auto Worker::do_query( FuncT fn, const Args &... args ) -> decltype( auto ) {
    return g_ctx->query( fn, shared_from_this(), args... )->execute( *this )->wait();
}

template <MessageType MesT, typename... Args>
constexpr void Worker::print_msg( const MessageInfo &message, const std::vector<MessageInfo> &notes, Args... head_args ) {
    g_ctx->print_msg<MesT>( shared_from_this(), message, notes, head_args... );
}

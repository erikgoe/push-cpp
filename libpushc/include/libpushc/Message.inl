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

// Actual error declaration
enum class MessageType {
    fatal_error = 0,
    ferr_abort_too_many_errors,
    ferr_abort_too_many_warnings,
    ferr_abort_too_many_notifications,

    error = 100,
    err_lexer_char_not_allowed,

    warning = 5000,

    notification = 10000,

    count
};

// Actual error definitions. Strucure: <MessageType>, <message class>, "<message source symbol>", "<error message>",
// {"notes", ...} The error message and the notes may contain additional data shipped by the GET_ARG(index) macro.

MESSAGE_DEFINITION( MessageType::ferr_abort_too_many_errors, MessageClass::Error, "X",
                    "Abort due to too many (" + to_string( GET_ARG( 0 ) ) + ") generated errors", {} );
MESSAGE_DEFINITION( MessageType::ferr_abort_too_many_warnings, MessageClass::Error, "X",
                    "Abort due to too many (" + to_string( GET_ARG( 0 ) ) + ") generated warnings", {} );
MESSAGE_DEFINITION( MessageType::ferr_abort_too_many_notifications, MessageClass::Error, "X",
                    "Abort due to too many (" + to_string( GET_ARG( 0 ) ) + ") generated notifications", {} );

MESSAGE_DEFINITION( MessageType::err_lexer_char_not_allowed, MessageClass::Error, "L",
                    "Character is not in allowed set of characters.", { "not allowed character" } );

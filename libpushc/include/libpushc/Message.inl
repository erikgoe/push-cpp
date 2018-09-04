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
    error_lexer_char_not_allowed,

    count
};

// Actual error definitions. Strucure: <MessageType>, <message class>, "<message source symbol>", "<error message>", {"notes", ...}
// The error message and the notes may contain additional data shipped by the GET_ARG(index) macro.
MESSAGE_DEFINITION( MessageType::error_lexer_char_not_allowed, MessageClass::Error, "L",
                    "Character is not in allowed set of characters.", { "not allowed character" } );

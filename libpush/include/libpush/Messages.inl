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

// Actual error declaration
enum class MessageType {
    fatal_error = 0,
    ferr_abort_too_many_errors,
    ferr_abort_too_many_warnings,
    ferr_abort_too_many_notifications,
    ferr_file_not_found,
    ferr_failed_prelude,

    error = 100,
    err_unknown_source_input_pref,
    err_unexpected_eof_at_line_query,
    err_unexpected_eof_at_string_parsing,
    err_lexer_char_not_allowed,
    err_not_allowed_token_in_prelude,
    err_parse_mci_rule,
    err_unknown_mci,
    err_feature_curr_not_supported,
    err_parse_number,

    err_unexpected_eof_after,
    err_malformed_prelude_command,
    err_expected_string,
    err_invalid_prelude,
    err_term_with_multiple_expr,
    err_semicolon_without_meaning,
    err_array_access_with_multiple_expr,

    warning = 5000,

    notification = 10000,

    count,
    test_message // used for testing
};

// Actual error definitions. Strucure: <MessageType>, <message class>, "<message source symbol>", "<error message>",
// ..."notes" The error message and the notes may contain additional data shipped by the GET_ARG(index) macro.

// Message source symbols: I(lexer/input), C(Compiler), L(Linker), X(may be everywhere)

MESSAGE_DEFINITION( MessageType::ferr_abort_too_many_errors, MessageClass::FatalError, "X",
                    "Abort due to too many (" + to_string( GET_ARG( 0 ) ) + ") generated errors." );
MESSAGE_DEFINITION( MessageType::ferr_abort_too_many_warnings, MessageClass::FatalError, "X",
                    "Abort due to too many (" + to_string( GET_ARG( 0 ) ) + ") generated warnings." );
MESSAGE_DEFINITION( MessageType::ferr_abort_too_many_notifications, MessageClass::FatalError, "X",
                    "Abort due to too many (" + to_string( GET_ARG( 0 ) ) + ") generated notifications." );
MESSAGE_DEFINITION( MessageType::ferr_file_not_found, MessageClass::FatalError, "I",
                    "File \"" + GET_ARG( 0 ) + "\" was not found." );
MESSAGE_DEFINITION( MessageType::ferr_failed_prelude, MessageClass::FatalError, "I",
                    "Failed to load prelude \"" + GET_ARG( 0 ) + "\"." );


MESSAGE_DEFINITION( MessageType::err_unknown_source_input_pref, MessageClass::Error, "I",
                    "Unknown source input type `" + GET_ARG( 0 ) + "` for file `" + GET_ARG( 1 ) + "`." );
MESSAGE_DEFINITION( MessageType::err_unexpected_eof_at_line_query, MessageClass::Error, "I",
                    "File `" + *GET_ARG( 0 ) + "` unexpectedly ended at line `" + to_string( GET_ARG( 1 ) ) +
                        "` while attempting to read range \"" + to_string( GET_ARG( 2 ) ) + ".." +
                        to_string( GET_ARG( 3 ) ) + "\"." );
MESSAGE_DEFINITION( MessageType::err_unexpected_eof_at_string_parsing, MessageClass::Error, "I",
                    "File `" + *GET_ARG( 0 ) + "` unexpectedly ended while attempting to read a string.",
                    "string begins here" );
MESSAGE_DEFINITION( MessageType::err_lexer_char_not_allowed, MessageClass::Error, "I",
                    "Character `" + String( 1, GET_ARG( 0 ) ) + "`(" + to_string( static_cast<u32>( GET_ARG( 0 ) ) ) +
                        ") is not in allowed set of characters.",
                    "not allowed unit point`" + String( 1, GET_ARG( 0 ) ) + "`(" +
                        to_string( static_cast<u32>( GET_ARG( 0 ) ) ) + ")" );
MESSAGE_DEFINITION( MessageType::err_not_allowed_token_in_prelude, MessageClass::Error, "I",
                    "Token `" + GET_ARG( 0 ) + "` is not allowed at this position in a prelude file.",
                    "not allowed token `" + GET_ARG( 0 ) + "`" );
MESSAGE_DEFINITION( MessageType::err_parse_mci_rule, MessageClass::Error, "I", "Failed to parse MCI rule.",
                    "at this token" );
MESSAGE_DEFINITION( MessageType::err_unknown_mci, MessageClass::Error, "I", "Unknown MCI `" + GET_ARG( 0 ) + "`.", "" );
MESSAGE_DEFINITION( MessageType::err_feature_curr_not_supported, MessageClass::Error, "X",
                    "The feature `" + GET_ARG( 0 ) + "` is not suppported in this compiler version.", "" );
MESSAGE_DEFINITION( MessageType::err_parse_number, MessageClass::Error, "I", "Failed to parse number literal value.",
                    "" );

MESSAGE_DEFINITION( MessageType::err_unexpected_eof_after, MessageClass::Error, "C", "Unexpected end of file.", "Missing closing token to this token" );
MESSAGE_DEFINITION( MessageType::err_malformed_prelude_command, MessageClass::Error, "C",
                    "Malformed prelude command. Expected " + GET_ARG( 0 ) + ".", "" );
MESSAGE_DEFINITION( MessageType::err_expected_string, MessageClass::Error, "C", "Expected string.", "" );
MESSAGE_DEFINITION( MessageType::err_invalid_prelude, MessageClass::Error, "C",
                    "The given prelude name or path is invalid.", "" );
MESSAGE_DEFINITION( MessageType::err_term_with_multiple_expr, MessageClass::Error, "C",
                    "The term contains multiple expressions, but may only contain one.", "remove this part" );
MESSAGE_DEFINITION( MessageType::err_semicolon_without_meaning, MessageClass::Error, "C",
                    "The semicolon does not finish an expression", "remove it" );
MESSAGE_DEFINITION( MessageType::err_array_access_with_multiple_expr, MessageClass::Error, "C",
                    "An array access may only contain one expression", "" );


MESSAGE_DEFINITION( MessageType::test_message, MessageClass::Error, "X", "Test error message.", "message for this",
                    "global information text" );

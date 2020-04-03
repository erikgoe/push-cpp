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
    err_symbol_not_found,
    err_symbol_is_ambiguous,
    err_operator_symbol_not_found,
    err_operator_symbol_is_ambiguous,
    err_orphan_token,
    err_unfinished_expr,
    err_expected_symbol,
    err_expected_parametes,
    err_expected_assignment,
    err_expected_comma_list,
    err_expected_implication,
    err_expected_only_one_parameter,
    err_expected_function_head,
    err_expected_function_definition,
    err_method_not_allowed,
    err_public_not_allowed_in_context,
    err_member_in_invalid_scope,
    err_multiple_fn_definitions,
    err_var_not_living,
    err_local_variable_scoped,

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

MESSAGE_DEFINITION( MessageType::err_unexpected_eof_after, MessageClass::Error, "C", "Unexpected end of file.",
                    "Missing closing token to this token" );
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
MESSAGE_DEFINITION( MessageType::err_symbol_not_found, MessageClass::Error, "C", "Symbol not found", "" );
MESSAGE_DEFINITION( MessageType::err_symbol_is_ambiguous, MessageClass::Error, "C",
                    "The symbol identifier does not uniquely specify a symbol.", "", "Possible match defined here" );
MESSAGE_DEFINITION( MessageType::err_operator_symbol_not_found, MessageClass::Error, "C",
                    "Symbol '" + GET_ARG( 0 ) + "' for operator '" + GET_ARG( 1 ) + "' not found", "" );
MESSAGE_DEFINITION( MessageType::err_operator_symbol_is_ambiguous, MessageClass::Error, "C",
                    "The symbol identifier '" + GET_ARG( 0 ) + "' for operator '" + GET_ARG( 1 ) +
                        "' does not uniquely specify a symbol.",
                    "", "Possible match defined here" );
MESSAGE_DEFINITION( MessageType::err_orphan_token, MessageClass::Error, "C",
                    "Orphan token found! Please check the syntax of the sourrounding operations.",
                    "This token could not be merged into an expression" );
MESSAGE_DEFINITION( MessageType::err_unfinished_expr, MessageClass::Error, "C",
                    "Unfinished expression, please add a semicolon at the end", "" );
MESSAGE_DEFINITION( MessageType::err_expected_symbol, MessageClass::Error, "C", "Expected a symbol",
                    "replace this by a valid symbol please" );
MESSAGE_DEFINITION( MessageType::err_expected_parametes, MessageClass::Error, "C", "Expected parameters in parenthesis",
                    "sourround this with parenthesis please" );
MESSAGE_DEFINITION( MessageType::err_expected_assignment, MessageClass::Error, "C", "Expected an assignment",
                    "replace this by an assignment please" );
MESSAGE_DEFINITION( MessageType::err_expected_comma_list, MessageClass::Error, "C",
                    "Expected a list of comma-separated entries", "" );
MESSAGE_DEFINITION( MessageType::err_expected_implication, MessageClass::Error, "C",
                    "Expected an implication \"=>\" operator", "instead of this expression" );
MESSAGE_DEFINITION( MessageType::err_expected_only_one_parameter, MessageClass::Error, "C",
                    "Only one parameter allowed", "insert only one parameter here" );
MESSAGE_DEFINITION( MessageType::err_expected_function_head, MessageClass::Error, "C", "Expected a function head",
                    "instead of this expression" );
MESSAGE_DEFINITION( MessageType::err_expected_function_definition, MessageClass::Error, "C",
                    "Expected a function definition", "instead of this expression" );
MESSAGE_DEFINITION( MessageType::err_method_not_allowed, MessageClass::Error, "C", "Method not allowed",
                    "Methods are not allowed in this scope, please move it into an impl block." );
MESSAGE_DEFINITION( MessageType::err_public_not_allowed_in_context, MessageClass::Error, "C",
                    "A symbol may not be public in this context", "This symbol." );
MESSAGE_DEFINITION( MessageType::err_member_in_invalid_scope, MessageClass::Error, "C",
                    "Member defined in a invalid scope", "Remove the scope operator" );
MESSAGE_DEFINITION( MessageType::err_multiple_fn_definitions, MessageClass::Error, "C",
                    "Found multiple definitions of the same function", "first definition", "other definition" );
MESSAGE_DEFINITION( MessageType::err_var_not_living, MessageClass::Error, "C",
                    "Tried to access a variable outside of its lifetime", "in this expression" );
MESSAGE_DEFINITION( MessageType::err_local_variable_scoped, MessageClass::Error, "C",
                    "Local variable name with scope operator", "only simple identifiers allowed" );


MESSAGE_DEFINITION( MessageType::test_message, MessageClass::Error, "X", "Test error message.", "message for this",
                    "global information text" );

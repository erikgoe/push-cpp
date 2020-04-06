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
#include "libpushc/stdafx.h"
#include "libpushc/Expression.h"

// Splits a symbol string into a chain of strings. NOTE: This is only used for external inputs like the prelude
sptr<std::vector<SymbolIdentifier>> split_symbol_chain( const String &chained, String separator );

// Checks if a symbol identifier matches a symbol identifier pattern
bool symbol_identifier_matches( const SymbolIdentifier &pattern, const SymbolIdentifier &candidate );

// Searches for a sub-symbol by name and returns its id
std::vector<SymbolId> find_sub_symbol_by_identifier( CrateCtx &c_ctx, const SymbolIdentifier &identifier,
                                                     SymbolId parent );

// Searches for a sub-symbol by name chain and returns its id
std::vector<SymbolId> find_sub_symbol_by_identifier_chain( CrateCtx &c_ctx,
                                                           sptr<std::vector<SymbolIdentifier>> identifier_chain,
                                                           SymbolId parent = ROOT_SYMBOL );

// Returns a list of indices of members which match the identifier
std::vector<size_t> find_member_symbol_by_identifier( CrateCtx &c_ctx, const SymbolIdentifier &identifier,
                                                      SymbolId parent_symbol );

// Returns only the head of a symbol
String get_local_symbol_name( CrateCtx &c_ctx, SymbolId symbol );

// Returns the full symbol path for a symbol
String get_full_symbol_name( CrateCtx &c_ctx, SymbolId symbol );

// Applies local alias rules to a name chain. Returns false on error
bool alias_name_chain( CrateCtx &c_ctx, std::vector<SymbolIdentifier> &symbol_chain );

// Creates a new symbol from a global name. @param name may not contain scope operators
SymbolId create_new_global_symbol( CrateCtx &c_ctx, const String &name );

// Creates a new symbol from a relative name. @param name may not contain scope operators
SymbolId create_new_relative_symbol( CrateCtx &c_ctx, const String &name, SymbolId parent_symbol );

// Creates a new symbol from a local name. @param name may not contain scope operators
SymbolId create_new_local_symbol( CrateCtx &c_ctx, const String &name );

// Creates a new global symbol from a symbol chain. Existing symbols are skipped
SymbolId create_new_global_symbol_from_name_chain( CrateCtx &c_ctx,
                                                   const sptr<std::vector<SymbolIdentifier>> symbol_chain );

// Creates a new relative symbol from a symbol chain. Existing symbols are skipped
SymbolId create_new_relative_symbol_from_name_chain( CrateCtx &c_ctx,
                                                     const sptr<std::vector<SymbolIdentifier>> symbol_chain,
                                                     SymbolId parent_symbol );

// Creates a new local symbol from a symbol chain. Existing symbols are skipped
SymbolId create_new_local_symbol_from_name_chain( CrateCtx &c_ctx,
                                                  const sptr<std::vector<SymbolIdentifier>> symbol_chain );

// Creates a new symbol for a member (attribute, method)
SymbolGraphNode &create_new_member_symbol( CrateCtx &c_ctx, const SymbolIdentifier &symbol_identifier,
                                           SymbolId parent_symbol );

// Creates a new type with no symbol
TypeId create_new_internal_type( CrateCtx &c_ctx );

// Creates a new type from a existing symbol
TypeId create_new_type( CrateCtx &c_ctx, SymbolId from_symbol );

// Changes the current scope symbol to be @param new_scope
void switch_scope_to_symbol( CrateCtx &c_ctx, SymbolId new_scope );

// Sets the current scope symbol to its parent scope
void pop_scope( CrateCtx &c_ctx );

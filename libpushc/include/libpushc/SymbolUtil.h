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
#include "libpushc/Ast.h"

// Splits a symbol string into a chain of strings
sptr<std::vector<String>> split_symbol_chain( const String &chained, String separator );

// Searches for a sub-symbol by name and returns its id. Returns 0 if no such sub-symbol exits
SymbolId find_sub_symbol_by_name( CrateCtx &c_ctx, const String &name, SymbolId parent );

// Searches for a sub-symbol by name chain and returns its id. Returns 0 if no such sub-symbol exits
SymbolId find_sub_symbol_by_name_chain( CrateCtx &c_ctx, sptr<std::vector<String>> name_chain,
                                        SymbolId parent = ROOT_SYMBOL );

// Returns the full symbol path for a symbol
String get_full_symbol_name( CrateCtx &c_ctx, SymbolId symbol );

// Creates a new symbol from a global name. @param name may not contain scope operators
SymbolId create_new_global_symbol( CrateCtx &c_ctx, const String &name );

// Creates a new symbol from a local name. @param name may not contain scope operators
SymbolId create_new_relative_symbol( CrateCtx &c_ctx, const String &name, SymbolId parent_symbol );

// Creates a new symbol from a local name. @param name may not contain scope operators
SymbolId create_new_local_symbol( CrateCtx &c_ctx, const String &name );

// Creates a new global symbol from a symbol chain. Existing symbols are skipped
SymbolId create_new_global_symbol_from_name_chain( CrateCtx &c_ctx, const sptr<std::vector<String>> symbol_chain );

// Creates a new type from a existing symbol
TypeId create_new_type( CrateCtx &c_ctx, SymbolId from_symbol );
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
#include "libpushc/CrateCtx.h"

// Create the MIR of the current compilation unit
void get_mir( JobsBuilder &jb, UnitCtx &parent_ctx );

// FOLLOWING FUNCTIONS ARE NOT QUERIES

// Creates a new mir operation and does some checks. @param result = 0 will create a new variable
MirEntryId create_operation( CrateCtx &c_ctx, Worker &w_ctx, FunctionImplId function, AstNode &original_expr,
                             MirEntry::Type type, MirVarId result, std::vector<MirVarId> parameters );

// Creates a MIR function call from a symbol id. Handles dangling varameters etc. See crate_operation.
MirEntryId create_call( CrateCtx &c_ctx, Worker &w_ctx, FunctionImplId calling_function, AstNode &original_expr,
                        std::vector<SymbolId> called_function_candidates, MirVarId result,
                        std::vector<MirVarId> parameters );

// Creates a new local variable and returns its id
MirVarId create_variable( CrateCtx &c_ctx, Worker &w_ctx, FunctionImplId function, AstNode *original_expr,
                          const String &name = "" );

// Destroys a local variable in a function
void drop_variable( CrateCtx &c_ctx, Worker &w_ctx, FunctionImplId function, AstNode &original_expr,
                    MirVarId variable );

// Call this e. g. when a variable is moved
void remove_from_local_living_vars( CrateCtx &c_ctx, Worker &w_ctx, FunctionImplId function, AstNode &original_expr,
                                    MirVarId variable );

// Analyses the function signature and updates the type if necessary
void analyse_function_signature( CrateCtx &c_ctx, Worker &w_ctx, SymbolId function );

// Decide which function overloading to call
bool infer_function_call( CrateCtx &c_ctx, Worker &w_ctx, FunctionImplId function, MirEntry &call_op,
                          std::vector<MirVarId> infer_stack = std::vector<MirVarId>() );

// Checks and infers types of variables inside a function. Returns false on error
bool infer_type( CrateCtx &c_ctx, Worker &w_ctx, FunctionImplId function, MirVarId var,
                 std::vector<MirVarId> infer_stack = std::vector<MirVarId>() );

// Finally selects a type for a variable with a minimal bounds. Requires that infer_type has already be called on the variable
bool enforce_type_of_variable( CrateCtx &c_ctx, Worker &w_ctx, FunctionImplId function, MirVarId var );

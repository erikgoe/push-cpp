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
                             MirEntry::Type type, MirVarId result, ParamContainer parameters );

// Creates a MIR function call from a symbol id. Handles dangling varameters etc. See crate_operation.
MirEntryId create_call( CrateCtx &c_ctx, Worker &w_ctx, FunctionImplId calling_function, AstNode &original_expr,
                        MirVarId symbol_var, MirVarId result, ParamContainer parameters );

// Creates a new local variable and returns its id
MirVarId create_variable( CrateCtx &c_ctx, Worker &w_ctx, FunctionImplId function, AstNode *original_expr,
                          const String &name = "" );

// Removes one or multiple variables from the context
void purge_variable( CrateCtx &c_ctx, Worker &w_ctx, FunctionImplId function, AstNode &original_expr,
                     std::vector<MirVarId> variables );

// Analyses the function signature and updates the type if necessary
void analyse_function_signature( CrateCtx &c_ctx, Worker &w_ctx, SymbolId function );

// Creates a function from a FuncExpr specified by @param symbolId
void generate_mir_function_impl( CrateCtx &c_ctx, Worker &w_ctx, SymbolId symbol_id );

// Resolves the mir operations of a function and initiates type inference
bool infer_operations( CrateCtx &c_ctx, Worker &w_ctx, FunctionImplId function );

// Resolves the dropping of variables
bool resolve_drops( CrateCtx &c_ctx, Worker &w_ctx, FunctionImplId function );

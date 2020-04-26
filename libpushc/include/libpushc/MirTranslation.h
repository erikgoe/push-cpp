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

// Create the MIR of the current compilation unit
void get_mir( JobsBuilder &jb, UnitCtx &parent_ctx );

// FOLLOWING FUNCTIONS ARE NOT QUERIES

// Creates a new mir operation and does some checks. @param result = 0 will create a new variable
MirEntry &create_operation( CrateCtx &c_ctx, Worker &w_ctx, FunctionImplId function, AstNode &original_expr,
                            MirEntry::Type type, MirVarId result, std::vector<MirVarId> parameters );

// Creates a MIR function call from a symbol id. Handles dangling varameters etc. See crate_operation.
MirEntry &create_call( CrateCtx &c_ctx, Worker &w_ctx, FunctionImplId calling_function, AstNode &original_expr,
                       SymbolId called_function, MirVarId result, std::vector<MirVarId> parameters );

// Creates a new local variable and returns its id
MirVarId create_variable( CrateCtx &c_ctx, Worker &w_ctx, FunctionImplId function, const String &name = "" );

// Destroys a local variable in a function
void drop_variable( CrateCtx &c_ctx, Worker &w_ctx, FunctionImplId function, AstNode &original_expr,
                    MirVarId variable );

// Analyses the function signature and updates the type if necessary
void analyse_function_signature( CrateCtx &c_ctx, Worker &w_ctx, SymbolId function);

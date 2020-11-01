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

// Decide which function overloading to call
bool infer_function_call( CrateCtx &c_ctx, Worker &w_ctx, FunctionImplId function, MirEntry &call_op,
                          std::vector<MirVarId> infer_stack = std::vector<MirVarId>() );

// Checks and infers types of variables inside a function. Returns false on error
bool infer_type( CrateCtx &c_ctx, Worker &w_ctx, FunctionImplId function, MirVarId var,
                 std::vector<MirVarId> infer_stack = std::vector<MirVarId>() );

// Chooses the final type of possible candidates, returns 0 when no selection can be done
TypeId choose_final_type( CrateCtx &c_ctx, Worker &w_ctx, std::vector<TypeId> types );

// Finally selects a type for a variable with a minimal bounds. Requires that infer_type has already be called on the
// variable
bool enforce_type_of_variable( CrateCtx &c_ctx, Worker &w_ctx, FunctionImplId function, MirVarId var );

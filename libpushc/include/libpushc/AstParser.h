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

// Create the Abstract Syntax tree of the current compilation unit
void get_ast( JobsBuilder &jb, UnitCtx &parent_ctx );

// Parse the AST from an input file (of this compilation unit)
void parse_ast( JobsBuilder &jb, UnitCtx &parent_ctx );

// NOT A QUERY. Translates the prelude syntax rules into ast syntax rules
void load_syntax_rules( Worker &w_ctx, CrateCtx &c_ctx );

// NOT A QUERY. Parses a scope
AstNode parse_scope( sptr<SourceInput> &input, Worker &w_ctx, CrateCtx &c_ctx, Token::Type end_token,
                        Token *last_token );

// NOT A QUERY. Loads basic types like int, string, etc
void load_base_types( CrateCtx &c_ctx, Worker &w_ctx, PreludeConfig &cfg );

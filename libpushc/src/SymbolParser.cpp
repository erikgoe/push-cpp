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

#include "libpushc/stdafx.h"
#include "libpushc/SymbolParser.h"
#include "libpushc/SymbolUtil.h"

void parse_symbols( sptr<CrateCtx> c_ctx, JobsBuilder &jb, UnitCtx &parent_ctx ) {
    jb.add_job<void>( [c_ctx]( Worker &w_ctx ) {
        bool successful = true;
        if ( successful )
            successful = c_ctx->ast->visit( *c_ctx, w_ctx, VisitorPassType::BASIC_SEMANTIC_CHECK, c_ctx->ast );
        if ( successful )
            successful = c_ctx->ast->visit( *c_ctx, w_ctx, VisitorPassType::SYMBOL_DISCOVERY, c_ctx->ast );
    } );
}

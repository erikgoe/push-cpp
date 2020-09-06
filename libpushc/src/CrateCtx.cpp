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
#include "libpushc/CrateCtx.h"
#include "libpushc/Expression.h"
#include "libpushc/SymbolUtil.h"

bool SyntaxRule::matches_reversed( std::vector<AstNode> &rev_list ) {
    if ( rev_list.size() < expr_list.size() )
        return false;

    for ( int i = 0; i < expr_list.size(); i++ ) {
        if ( !rev_list[i].matches( expr_list[expr_list.size() - i - 1] ) ) {
            return false;
        }
    }
    return true;
}

bool SymbolIdentifier::ParamSig::operator==( const ParamSig &other ) const {
    if ( name != other.name || ref != other.ref || mut != other.mut )
        return false;
    if ( tmp_type_symbol != nullptr || other.tmp_type_symbol != nullptr )
        return false;
    return type == 0 || other.type == 0 ||
           type == other.type; // type == 0 means the type of this is not known, so it's a candidate
}

CrateCtx::CrateCtx() {
    ast = make_shared<AstNode>();
    symbol_graph.resize( 2 );
    type_table.resize( LAST_FIX_TYPE + 1 );
    functions.resize( 1 );
}

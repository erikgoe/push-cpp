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
    if ( template_type_index != 0 && other.template_type_index != 0 &&
         template_type_index != other.template_type_index )
        return false;
    return type == 0 || other.type == 0 ||
           type == other.type; // type == 0 means the type of this is not known, so it's a candidate
}

void TypeSelection::add_requirement( const TypeSelection &type ) {
    if ( type.is_final() ) {
        if ( type.final_type == final_type )
            return;
        assert( final_type == 0 );
        type_requirements.push_back( type.final_type );
    } else {
        if ( type.get_requirements().size() == 1 && type.get_requirements().front() == final_type )
            return;
        assert( final_type == 0 );
        add_requirements( type.get_requirements() );
    }
}

CrateCtx::CrateCtx() {
    ast = make_shared<AstNode>();
    symbol_graph.resize( 2 );
    type_table.resize( LAST_FIX_TYPE + 1 );
    functions.resize( 1 );
}

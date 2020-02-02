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
#include "libpushc/Ast.h"

bool SyntaxRule::matches_reversed( std::vector<sptr<Expr>> &rev_list ) {
    if ( rev_list.size() < expr_list.size() )
        return false;

    for ( int i = 0; i < expr_list.size(); i++ ) {
        if ( !expr_list[expr_list.size() - i - 1]->matches( rev_list[i] ) ) {
            return false;
        }
    }
    return true;
}

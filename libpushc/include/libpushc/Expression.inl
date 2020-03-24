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

template <typename T>
void pre_visit_impl( CrateCtx &c_ctx, Worker &w_ctx, VisitorPassType vpt, T &expr ) {
    if ( vpt == VisitorPassType::SYMBOL_DISCOVERY ) {
        expr.pre_symbol_discovery( c_ctx, w_ctx );
    }
}

template <typename T>
bool visit_impl( CrateCtx &c_ctx, Worker &w_ctx, VisitorPassType vpt, T &expr ) {
    if ( vpt == VisitorPassType::BASIC_SEMANTIC_CHECK ) {
        if ( !expr.primitive_semantic_check( c_ctx, w_ctx ) )
            return false;
    } else if ( vpt == VisitorPassType::SYMBOL_DISCOVERY ) {
        expr.symbol_discovery( c_ctx, w_ctx );
    }

    bool result = true;
    for ( auto &ss : expr.static_statements ) {
        if ( !ss->visit( c_ctx, w_ctx, vpt ) )
            result = false;
    }
    return result;
}
